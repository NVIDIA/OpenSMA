/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nv/logger/task.h"

#include <cstring>

#include "nv/bootloader.h"
#include "nv/debugtoken/debugtoken.h"
#include "nv/flash/flash.h"
#include "nv/ipc/supervisor.h"
#include "corepdk/platforms/mcxn236/pldm-fd/src/pldm_wrap.h"
#include "nv/watchdog/runtime.h"
#include "sys/flash/flash_config.h"
#include "nv/fw_parser/fw_parser_mcu.h"
#include NV_IPC_CONFIG_H

using namespace nv::logger;
using namespace nv;
using namespace mctp;
using namespace sys::flash::config;

Task::Task() noexcept
: nv::ipc::Task(nv::ipc::TaskId::Logger, "Logger")
, _event(nv::ipc::Event::make(nv::ipc::EventId::LogEvent))
, _request(nv::ipc::Queue::make(nv::ipc::QueueId::LogRequest))
, _response(nv::ipc::Queue::make(nv::ipc::QueueId::LogResponseBlocking))
, download_queue(nv::ipc::Queue::make(nv::ipc::QueueId::LogDownload))
, download_resp_queue(nv::ipc::Queue::make(nv::ipc::QueueId::LogDownloadResp))
, isr_queue(nv::ipc::Queue::make(nv::ipc::QueueId::LogISR))
, _uart_driver(static_cast<nv::uart::Port>(nv::ipc::UartInstance))
{
    // Assert the queue item_size is correct in config.h
    static_assert(sizeof(Item)
                      == std::get<2>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::LogRequest)]),
                  "LogRequest size is mismatch");
    static_assert(
        sizeof(Item)
            == std::get<2>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::LogResponseBlocking)]),
        "LogResponseBlocking size is mismatch");
    static_assert(sizeof(Item)
                      == std::get<2>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::LogISR)]),
                  "LogISR size is mismatch");
    static_assert(sizeof(Dlreq)
                      == std::get<2>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::LogDownload)]),
                  "LogDownload size is mismatch");
    static_assert(
        sizeof(Dlreq)
            == std::get<2>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::LogDownloadResp)]),
        "LogDownloadResp size is mismatch");

    dump_buffer.at(0)  = DumpHeadMagic;
    dump_buffer.back() = DumpTailMagic;
}

