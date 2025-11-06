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
#include "nv/flash/task.h"

#include <chrono>
#include <cstring>

#include "nv/bootloader.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "nv/watchdog/runtime.h"
#include "nv/spdm/task.h"
#include "nv/ipc/driver.h"

using namespace nv::flash;
using namespace std::chrono_literals;

Task::Task() noexcept
: nv::ipc::Task(nv::ipc::TaskId::Flash, "Flash")
, _event(nv::ipc::Event::make(nv::ipc::EventId::FlashEvent))
, _request(nv::ipc::Queue::make(nv::ipc::QueueId::FlashRequest))
, _response(nv::ipc::Queue::make(nv::ipc::QueueId::FlashResponse))
{
    // Assert the queue item_size is correct in config.h
    static_assert(sizeof(Request)
                      == std::get<2>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::FlashRequest)]),
                  "FlashRequest size is mismatch");
    static_assert(sizeof(Response)
                      == std::get<2>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::FlashResponse)]),
                  "FlashResponse size is mismatch");

    // Dual core project will need FlashRequest max_items = 2
    // 1 for Core0, 1 for Core1
    static_assert((!nv::ipc::EnableDualCore)
                      || std::get<1>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::FlashRequest)])
                             == DualCoreFlashRequestQueueMaxItems,
                  "FlashRequest queue max_items is mismatch");

    auto status = semaphore_give();
    if (status != nv::ipc::Queue::Status::Ok) {
        nv::info("Flash semaphore init failed\n");
    }
    // Only called semaphore_give for Core1 if it is dual core project
    if constexpr (nv::ipc::EnableDualCore) {
        status = semaphore_give(FlashTimeout, nv::ipc::CoreId::Core1);
        if (status != nv::ipc::Queue::Status::Ok) {
            nv::info("Flash semaphore core1 init failed\n");
        }
    }

    _driver.init(sys::flash::Driver::ApiSelect::Flash);
    _driver.init_efuse();

    _pds.init(_driver);
    cfpa_write_page = _driver.get_cfpa_write_page();
}

