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

#include "nv/pldm/task.h"

#include <cstring>

#include "nv/bootloader.h"
#include "nv/common/build.h"
#include "nv/flash/flash.h"
#include "nv/mctp/composer.h"
#include "nv/mctp/driver.h"
#include "nv/watchdog/runtime.h"
#include "nv/nv.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include "corepdk/platforms/mcxn236/pldm-fd/src/pldm_wrap.h"
#include "nv/vrot/ap_background_copy.h"
#include "sys/flash/flash_config.h"
#include NV_IPC_CONFIG_H

using namespace std::chrono_literals;
using namespace nv::ipc;
using namespace nv::pldm;
using namespace nv::mctp;

static_assert(get_queue_max_items(nv::ipc::QueueId::PldmRx) == 1, "rx queue size should be 1");

//  base
extern "C" void ada_init_pldm_base_ctx(PldmBaseContextRecord* pldm_base_context);
extern "C" void ada_process_base_commands(PldmBaseContextRecord* pldm_base_context, NvU32 size);

//  fwupdate
extern "C" void ada_pldmfw_context_init(PldmContextRecord* pldm_context);
extern "C" void ada_pldmfw_state_runner(PldmContextRecord* pldm_context, NvU32 size);
extern "C" void ada_pldmfw_timeout_handler(PldmContextRecord* pldm_context);
extern "C" void ada_pldmfw_authenticate_handler(PldmContextRecord* pldm_context,
                                                NvU8               verify_cc);