void Task::make()
{
    NV_TASK_DATA static Task task;
    constexpr auto           StackSize = std::max(736, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Logger, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

Status
Task::request(const Item& item, Item& resp_item, bool wait, nv::ipc::Queue::Usecs timeout)
{
    const bool is_in_isr = sys::ipc::is_in_isr();

    if (is_in_isr) {
        timeout = nv::ipc::Queue::Usecs(0);
        wait    = false;
    }

    auto queue_id     = ipc::QueueId::LogRequest;
    auto request_item = ipc::Queue::Item(std::bit_cast<uint8_t*>(&item), sizeof(item));
    ipc::Queue::Status request_status{};
    if (is_in_isr) {
        request_status = ipc::Queue::make(queue_id).send_isr(request_item);
    }
    else {
        request_status = ipc::Queue::make(queue_id).send(request_item, timeout);
    }

    if (request_status != ipc::Queue::Status::Ok) {
        if ((nv::common::to_underlying(item.direction)
             & nv::common::to_underlying(OutputDirection::Flash))
            != nv::common::to_underlying(OutputDirection::None)) {
            // Only record event is dropped when it is required to write to flash
            auto event      = ipc::Event::make(ipc::EventId::LogEvent);
            auto set_status = event.set(EventBits::DropLogEvent);
            if (set_status != ipc::Event::Status::Ok) {
                nv::info("Set event 0x%x failed\n", EventBits::DropLogEvent);
            }
        }
        return nv::logger::Status::Timeout;
    }

    auto           event          = ipc::Event::make(ipc::EventId::LogEvent);
    const uint32_t EventBitsToSet = EventBits::AddLogEvent;
    auto           set_status     = event.set(EventBitsToSet);

    if (set_status != ipc::Event::Status::Ok) {
        return nv::logger::Status::Error;
    }

    if (wait) {
        auto response_item   = ipc::Queue::Item(std::bit_cast<uint8_t*>(&resp_item),
                                              sizeof(Item));
        auto response_status = ipc::Queue::make(ipc::QueueId::LogResponseBlocking)
                                   .recv(response_item, timeout);
        if (response_status != ipc::Queue::Status::Ok) {
            return nv::logger::Status::Timeout;
        }
        return resp_item.status;
    }

    return nv::logger::Status::Ok;
}

void Task::request(const AsciiArr& ascii_arr)
{
    auto               queue_id     = ipc::QueueId::LogRequest;
    auto               request_item = ipc::Queue::Item(std::bit_cast<uint8_t*>(&ascii_arr),
                                         sizeof(ascii_arr));
    ipc::Queue::Status request_status{};
    constexpr auto     DumpRawStrTimeout = 5000;
    request_status                       = ipc::Queue::make(queue_id).send(request_item,
                                                     nv::ipc::Queue::Usecs(DumpRawStrTimeout));
    if (request_status != ipc::Queue::Status::Ok) {
        return;
    }

    auto           event          = ipc::Event::make(ipc::EventId::LogEvent);
    const uint32_t EventBitsToSet = EventBits::AddLogEvent;
    (void)event.set(EventBitsToSet);
}

Status Task::request_from_isr(const Item& request)
{
    auto isr_queue = ipc::Queue::make(ipc::QueueId::LogISR);
    // TBD: logic from glacier, need check why
    nv::ipc::Queue::Status status{};
    auto request_item = ipc::Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    status            = isr_queue.send_isr(request_item);

    if (status != nv::ipc::Queue::Status::Ok) {
        nv::info("ISR queue status:%d\n", status);
        return nv::logger::Status::Error;
    }

    // Event from ISR
    auto event      = ipc::Event::make(ipc::EventId::LogEvent);
    auto set_status = event.set(EventBits::AddLogISREvent);
    if (set_status != nv::ipc::Event::Status::Ok) {
        return nv::logger::Status::Error;
    }

    return nv::logger::Status::Ok;
}

Status Task::download(Dlreq& dl_req, nv::ipc::Queue::Usecs timeout)
{
    auto queue_id       = ipc::QueueId::LogDownload;
    auto request_item   = ipc::Queue::Item(std::bit_cast<uint8_t*>(&dl_req), sizeof(dl_req));
    auto request_status = ipc::Queue::make(queue_id).send(request_item, timeout);
    if (request_status != ipc::Queue::Status::Ok) {
        return nv::logger::Status::Timeout;
    }

    auto event      = ipc::Event::make(ipc::EventId::LogEvent);
    auto set_status = event.set(EventBits::DownloadLogEvent);
    if (set_status != nv::ipc::Event::Status::Ok) {
        return nv::logger::Status::Error;
    }

    auto response_item   = ipc::Queue::Item(std::bit_cast<uint8_t*>(&dl_req), sizeof(dl_req));
    auto response_status = ipc::Queue::make(ipc::QueueId::LogDownloadResp)
                               .recv(response_item, timeout);
    if (response_status != ipc::Queue::Status::Ok) {
        return nv::logger::Status::Timeout;
    }
    return dl_req.status;
}

Status Task::clean()
{
    auto event      = ipc::Event::make(ipc::EventId::LogEvent);
    auto set_status = event.set(EventBits::CleanLogEvent);
    if (set_status != nv::ipc::Event::Status::Ok) {
        return nv::logger::Status::Error;
    }

    return nv::logger::Status::Ok;
}

nv::ipc::Queue::Status Task::receive_request_from_queue(const Item& item_req)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    auto& req_queue    = _request;
    auto  request_item = Queue::Item(std::bit_cast<uint8_t*>(&item_req), sizeof(item_req));
    /// FixMe to avoid race condition, we need to peek the item first and remove it
    /// after processing
    auto status = req_queue.recv(request_item, 0s);
    return status;
}

nv::ipc::Queue::Status Task::send_response_to_queue(Item& item_resp)
{
    using namespace nv::ipc;
    auto response_item   = Queue::Item(std::bit_cast<uint8_t*>(&item_resp), sizeof(item_resp));
    auto response_status = _response.send(response_item);
    return response_status;
}

nv::ipc::Queue::Status Task::receive_request_from_queue(const Dlreq& dl_req)
{
    using namespace nv::ipc;
    auto request_item = Queue::Item(std::bit_cast<uint8_t*>(&dl_req), sizeof(dl_req));
    /// FixMe to avoid race condition, we need to peek the item first and remove it
    /// after processing
    auto status = download_queue.recv(request_item);
    return status;
}
nv::ipc::Queue::Status Task::send_response_to_queue(Dlreq& dl_req)
{
    using namespace nv::ipc;
    auto response_item   = Queue::Item(std::bit_cast<uint8_t*>(&dl_req), sizeof(dl_req));
    auto response_status = download_resp_queue.send(response_item);
    return response_status;
}

nv::logger::Status Task::dump_uart(const Item& request)
{
    const nv::logger::Entry LogEntry{
        .timestamp     = request.timestamp,
        .level         = request.level,
        .footprint_msb = request.footprint_msb,
        .event         = request.event,
        .footprint_lsb = static_cast<uint8_t>(request.footprint_lsb & 0x7Fu),
        .coreId        = static_cast<uint8_t>(request.core_id & 0x01u),
        .data          = request.data};

    static_assert(sizeof(dump_buffer) >= sizeof(nv::logger::Entry) + DumpCheckSize,
                  "Log dump buffer size invalid");

    std::memcpy(dump_buffer.data() + 1, LogEntry.to_span().data(), sizeof(LogEntry));
    auto status = _uart_driver.tx(dump_buffer);
    if (status != nv::uart::Status::Ok) {
        return nv::logger::Status::Error;
    }
    return nv::logger::Status::Ok;
}

void Task::dump_ascii(const AsciiArr& ascii_arr)
{
    constexpr uint32_t                            AsciiLen = AsciiStrLen + 1;  // 1 for core_id
    std::array<uint8_t, DumpCheckSize + AsciiLen> dump_buffer_raw{};
    dump_buffer_raw[0]     = DumpHeadMagicRaw;
    dump_buffer_raw.back() = DumpTailMagicRaw;
    std::memcpy(
        dump_buffer_raw.data() + 1, ascii_arr.ascii_arr.data(), ascii_arr.ascii_arr.size() + 1);
    _uart_driver.tx(dump_buffer_raw);
}

uint32_t array_to_u32(std::array<uint8_t, 4>& buffer)
{
    uint32_t result = 0;
    memcpy(&result, buffer.data(), sizeof(result));
    return result;
}

bool Task::log_signing_class()
{
    using namespace nv::ipc;
    EventData data{};

    auto key_version_parse_result = nv::fw_parser::mcu::get_image_signing_key_version(
        nv::fw_parser::mcu::ParsingFwType::ActiveSlot);
    if (!key_version_parse_result.has_value()) {
        return false;
    }
    // for backward compatibility, the key version is limited to 255
    if (*key_version_parse_result > std::numeric_limits<uint8_t>::max()) {
        return false;
    }
    data[0] = *key_version_parse_result;

    const Item SigningLog{
        .event = Event::SigningClass.unique_id, .level = Level::Info, .data = data};

    Item signing_resp{};
    dump_uart(SigningLog);
    _logger.store(SigningLog, signing_resp);

    return true;
}

bool Task::log_debug_token()
{
    using namespace nv::ipc;
    EventData  data{};
    const auto isTokenInstalled = debugtoken::is_dbg_token_tlv_in_flash() == true ? 1 : 0;

    data[0] = isTokenInstalled;

    const Item DebugtokenLog{
        .event = Event::DebugTokenInstall.unique_id, .level = Level::Info, .data = data};

    Item debugtoken_resp{};
    dump_uart(DebugtokenLog);
    _logger.store(DebugtokenLog, debugtoken_resp);

    return true;
}

void Task::start()
{
    using namespace nv::ipc;
    _logger.init();

    if (_logger.need_handle_error()) {
        _logger.handle_error();
    }

    const FwVersion Ver = nv::logger::get_fw_version();
    EventData       data{};
    memcpy(data.data(), Ver.to_span().data(), sizeof(FwVersion));
    const Item VersionLog{
        .event = Event::LoggerStart.unique_id, .level = Level::Info, .data = data};
    Item version_resp{};
    dump_uart(VersionLog);
    _logger.store(VersionLog, version_resp);

    uint8_t boot_src = nv::bootloader::Driver::get_boot_src();
    memcpy(data.data(), &boot_src, sizeof(boot_src));
    const Item BootSrcLog{
        .event = Event::BootSource.unique_id, .level = Level::Info, .data = data};
    Item boot_src_resp{};
    dump_uart(BootSrcLog);
    _logger.store(BootSrcLog, boot_src_resp);

    // log signing class
    bool status = log_signing_class();
    if (!status) {
        nv::info("log_signing_class failed!\n");
    }

    // log debug token
    status = log_debug_token();
    if (!status) {
        nv::info("log_debug_token failed!\n");
    }

    if (_logger.need_handle_error()) {
        _logger.handle_error();
    }

    auto wait_bits = EventBits::AddLogEvent | EventBits::DownloadLogEvent
                   | EventBits::AddLogBlockingEvent | EventBits::CleanLogEvent
                   | EventBits::DropLogEvent | EventBits::AddLogISREvent
                   | EventBits::WdtEventEvent;
    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Logger);

    while (true) {
        auto bits       = _event.wait(wait_bits, false, false);
        auto event_bits = bits.value();

        if (event_bits & EventBits::WdtEventEvent) {
            nv::watchdog::Runtime::mark_task_alive(nv::watchdog::TaskMonitorIndex::Logger);
            _event.clear(EventBits::WdtEventEvent);
        }

        if (event_bits & EventBits::AddLogEvent) {
            // nv::info("EventBits::AddLogEvent\n");
            auto event_status = _event.clear(EventBits::AddLogEvent);
            if (event_status != ipc::Event::Status::Ok) {
                nv::info("Clear event 0x%x failed\n", EventBits::AddLogEvent);
            }
            const Item ReqItem{};
            Item       resp_item{};
            auto       status = receive_request_from_queue(ReqItem);

            if (status != nv::ipc::Queue::Status::Ok) {
                continue;
            }

            if (ReqItem.flag) {
                dump_ascii(std::bit_cast<AsciiArr>(ReqItem));
            }
            else {
                if ((nv::common::to_underlying(ReqItem.direction)
                     & nv::common::to_underlying(OutputDirection::Console))
                    != std::to_underlying(OutputDirection::None)) {
                    dump_uart(ReqItem);
                }

                if ((nv::common::to_underlying(ReqItem.direction)
                     & nv::common::to_underlying(OutputDirection::Flash))
                    != std::to_underlying(OutputDirection::None)) {
                    _logger.store(ReqItem, resp_item);
                }

                if (ReqItem.wait) {
                    auto response_status = send_response_to_queue(resp_item);
                    if (response_status != Queue::Status::Ok) {
                        nv::info("response fail - %d\n", response_status);
                    }
                }
            }

            if (_request.size() > 0) {
                auto set_status = _event.set(EventBits::AddLogEvent);
                if (set_status != nv::ipc::Event::Status::Ok) {
                    nv::info("Set event 0x%x failed\n", EventBits::AddLogEvent);
                }
            }
        }
        else if (event_bits & EventBits::DownloadLogEvent) {
            auto event_status = _event.clear(EventBits::DownloadLogEvent);
            if (event_status != ipc::Event::Status::Ok) {
                nv::info("Clear event 0x%x failed\n", EventBits::DownloadLogEvent);
            }
            const Dlreq DlReqItem{};
            Dlreq       resp_dl_req_item{};
            auto        status = receive_request_from_queue(DlReqItem);
            if (status != nv::ipc::Queue::Status::Ok) {
                continue;
            }

            _logger.process_download(DlReqItem, resp_dl_req_item);

            auto response_status = send_response_to_queue(resp_dl_req_item);
            if (response_status != Queue::Status::Ok) {
                nv::info("response fail - %d\n", response_status);
            }

            // nv::info("ReqItem.event:0x%x  ReqItem.level:0x%x\n", ReqItem.event,
            // ReqItem.level);
        }
        else if (event_bits & EventBits::CleanLogEvent) {
            auto event_status = _event.clear(EventBits::CleanLogEvent);
            if (event_status != ipc::Event::Status::Ok) {
                nv::info("Clear event 0x%x failed\n", EventBits::CleanLogEvent);
            }
            _logger.clean();
        }
        else if (event_bits & EventBits::DropLogEvent) {
            nv::info("EventBits::DropLogEvent\n");
            auto event_status = _event.clear(EventBits::DropLogEvent);
            if (event_status != ipc::Event::Status::Ok) {
                nv::info("Clear event 0x%x failed\n", EventBits::DropLogEvent);
            }
            const Item ReqItem{.timestamp = nv::ipc::Supervisor::get_os_ticks(),
                               .event     = Event::LoggerLogDrop.unique_id,
                               .level     = Level::Info};
            Item       resp_item{};
            _logger.store(ReqItem, resp_item);
        }

        if (event_bits & EventBits::AddLogISREvent) {
            auto event_status = _event.clear(EventBits::AddLogISREvent);
            if (event_status != ipc::Event::Status::Ok) {
                nv::info("Clear event 0x%x failed\n", EventBits::AddLogISREvent);
            }
            const Item ReqItem{};
            Item       resp_item{};
            if (isr_queue.size() == 0) {
                nv::info("ISR queue is empty\n");
                continue;
            }
            auto request_item = Queue::Item(std::bit_cast<uint8_t*>(&ReqItem), sizeof(ReqItem));
            auto status       = isr_queue.recv(request_item);
            if (status != nv::ipc::Queue::Status::Ok) {
                nv::info("Recv from ISR queue failed\n");
                continue;
            }
            if ((nv::common::to_underlying(ReqItem.direction)
                 & nv::common::to_underlying(OutputDirection::Console))
                != std::to_underlying(OutputDirection::None)) {
                dump_uart(ReqItem);
            }

            if ((nv::common::to_underlying(ReqItem.direction)
                 & nv::common::to_underlying(OutputDirection::Flash))
                != std::to_underlying(OutputDirection::None)) {
                _logger.store(ReqItem, resp_item);
            }

            if (resp_item.status != nv::logger::Status::Ok) {
                nv::info("Adding log from ISR failed\n");
            }

            if (isr_queue.size() > 0) {
                auto set_status = _event.set(EventBits::AddLogISREvent);
                if (set_status != nv::ipc::Event::Status::Ok) {
                    nv::info("Set event 0x%x failed\n", EventBits::AddLogEvent);
                }
            }
        }
        if (_logger.need_handle_error()) {
            _logger.handle_error();
        }
    }
}

void Task::wdt_notify()
{
    auto event = nv::ipc::Event::make(nv::ipc::EventId::LogEvent);
    auto sts   = event.set(EventBits::WdtEventEvent);
    if (sts != nv::ipc::Event::Status::Ok) {
        // TBD
    }
}