void Task::make()
{
    NV_TASK_DATA static Task task;
    constexpr auto           StackSize = std::max(2752, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Norm, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

//@WAR: GFWLYNT1-213
nv::ipc::Queue::Status Task::semaphore_take(nv::ipc::Queue::Usecs timeout)
{
    using namespace nv::ipc;
    auto          sema = Queue::make(QueueId::FlashSema);
    const uint8_t ValGet{};
    auto          resp_item = Queue::Item(std::bit_cast<uint8_t*>(&ValGet), sizeof(ValGet));
    auto          status    = sema.recv(resp_item, timeout);
    return status;
}

//@WAR: GFWLYNT1-213
nv::ipc::Queue::Status Task::semaphore_give(nv::ipc::Queue::Usecs timeout,
                                            nv::ipc::CoreId       dest_core)
{
    using namespace nv::ipc;
    auto          sema    = Queue::make(QueueId::FlashSema);
    const uint8_t ValSend = 0;
    auto request_item     = Queue::Item(std::bit_cast<uint8_t*>(&ValSend), sizeof(ValSend));
    auto status           = sema.send(request_item, timeout, dest_core);
    return status;
}

nv::flash::Status
Task::request(const Request& request, Response& response, nv::ipc::Queue::Usecs timeout)
{
    using namespace nv;

    if (semaphore_take(timeout) != ipc::Queue::Status::Ok) {
        return flash::Status::Timeout;
    }

    /// send request
    auto request_item   = ipc::Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto request_status = ipc::Queue::make(ipc::QueueId::FlashRequest)
                              .send(request_item, timeout);

    if (request_status != ipc::Queue::Status::Ok) {
        auto status = semaphore_give(timeout);
        if (status != nv::ipc::Queue::Status::Ok) {
            return flash::Status::Timeout;
        }
        return flash::Status::Busy;
    }
    auto event = ipc::Event::make(ipc::EventId::FlashEvent);

    switch (request.header.type) {
        case RequestType::BgStatusQuery: {
            auto status = event.set(EventBits::BackgroundCopyQueryEvent);
            if (status != ipc::Event::Status::Ok) {
                nv::info("Set event 0x%x failed\n", EventBits::BackgroundCopyQueryEvent);
            }
        } break;
        case RequestType::Read:
        case RequestType::Write:
        case RequestType::Erase:
        case RequestType::CfpaRead:
        case RequestType::CfpaWriteSecVersion:
        case RequestType::CfpaWriteKeyRevoke:
        case RequestType::CfpaCustomerRead:
        case RequestType::CfpaCustomerWrite:
        case RequestType::CmpaRead:
        case RequestType::EfuseRead:
        case RequestType::EfuseProgram:
        case RequestType::EfuseLifeCycleRead:
        case RequestType::CrcRead            : {
            auto status = event.set(EventBits::FlashOperationEvent);
            if (status != ipc::Event::Status::Ok) {
                nv::info("Set event 0x%x failed\n", EventBits::FlashOperationEvent);
            }
        } break;
        case RequestType::DataStoreSet:
        case RequestType::DataStoreGet:
        case RequestType::GetApFwAuthenticateData:
        case RequestType::SetApFwAuthenticateData: {
            auto status = event.set(EventBits::DataStoreEvent);
            if (status != ipc::Event::Status::Ok) {
                nv::info("Set event 0x%x failed\n", EventBits::DataStoreEvent);
            }
        } break;
        default: break;
    }

    /// wait response
    auto response_item = ipc::Queue::Item(std::bit_cast<uint8_t*>(&response), sizeof(response));
    auto response_status = ipc::Queue::make(ipc::QueueId::FlashResponse)
                               .recv(response_item, timeout);

    if (response_status != ipc::Queue::Status::Ok) {
        auto bits_to_set = (request.header.core_id == nv::ipc::CoreId::Core0)
                             ? EventBits::TimeoutEvent
                             : EventBits::TimeoutEventFromCore1;
        (void)event.set(bits_to_set);
        return flash::Status::Timeout;
    }

    if (semaphore_give(timeout) != ipc::Queue::Status::Ok) {
        return flash::Status::Timeout;
    }

    return response.status;
}

nv::ipc::Queue::Status Task::receive_request_from_queue(const Request& request)
{
    using namespace nv::ipc;
    auto request_item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    /// FixMe to avoid race condition, we need to peek the item first and remove it
    /// after processing
    auto status = _request.recv(request_item);
    return status;
}

nv::ipc::Queue::Status Task::send_response_to_queue(Response&       response,
                                                    nv::ipc::CoreId dest_core)
{
    using namespace nv::ipc;
    auto timeout_event_bits = (dest_core == CoreId::Core0) ? EventBits::TimeoutEvent
                                                           : EventBits::TimeoutEventFromCore1;
    if (_event.bits().value() & timeout_event_bits) {
        (void)_event.clear(timeout_event_bits);
        auto status = semaphore_give(FlashTimeout, dest_core);
        if (status != nv::ipc::Queue::Status::Ok) {
            return status;
        }
        return nv::ipc::Queue::Status::Ok;
    }
    auto response_item   = Queue::Item(std::bit_cast<uint8_t*>(&response), sizeof(response));
    auto response_status = _response.send(response_item, FlashTimeout, dest_core);
    return response_status;
}

void Task::start()
{
    using namespace nv::ipc;
    // nv::info("%s start event loop\n", name().data());

    auto wait_bits = []() {
        if constexpr (nv::common::build::IsStreamBoot) {
            return EventBits::FlashOperationEvent | EventBits::DataStoreEvent
                 | EventBits::WdtEvent;
        }
        else {
            return EventBits::FlashOperationEvent | EventBits::BackgroundCopyStartEvent
                 | EventBits::BackgroundCopyQueryEvent | EventBits::BackgroundCopyNextEvent
                 | EventBits::DataStoreEvent | EventBits::WdtEvent;
        }
    }();
    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Flash);

    // notify spdm task that flash task is ready, since we need to use flash task to generate
    // the certificate and measurement.
    if (auto& spdm_event = nv::ipc::Event::make(nv::ipc::EventId::SpdmTask);
        spdm_event.set(nv::spdm::Task::CertReadyBit) != nv::ipc::Event::Status::Ok) {
        nv::info("set nv::spdm::Task::CertReadyBit failed\n");
    }

    using namespace std::chrono_literals;
    while (true) {
        auto bits       = _event.wait(wait_bits, false, false);
        auto event_bits = bits.value();

        if (event_bits & EventBits::WdtEvent) {
            nv::watchdog::Runtime::mark_task_alive(nv::watchdog::TaskMonitorIndex::Flash);
            (void)_event.clear(EventBits::WdtEvent);
        }

        if (event_bits & EventBits::FlashOperationEvent) {
            const Request Req{};
            Response      response{};
            auto          status = receive_request_from_queue(Req);
            if (status != Queue::Status::Ok) {
                nv::info("request fail -  %d\n", status);
                continue;
            }

            // Clear event if _request is empty
            if (_request.size() == 0) {
                (void)_event.clear(EventBits::FlashOperationEvent);
            }

            process(Req, response);

            auto response_status = send_response_to_queue(response, Req.header.core_id);
            if (response_status != Queue::Status::Ok) {
                nv::info("response fail - %d\n", response_status);
            }
        }
        else if (event_bits & EventBits::BackgroundCopyStartEvent) {
            // Start BG
            (void)_event.clear(EventBits::BackgroundCopyStartEvent);
            _bg_handler.start();
            (void)_event.set(EventBits::BackgroundCopyNextEvent);
        }
        else if (event_bits & EventBits::BackgroundCopyQueryEvent) {
            const Request Req{};
            Response      response{};
            auto          status = receive_request_from_queue(Req);
            if (status != Queue::Status::Ok) {
                nv::info("request fail -  %d\n", status);
                continue;
            }

            // Clear event if _request is empty
            if (_request.size() == 0) {
                (void)_event.clear(EventBits::BackgroundCopyQueryEvent);
            }

            auto bg_status  = _bg_handler.status_progress(response);
            response.status = bg_status;
            response.header = Req.header;

            auto response_status = send_response_to_queue(response, Req.header.core_id);
            if (response_status != Queue::Status::Ok) {
                nv::info("response fail - %d\n", response_status);
            }
        }

        else if (event_bits & EventBits::DataStoreEvent) {
            const Request Req{};
            Response      response{};
            auto          status = receive_request_from_queue(Req);
            if (status != Queue::Status::Ok) {
                nv::info("request fail -  %d\n", status);
                continue;
            }

            // Clear event if _request is empty
            if (_request.size() == 0) {
                (void)_event.clear(EventBits::DataStoreEvent);
            }

            process(Req, response);

            auto response_status = send_response_to_queue(response, Req.header.core_id);
            if (response_status != Queue::Status::Ok) {
                nv::info("response fail - %d\n", response_status);
            }
        }
        else if (event_bits == EventBits::BackgroundCopyNextEvent) {
            // Checking current BG status
            (void)_event.clear(EventBits::BackgroundCopyNextEvent);
            _bg_handler.next(_driver);
            if (_bg_handler.state() == BackgroundCopy::State::InProgress) {
                (void)_event.set(EventBits::BackgroundCopyNextEvent);
            }
        }
    }
}

void Task::process(const Request& request, Response& response)
{
    response.header  = request.header;
    response.address = request.address;
    response.length  = request.length;
    switch (request.header.type) {
        case RequestType::Read        : read(request, response); break;
        case RequestType::Write       : write(request, response); break;
        case RequestType::Erase       : erase(request, response); break;
        case RequestType::DataStoreSet: set_data(request, response); break;
        case RequestType::DataStoreGet: get_data(request, response); break;
        case RequestType::CfpaRead    : cfpa_get(request, response); break;
        case RequestType::CfpaWriteSecVersion:
            cfpa_set_secure_version(request, response);
            break;
        case RequestType::CfpaWriteKeyRevoke: cfpa_set_key_revoke(request, response); break;
        case RequestType::CfpaCustomerRead  : cfpa_get_customer(request, response); break;
        case RequestType::CfpaCustomerWrite : cfpa_set_customer(request, response); break;
        case RequestType::CmpaRead          : cmpa_get(request, response); break;
        case RequestType::EfuseProgram      : efuse_program(request, response); break;
        case RequestType::EfuseRead         : efuse_read(request, response); break;
        case RequestType::CrcRead           : crc_read(response); break;
        case RequestType::EfuseLifeCycleRead: efuse_life_cycle_read(response); break;
        case RequestType::GetApFwAuthenticateData:
            get_ap_fw_authenticate_data(request, response);
            break;
        case RequestType::SetApFwAuthenticateData:
            set_ap_fw_authenticate_data(request, response);
            break;
        default: break;
    }
}

void Task::get_ap_fw_authenticate_data(const Request& request, Response& response)
{
    if (request.key != Key::NpdsActiveApFwAuthenticateData
        && request.key != Key::NpdsUpdateApFwAuthenticateData) {
        response.status = nv::flash::Status::InvalidParam;
        return;
    }
    const auto& now_data = _npds.get_ap_fw_authenticate_data_index(request.key);

    if (request.offset + request.length > sizeof(now_data.active_ap_fw_authenticate_data)) {
        response.status = nv::flash::Status::InvalidParam;
        return;
    }
    if (request.length > BufferSize) {
        response.status = nv::flash::Status::InvalidParam;
        return;
    }
    if (now_data.is_on_set_data) {
        response.status = nv::flash::Status::Busy;
        return;
    }
    auto data_in_view = std::span<uint8_t>(
        std::bit_cast<uint8_t*>(&now_data.active_ap_fw_authenticate_data),
        sizeof(now_data.active_ap_fw_authenticate_data));
    auto data_in_view_sub = data_in_view.subspan(request.offset, request.length);
    response.length       = data_in_view_sub.size();
    std::copy(data_in_view_sub.begin(), data_in_view_sub.end(), response.buffer.begin());
    response.status = nv::flash::Status::Ok;
    return;
}

void Task::set_ap_fw_authenticate_data(const Request& request, Response& response)
{
    if (request.key != Key::NpdsActiveApFwAuthenticateData
        && request.key != Key::NpdsUpdateApFwAuthenticateData) {
        response.status = nv::flash::Status::InvalidParam;
        return;
    }
    auto& now_data = _npds.get_ap_fw_authenticate_data_index(request.key);

    if (request.offset + request.length > sizeof(now_data.active_ap_fw_authenticate_data)) {
        response.status = nv::flash::Status::InvalidParam;
        return;
    }
    if (request.length > BufferSize) {
        response.status = nv::flash::Status::InvalidParam;
        return;
    }
    now_data.is_on_set_data = true;

    auto data_in_view = std::span<uint8_t>(
        std::bit_cast<uint8_t*>(&now_data.active_ap_fw_authenticate_data),
        sizeof(now_data.active_ap_fw_authenticate_data));
    auto data_in_view_sub = data_in_view.subspan(request.offset, request.length);
    std::copy_n(request.buffer.begin(), request.length, data_in_view_sub.begin());

    if (request.offset + request.length == sizeof(now_data.active_ap_fw_authenticate_data)) {
        now_data.is_on_set_data = false;
    }
    response.status = nv::flash::Status::Ok;
    return;
}

void Task::efuse_program(const Request& request, Response& response)
{
    auto flash_status = _driver.program_efuse(*std::bit_cast<uint32_t*>(request.buffer.data()),
                                              request.address);

    response.status = flash_status;
}

void Task::efuse_read(const Request& request, Response& response)
{
    auto flash_status = _driver.read_efuse(*std::bit_cast<uint32_t*>(response.buffer.data()),
                                           request.address);

    response.status = flash_status;
}

void Task::read(const Request& request, Response& response)
{
    auto flash_status = _driver.read(request.address, request.length, response.buffer);

    response.status = flash_status;
}

void Task::write(const Request& request, Response& response)
{
    auto flash_status = _driver.write(request.address, request.length, request.buffer);

    response.status = flash_status;
}

void Task::erase(const Request& request, Response& response)
{
    auto flash_status = _driver.erase(request.address);

    response.status = flash_status;
}

void Task::set_data(const Request& request, Response& response)
{
    response.status = nv::flash::Status::InvalidParam;
    if (request.key < Key::NpdsEnd) {
        Data data = 0;
        std::memcpy(&data, std::bit_cast<void*>(&request.buffer[0]), sizeof(Data));
        auto status     = _npds.set_data(request.key, data);
        response.status = status;
    }
    else if (request.key >= Key::PdsStart && request.key < Key::PdsEnd) {
        Data data = 0;
        std::memcpy(&data, std::bit_cast<void*>(&request.buffer[0]), sizeof(Data));
        auto status     = _pds.set_data(request.key, data);
        response.status = status;
    }
}

void Task::get_data(const Request& request, Response& response)
{
    response.status = nv::flash::Status::InvalidParam;
    if (request.key < Key::NpdsEnd) {
        Data data       = 0;
        auto status     = _npds.get_data(request.key, data);
        response.status = status;
        std::memcpy(std::bit_cast<void*>(&response.buffer[0]), &data, sizeof(Data));
    }
    else if (request.key >= Key::PdsStart && request.key < Key::PdsEnd) {
        Data data       = 0;
        auto status     = _pds.get_data(request.key, data);
        response.status = status;
        std::memcpy(std::bit_cast<void*>(&response.buffer[0]), &data, sizeof(Data));
    }
}

Status Task::get_data_from_kernel(Key key, Data& data)
{
    if (key < Key::NpdsEnd) {
        return _npds.get_data(key, data);
    }
    else if (key >= Key::PdsStart && key < Key::PdsEnd) {
        return _pds.get_data(key, data);
    }
    return nv::flash::Status::InvalidParam;
}

Status Task::set_data_from_kernel(Key key, const Data& data)
{
    if (key < Key::NpdsEnd) {
        return _npds.set_data(key, data);
    }
    else if (key >= Key::PdsStart && key < Key::PdsEnd) {
        return _pds.set_data(key, data);
    }
    return nv::flash::Status::InvalidParam;
}

void Task::cfpa_get_customer(const Request& request, Response& response)
{
    Data last_updated{};
    _pds.get_data(Key::PdscfpaCustomerLastUpdated, last_updated);
    if (last_updated < UINT8_MAX) {
        auto sts = _driver.read_cfpa_customer(
            response.buffer, static_cast<uint8_t>(last_updated), request.offset);
        response.status = sts;
    }
    else {
        response.status = nv::flash::Status::InvalidParam;
    }
}

void Task::cfpa_set_customer(const Request& request, Response& response)
{
    auto sts = _driver.write_cfpa_customer(request.buffer, cfpa_write_page, request.offset);
    if (sts == nv::flash::Status::Ok) {
        _pds.set_data(Key::PdscfpaCustomerLastUpdated, cfpa_write_page);
    }

    response.status = sts;
}

void Task::cfpa_set_secure_version(const Request& request, Response& response)
{
    auto sts        = _driver.increase_cfpa_secure_version(request.sec_key_version,
                                                    request.key_rollback_select);
    response.status = sts;
}

void Task::cfpa_set_key_revoke(const Request& request, Response& response)
{
    auto sts        = _driver.increase_cfpa_image_key_revoke(request.sec_key_version,
                                                      request.key_rollback_select);
    response.status = sts;
}

void Task::cfpa_get(const Request& request, Response& response)
{
    auto sts        = _driver.read_cfpa(response.buffer, request.offset);
    response.status = sts;
}

void Task::cmpa_get(const Request& request, Response& response)
{
    auto sts        = _driver.read_cmpa(response.buffer, request.offset);
    response.status = sts;
}

void Task::crc_read(Response& response)
{
    auto sts        = _driver.read_crc(*std::bit_cast<uint32_t*>(response.buffer.data()));
    response.status = sts;
}

void Task::efuse_life_cycle_read(Response& response)
{
    auto sts = _driver.read_life_cycle(
        *std::bit_cast<sys::flash::LifeCycleStatus*>(response.buffer.data()));
    response.status = sts;
}

void Task::wdt_notify()
{
    auto event = nv::ipc::Event::make(nv::ipc::EventId::FlashEvent);
    auto sts   = event.set(EventBits::WdtEvent);
    if (sts != nv::ipc::Event::Status::Ok) {
        // TBD
    }
}