namespace nv::pldm {

namespace {

pldm::Status record_background_copy_component(uint16_t component_id)
{
    if (flash::Flash::set_data(flash::Key::NpdsBackgroundCopyComponentId,
                               static_cast<flash::Data>(component_id))
        != flash::Status::Ok) {
        return pldm::Status::Unknown;
    }
    return pldm::Status::Ok;
}

}  // namespace

void Task::make()
{
    constexpr auto StackSize = std::max(2144, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    NV_TASK_DATA static Task                       task;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Pldm, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    using namespace std::chrono_literals;
    auto& task = *static_cast<Task*>(params);
    nv::info("MCU Firmware PLDM Task\n");
    task.main();
}

void Task::left_shift_array(std::span<NvU8> arr, std::size_t shift)
{
    const std::size_t Size = arr.size();
    if (shift >= Size) {
        // If shift is greater than or equal to the size of the array, fill with zeros
        std::fill(arr.begin(), arr.end(), 0);
    }
    else {
        // Cast shift to the appropriate type
        auto diff_type_shift = static_cast<
            std::iterator_traits<decltype(arr.begin())>::difference_type>(shift);
        // Shift elements to the left using std::copy
        std::copy(std::next(arr.begin(), diff_type_shift), arr.end(), arr.begin());
        // Fill the remaining positions with zeros
        std::fill(std::prev(arr.end(), diff_type_shift), arr.end(), 0);
    }
}

void Task::get_interface_info(PldmContextRecord&   pldm_context,
                              PldmfwInterfaceInfo& interface_info,
                              uint16_t&            size)
{
    mctp::Packet packet = {};

    std::array<uint8_t, 8> arr_rx = {};
    const std::span<NvU8>  SpanRx(static_cast<NvU8*>(pldm_context.rx_msg), 8);
    std::copy(SpanRx.begin(), SpanRx.end(), arr_rx.begin());

    memcpy(&packet.priv, arr_rx.data(), sizeof(packet.priv));
    memcpy(&packet.hdr, arr_rx.data() + sizeof(packet.priv), sizeof(packet.hdr));

    // get interface info
    interface_info.client         = packet.priv.packet_interface;
    interface_info.header_version = packet.hdr.hdr_ver;
    interface_info.message_tag    = packet.hdr.msg_tag;
    interface_info.src_eid        = packet.hdr.src_eid;

    // get size
    if (packet.priv.packet_length < NV_PLDM_MCTP_HDR_SIZE + NV_PLDM_MSG_TYPE_SIZE) {
        // prevent unsigned wrap
        size = 0;
    }
    else {
        size = packet.priv.packet_length - NV_PLDM_MCTP_HDR_SIZE - NV_PLDM_MSG_TYPE_SIZE;
    }
}

bool Task::is_background_copy_automatic(bool log_init_setup)
{
    flash::Data background_copy_policy{};
    flash::Data background_copy_policy_one_time{};
    auto        status = flash::Flash::get_data(flash::Key::PdsBackgroundSetup,
                                         background_copy_policy);
    if (status != flash::Status::Ok) {
        background_copy_policy = static_cast<flash::Data>(BackgroundCopyPolicy::Default);
    }
    status = flash::Flash::get_data(flash::Key::PdsBackgroundSetupOneTime,
                                    background_copy_policy_one_time);
    if (status != flash::Status::Ok) {
        background_copy_policy_one_time = static_cast<flash::Data>(
            BackgroundCopyPolicy::Default);
    }

    // clean up one time setup
    (void)flash::Flash::set_data(flash::Key::PdsBackgroundSetupOneTime,
                                 static_cast<flash::Data>(BackgroundCopyPolicy::Default));

    if (log_init_setup) {
        nv::logger::info(nv::logger::Event::BackgroundCopyInitSetup,
                         {static_cast<uint8_t>(background_copy_policy & UINT8_MAX),
                          static_cast<uint8_t>(background_copy_policy_one_time & UINT8_MAX)});
    }

    // priority one time setup
    if (background_copy_policy_one_time
        == static_cast<flash::Data>(BackgroundCopyPolicy::OnceEnable)) {
        return true;
    }
    else if (background_copy_policy_one_time
             == static_cast<flash::Data>(BackgroundCopyPolicy::OnceDisable)) {
        return false;
    }
    // default setup
    else if (background_copy_policy == static_cast<flash::Data>(BackgroundCopyPolicy::Disable)
             || background_copy_policy
                    == static_cast<flash::Data>(BackgroundCopyPolicy::Default)) {
        return false;
    }

    return true;
}

Task::Task()
: nv::ipc::Task(nv::ipc::TaskId::Pldm, "PLDM")
, _event(nv::ipc::Event::make(nv::ipc::EventId::PldmTask))
, _rx(nv::ipc::Queue::make(nv::ipc::QueueId::PldmRx))
, _rx4k(nv::ipc::Queue::make(nv::ipc::QueueId::PldmRx4k))
, _timer(nv::ipc::Timer::make(nv::ipc::TimerId::Pldm, 1s, on_timer, false))
{}

[[noreturn]] void Task::main()
{
    // init pldm context
    PldmContextRecord     pldm_context      = {};
    PldmBaseContextRecord pldm_base_context = {};
    ada_pldmfw_context_init(&pldm_context);
    ada_init_pldm_base_ctx(&pldm_base_context);

    auto wait_bits = []() {
        if constexpr (nv::common::build::IsStreamBoot) {
            return static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::PldmTaskRxBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::PldmTaskRx4kBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::TimerBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::ApBgStartBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::WdtEventBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::ProtocolResetBit);
        }
        else {
            return static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::PldmTaskRxBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::PldmTaskRx4kBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::TimerBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::BgStartBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::ApBgStartBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::WdtEventBit)
                 | static_cast<std::underlying_type_t<PldmEventBits>>(
                       Task::PldmEventBits::ProtocolResetBit);
        }
    }();

    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Pldm);

    while (true) {
        auto bits  = _event.wait(wait_bits, false, false, 1s);
        auto event = bits.value();

        if (event & Task::PldmEventBits::WdtEventBit) {
            _event.clear(Task::PldmEventBits::WdtEventBit);
            watchdog::Runtime::mark_task_alive(nv::watchdog::TaskMonitorIndex::Pldm);
        }

        if (event & Task::PldmEventBits::ProtocolResetBit) {
            nv::logger::info(nv::logger::Event::PldmProtocolResetStart);
            // Clear all event bits
            _event.clear(wait_bits);

            // Set to 0 to avoid executing the code below
            event = 0;

            // Clear queue PldmRx & PldmRx4k
            const std::span<NvU8> PldmRx_Span(static_cast<NvU8*>(pldm_context.rx_msg),
                                              NV_PLDM_BASE_RX_QUEUE_SIZE);

            Queue::Item PldmRx_Item(PldmRx_Span.data(), NV_PLDM_BASE_RX_QUEUE_SIZE);
            (void)_rx.recv(PldmRx_Item, 0s);

            const std::span<NvU8> PldmRx4k_Span(static_cast<NvU8*>(pldm_context.rx_msg),
                                                NV_PLDM_MAX_4K_RX_QUEUE_SIZE);

            Queue::Item PldmRx4k_Item(PldmRx4k_Span.data(), NV_PLDM_MAX_4K_RX_QUEUE_SIZE);
            (void)_rx4k.recv(PldmRx4k_Item, 0s);

            // Reset pldm context
            pldm_context      = {};
            pldm_base_context = {};
            ada_pldmfw_context_init(&pldm_context);
            ada_init_pldm_base_ctx(&pldm_base_context);

            // Revert bgcopy possible changes
            (void)flash::Flash::set_data(flash::Key::NpdsAllowInitBackgroundCopy, 0);

            // Restart the PLDM timer
            _timer.reset();

            nv::logger::info(nv::logger::Event::PldmProtocolResetEnd);
        }

        if (event & Task::PldmEventBits::PldmTaskRxBit) {
            // nv::info("pldm:event: PldmTaskRxBit\n");
            _event.clear(Task::PldmEventBits::PldmTaskRxBit);

            const std::span<NvU8> Span(static_cast<NvU8*>(pldm_context.rx_msg),
                                       NV_PLDM_BASE_RX_QUEUE_SIZE);

            Queue::Item item(Span.data(), NV_PLDM_BASE_RX_QUEUE_SIZE);
            auto        status = _rx.recv(item, 100ms);
            if (status != Queue::Status::Ok) {
                nv::info("pldm:queue: recv fail %d\n", status);
            }
            else {
                uint16_t            size           = 0;
                PldmfwInterfaceInfo interface_info = {};
                //  update eid/interface to pldm context and get size
                get_interface_info(pldm_context, interface_info, size);

                left_shift_array(pldm_context.rx_msg, NV_PLDM_SHIFT_BYTES);

#if 0
                nv::info("0x00: ");
                for (size_t i=0; i<64; i++) {
                    if (i && (i % 16 == 0)) nv::info("\n0x%02x: ", i);
                    nv::info("%02x ", pldm_context.rx_msg[i]);
                }
#endif

                if (pldm_context.rx_msg[1] == NV_PLDM_TYPE_MSG_CTRL_AND_DISCOVERY) {
                    const std::span<NvU8> SpanReq(pldm_base_context.req);
                    const std::span<NvU8> SpanRx(static_cast<NvU8*>(pldm_context.rx_msg),
                                                 NV_PLDM_MAX_PLDM_RESP_SIZE);
                    std::copy(SpanRx.begin(), SpanRx.end(), SpanReq.begin());

                    pldm_base_context.rx_interface = interface_info;
                    ada_process_base_commands(&pldm_base_context, size);
                }
                else if (pldm_context.rx_msg[1] == NV_PLDM_TYPE_FIRMWARE_UPDATE) {
                    pldm_context.rx_interface = interface_info;
                    ada_pldmfw_state_runner(&pldm_context, size);
                }
                else {
                    nv::info("Invalid pldm type 0x%x", pldm_context.rx_msg[1]);
                }
            }
        }

        if (event & Task::PldmEventBits::PldmTaskRx4kBit) {
            // nv::info("pldm:event: PldmTaskRx4kBit\n");
            _event.clear(Task::PldmEventBits::PldmTaskRx4kBit);

            const std::span<NvU8> Span(static_cast<NvU8*>(pldm_context.rx_msg),
                                       NV_PLDM_MAX_4K_RX_QUEUE_SIZE);

            Queue::Item item(Span.data(), NV_PLDM_MAX_4K_RX_QUEUE_SIZE);
            auto        status = _rx4k.recv(item, 100ms);
            if (status != Queue::Status::Ok) {
                nv::info("pldm:queue: recv fail %d\n", status);
            }
            else {
                // @todo do we need to add pldm base support for 4k ?
                uint16_t            size           = 0;
                PldmfwInterfaceInfo interface_info = {};
                //  update eid/interface to pldm context and get size
                get_interface_info(pldm_context, interface_info, size);
                pldm_context.rx_interface = interface_info;

                left_shift_array(pldm_context.rx_msg, NV_PLDM_SHIFT_BYTES);
                ada_pldmfw_state_runner(&pldm_context, size);
            }
        }

        if (event & Task::PldmEventBits::TimerBit) {
            _event.clear(Task::PldmEventBits::TimerBit);
            ada_pldmfw_timeout_handler(&pldm_context);
        }
        if (event & Task::PldmEventBits::BgStartBit) {
            if (record_background_copy_component(sys::flash::config::McuComponentId)
                != pldm::Status::Ok) {
                nv::info("record MCU bg component failed\n");
            }

            flash::Data update_state{};
            flash::Data allow_bg_copy{};
            const bool  is_bgcopy_automatic = is_background_copy_automatic(true);
            bool        ap_bg_in_progress   = false;
            if constexpr (!nv::vrot::ApList.empty()) {
                ap_bg_in_progress = nv::vrot::ap_background_copy::is_in_progress();
            }
            auto flash_status = flash::Flash::get_data(flash::Key::PdsUpdateState,
                                                       update_state);
            if (flash_status != flash::Status::Ok) {
                update_state = static_cast<flash::Data>(bootloader::Driver::State::Invalid);
            }

            (void)flash::Flash::get_data(flash::Key::NpdsAllowInitBackgroundCopy,
                                         allow_bg_copy);

            nv::flash::ProgressPercent progress{};
            flash_status = flash::Flash::background_copy_query(progress);

            if (!ap_bg_in_progress
                && update_state != static_cast<flash::Data>(bootloader::Driver::State::Stage)
                && pldm_context.pldm_state == 0 &&  // idle
                ((is_bgcopy_automatic || allow_bg_copy == 1)
                 && flash_status != flash::Status::BackgroundCopyInprogress)) {
                nv::info("bg init\n");
                // clean AllowInitBackgroundCopy
                (void)flash::Flash::set_data(flash::Key::NpdsAllowInitBackgroundCopy, 0);

                auto status = flash::Flash::background_copy_start();
                nv::info("bg start 0x%x\n", status);

                if (status == flash::Status::Ok) {
                    pldm_context.is_backup_in_progress = true;

                    // set inacitve non bootable, clear update status
                    (void)bootloader::Driver::set_inactive_bootable(false);
                    (void)flash::Flash::set_data(flash::Key::PdsUpdateState, 0);
                    (void)flash::Flash::set_data(flash::Key::PdsUpdateSlot, 0);

                    // start polling bg status
                    auto event_status = _event.set(Task::PldmEventBits::BgEndBit);
                    if (event_status != Event::Status::Ok) {
                        nv::info("set event BgEndBit fail 0x%x\n", event_status);
                    }
                }
            }
            else if (ap_bg_in_progress) {
                nv::info("MCU bg rejected: AP bg in progress\n");
            }
            else if (!is_bgcopy_automatic) {
                nv::info("bg copy disabled\n");
                (void)flash::Flash::set_data(flash::Key::NpdsAllowInitBackgroundCopy, 1);
            }

            _event.clear(Task::PldmEventBits::BgStartBit);
        }
        if constexpr (nv::vrot::ApList.size() > 0) {
            if (event & Task::PldmEventBits::ApBgStartBit) {
                _event.clear(Task::PldmEventBits::ApBgStartBit);
                if (pldm_context.pldm_state != 0) {
                    nv::vrot::ap_background_copy::cancel(true);
                    pldm_context.is_backup_in_progress = false;
                    nv::info("ap bg canceled: PLDM busy\n");
                }
                else {
                    pldm_context.is_backup_in_progress = true;
                    const auto copy_status = nv::vrot::ap_background_copy::service();
                    const bool in_progress = copy_status
                                          == nv::flash::Status::BackgroundCopyInprogress;
                    pldm_context.is_backup_in_progress = in_progress;
                    if (in_progress) {
                        auto event_status = _event.set(Task::PldmEventBits::ApBgStartBit);
                        if (event_status != Event::Status::Ok) {
                            nv::info("set event ApBgStartBit fail 0x%x\n", event_status);
                            nv::vrot::ap_background_copy::cancel(true);
                            pldm_context.is_backup_in_progress = false;
                        }
                    }
                }
            }
        }
        if (event & Task::PldmEventBits::BgEndBit) {
            nv::flash::ProgressPercent progress{};
            auto                       status = flash::Flash::background_copy_query(progress);

            if (status == flash::Status::BackgroundCopyDone) {
                _event.clear(Task::PldmEventBits::BgEndBit);
                pldm_context.is_backup_in_progress = false;
                nv::info("bg end\n");

                // set inacitve bootable
                bootloader::Driver::set_inactive_bootable(true);
            }
        }
        if (event & Task::PldmEventBits::EventRequestBit) {
            _event.clear(Task::PldmEventBits::EventRequestBit);

            Request     request{};
            Queue::Item item(std::bit_cast<uint8_t*>(&request), sizeof(request));
            auto        queue_status = _rx.recv(item, 100ms);
            if (queue_status == Queue::Status::Ok) {
                switch (request.type) {
                    case RequestType::AuthMcuFinish: {
                        uint8_t verify_cc = 0;
                        get_mcu_authentication_result(request.buffer[0], verify_cc);
                        ada_pldmfw_authenticate_handler(&pldm_context, verify_cc);
                    } break;
                    case RequestType::AuthApFinish: {
                        if (request.length < 2) {
                            break;
                        }
                        const auto auth_request_id = request.buffer[1];
                        if (auth_request_id
                            != static_cast<uint8_t>(pldm_context.auth_request_id)) {
                            break;
                        }
                        uint8_t verify_cc = 0;
                        get_ap_authentication_result(
                            pldm_context.comp_id, request.buffer[0], verify_cc);
                        ada_pldmfw_authenticate_handler(&pldm_context, verify_cc);
                    } break;
                    default: break;
                }
            }
        }
    }
}

nv::pldm::Status Task::pldm_tx(const nv::mctp::Packet& packet)
{
    nv::ipc::Queue& queue = (packet.priv.packet_length > NV_PLDM_MCTP_HDR_PAYLOAD_SIZE)
                              ? nv::ipc::Queue::make(nv::ipc::QueueId::PldmRx4k)
                              : nv::ipc::Queue::make(nv::ipc::QueueId::PldmRx);

    auto item_size = (packet.priv.packet_length > NV_PLDM_MCTP_HDR_PAYLOAD_SIZE)
                       ? NV_PLDM_MAX_4K_RX_QUEUE_SIZE
                       : NV_PLDM_BASE_RX_QUEUE_SIZE;

    nv::ipc::Event& event        = Event::make(EventId::PldmTask);
    Event::Status   event_status = Event::Status::Ok;
    auto            item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&packet), item_size);

    auto queue_status = queue.send(item, 100ms);
    if (queue_status != Queue::Status::Ok) {
        return nv::pldm::Status::QueueSendFail;
    }

    event_status = (packet.priv.packet_length > NV_PLDM_MCTP_HDR_PAYLOAD_SIZE)
                     ? event.set(Task::PldmEventBits::PldmTaskRx4kBit)
                     : event.set(Task::PldmEventBits::PldmTaskRxBit);

    if (event_status != Event::Status::Ok) {
        return nv::pldm::Status::EventSetFail;
    }

    return nv::pldm::Status::Ok;
}

pldm::Status Task::pldm_bg_start()
{
    if constexpr (!nv::vrot::ApList.empty()) {
        if (nv::vrot::ap_background_copy::is_in_progress()) {
            nv::info("MCU bg rejected: AP bg in progress\n");
            return pldm::Status::Unknown;
        }
    }

    ipc::Event& event        = Event::make(EventId::PldmTask);
    auto        event_status = event.set(Task::PldmEventBits::BgStartBit);

    if (event_status != Event::Status::Ok) {
        return nv::pldm::Status::EventSetFail;
    }

    return pldm::Status::Ok;
}

pldm::Status Task::pldm_ap_bg_start([[maybe_unused]] uint16_t component_id)
{
    if constexpr (nv::vrot::ApList.empty()) {
        return pldm::Status::Unknown;
    }

    nv::flash::ProgressPercent progress{};
    if (flash::Flash::background_copy_query(progress)
        == flash::Status::BackgroundCopyInprogress) {
        nv::info("AP bg rejected: MCU bg in progress\n");
        return pldm::Status::Unknown;
    }

    if (const auto status = record_background_copy_component(component_id);
        status != pldm::Status::Ok) {
        return status;
    }

    ipc::Event& event        = Event::make(EventId::PldmTask);
    auto        event_status = event.set(Task::PldmEventBits::ApBgStartBit);
    if (event_status != Event::Status::Ok) {
        return pldm::Status::EventSetFail;
    }

    return pldm::Status::Ok;
}

uint16_t Task::latest_background_copy_component_id()
{
    flash::Data component_id{};
    if (flash::Flash::get_data(flash::Key::NpdsBackgroundCopyComponentId, component_id)
        != flash::Status::Ok) {
        return sys::flash::config::McuComponentId;
    }
    return static_cast<uint16_t>(component_id);
}

void Task::on_timer([[maybe_unused]] ipc::Timer& id)
{
    nv::ipc::Event& event = Event::make(EventId::PldmTask);
    // Explicitly ignoring the return value
    static_cast<void>(event.set(Task::PldmEventBits::TimerBit));
}

void Task::set_timer(std::chrono::microseconds ms)
{
    nv::ipc::Timer& timer = nv::ipc::Timer::make(nv::ipc::TimerId::Pldm, 1s, on_timer, false);
    timer.period(ms);
}

void Task::clear_timer()
{
    nv::ipc::Timer& timer = nv::ipc::Timer::make(nv::ipc::TimerId::Pldm, 1s, on_timer, false);
    timer.stop();
}

uint32_t Task::get_ticks()
{
    return sys::ipc::get_os_ticks();
}

void Task::wdt_notify()
{
    auto event = nv::ipc::Event::make(nv::ipc::EventId::PldmTask);
    auto sts   = event.set(Task::PldmEventBits::WdtEventBit);
    if (sts != nv::ipc::Event::Status::Ok) {
        // TBD
    }
}

bool Task::to_pldm(ipchandler::Id src_id, Request request)
{
    auto request_item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));

    auto status = ipchandler::Driver::send(
        src_id, ipchandler::Id::Pldm, request_item, sizeof(request), true);
    if (status != ipchandler::Status::Success) {
        return false;
    }

    // set event
    if (nv::ipc::Event::Status::Ok
        != nv::ipc::Event::make(nv::ipc::EventId::PldmTask)
               .set(nv::pldm::Task::PldmEventBits::EventRequestBit)) {
        return false;
    }

    return true;
}

void Task::protocol_reset()
{
    nv::ipc::Event& event = Event::make(EventId::PldmTask);
    // Explicitly ignoring the return value
    static_cast<void>(event.set(Task::PldmEventBits::ProtocolResetBit));
}

}  //  namespace nv::pldm
