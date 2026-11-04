/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/secure_boot/secure_boot.h"

#include <bit>
#include <chrono>
#include "nv/vrot/interface/interface.h"
#include "nv/common/debug.h"
#include "nv/common/utils.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/bootloader.h"
#include "nv/debugtoken/debugtoken.h"
#include "nv/flash/flash.h"
#include "nv/pldm/task.h"
#include "nv/vrot/ap_background_copy.h"
#include "nv/logger/log.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"

namespace nv::secure_boot {

namespace {
struct SecureBootAuthResult
{
    uint8_t                        ap_id{};
    Slot                           slot{nv::vrot::InvalidSlot};
    nv::spdm::crypto::CryptoStatus result{nv::spdm::crypto::CryptoStatus::FailUnknown};
    uint8_t                        auth_request_id{};
};

static_assert(sizeof(SecureBootAuthResult) == nv::ipc::SecureBootAuthResultQueueSize);

constexpr auto SecureBootTaskId  = nv::ipc::TaskId::SecureBoot;
constexpr auto SecureBootEventId = nv::ipc::EventId::SecureBootTask;
static_assert(nv::vrot::ApList.size() <= 1,
              "SecureBoot uses one AuthResultReady bit; multi-AP needs per-AP signaling");
constexpr nv::ipc::Event::Bits IdleWaitBits = EventBits::AuthResultReady
                                            | EventBits::ApResetRequested;
constexpr auto AuthEventWaitTimeout       = std::chrono::milliseconds{1000};
constexpr auto BootPollDelay              = std::chrono::milliseconds{100};
constexpr auto BootWaitTimeout            = std::chrono::seconds{120};
constexpr auto BackgroundCopyPollInterval = std::chrono::milliseconds{100};
constexpr auto IdleEventWaitTimeout       = std::chrono::seconds{60};

constexpr uint32_t BootWaitTimeoutTicks = static_cast<uint32_t>(configTICK_RATE_HZ)
                                        * static_cast<uint32_t>(BootWaitTimeout.count());

bool boot_wait_timed_out(uint32_t started_ticks)
{
    return nv::ipc::Supervisor::get_os_ticks() - started_ticks >= BootWaitTimeoutTicks;
}

bool is_secure_boot_request(nv::ipc::TaskId request_task_id)
{
    return request_task_id == SecureBootTaskId;
}

bool background_copy_timed_out(const nv::vrot::ApInfo& ap, uint32_t started_ticks)
{
    const auto timeout_ticks = static_cast<uint32_t>(configTICK_RATE_HZ)
                             * ap.background_copy_timeout_seconds;
    return nv::ipc::Supervisor::get_os_ticks() - started_ticks >= timeout_ticks;
}

void log_flash_write_fail(uint8_t ap_id, nv::flash::Key key, nv::flash::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootFlashWriteFail,
        nv::logger::data_from_two_u32(static_cast<uint32_t>(key),
                                      (static_cast<uint32_t>(status) << 8U) | ap_id));
}

void log_event_set_fail(uint8_t ap_id, nv::ipc::Event::Bits bits, nv::ipc::Event::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootEventSetFail,
        nv::logger::data_from_two_u32(bits, (static_cast<uint32_t>(status) << 8U) | ap_id));
}

void log_queue_send_fail(uint8_t ap_id, nv::ipc::Queue::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootQueueSendFail,
        nv::logger::data_from_u32((static_cast<uint32_t>(status) << 8U) | ap_id));
}

void log_queue_recv_fail(uint8_t ap_id, nv::ipc::Queue::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootQueueRecvFail,
        nv::logger::data_from_u32((static_cast<uint32_t>(status) << 8U) | ap_id));
}

bool start_recovery_copy(const nv::vrot::ApInfo& ap, uint8_t source_slot, uint8_t dest_slot)
{
    const auto ap_id = static_cast<uint8_t>(ap.id);
    if (ap.background_copy_timeout_seconds == 0U) {
        nv::logger::error_no_wait(nv::logger::Event::SecureBootSlotRestoreFail,
                                  nv::logger::data_from_two_u32(
                                      static_cast<uint32_t>(nv::flash::Status::InvalidParam),
                                      (static_cast<uint32_t>(source_slot) << 8U) | ap_id));
        return false;
    }

    const auto copy_status = nv::vrot::ap_background_copy::start(ap, source_slot, dest_slot);
    if (copy_status != nv::flash::Status::Ok) {
        nv::logger::error_no_wait(
            nv::logger::Event::SecureBootSlotRestoreFail,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(copy_status),
                                          (static_cast<uint32_t>(source_slot) << 8U) | ap_id));
        return false;
    }

    const auto schedule_status = nv::pldm::Task::pldm_ap_bg_start(ap.component_id);
    if (schedule_status != nv::pldm::Status::Ok) {
        nv::vrot::ap_background_copy::cancel(false);
        nv::logger::error_no_wait(
            nv::logger::Event::SecureBootSlotRestoreFail,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(schedule_status),
                                          (static_cast<uint32_t>(source_slot) << 8U) | ap_id));
        return false;
    }
    return true;
}

void signal_auth_result_ready(uint8_t ap_id)
{
    auto&      event  = nv::ipc::Event::make(SecureBootEventId);
    const auto status = event.set(EventBits::AuthResultReady, nv::ipc::CoreId::Core0);
    if (status != nv::ipc::Event::Status::Ok) {
        log_event_set_fail(ap_id, EventBits::AuthResultReady, status);
    }
}

void log_event_clear_fail(uint8_t                ap_id,
                          nv::ipc::Event::Bits   bits,
                          nv::ipc::Event::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootEventClearFail,
        nv::logger::data_from_two_u32(bits, (static_cast<uint32_t>(status) << 8U) | ap_id));
}

void clear_event_bits(uint8_t ap_id, nv::ipc::Event& event, nv::ipc::Event::Bits bits)
{
    const auto status = event.clear(bits);
    if (status != nv::ipc::Event::Status::Ok) {
        log_event_clear_fail(ap_id, bits, status);
    }
}

nv::ipc::Event::Bits wait_event_bits(nv::ipc::Event&           event,
                                     nv::ipc::Event::Bits      wait_bits,
                                     std::chrono::milliseconds timeout)
{
    const auto bits = event.wait(wait_bits, false, false, timeout);
    if (!bits) {
        return 0U;
    }
    return bits.value() & wait_bits;
}

void drain_auth_result_queue()
{
    auto& queue = nv::ipc::Queue::make(nv::ipc::QueueId::SecureBootAuthResult);
    // NOLINTNEXTLINE(misc-const-correctness) queue.recv() writes through Queue::Item.
    SecureBootAuthResult result{};
    auto item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&result), sizeof(result));
    while (queue.recv(item, std::chrono::microseconds{0}) == nv::ipc::Queue::Status::Ok) {}
}

bool send_auth_result(uint8_t                        ap_id,
                      Slot                           slot,
                      nv::spdm::crypto::CryptoStatus result,
                      uint8_t                        auth_request_id)
{
    auto& queue = nv::ipc::Queue::make(nv::ipc::QueueId::SecureBootAuthResult);

    const SecureBootAuthResult auth_result{
        .ap_id           = ap_id,
        .slot            = slot,
        .result          = result,
        .auth_request_id = auth_request_id,
    };
    auto item = nv::ipc::Queue::ConstItem(std::bit_cast<const uint8_t*>(&auth_result),
                                          sizeof(auth_result));

    const auto status = queue.send(item, std::chrono::microseconds{0});
    if (status != nv::ipc::Queue::Status::Ok) {
        log_queue_send_fail(ap_id, status);
        return false;
    }
    signal_auth_result_ready(ap_id);
    return true;
}

nv::flash::Key authenticate_data_key_for_ap(uint8_t ap_id, nv::flash::Key role_key)
{
    if (ap_id != static_cast<uint8_t>(nv::vrot::ApId::AP0)
        && ap_id != static_cast<uint8_t>(nv::vrot::ApId::AP1)) {
        return nv::flash::Key::NpdsInvalid;
    }

    const bool is_ap1 = ap_id == static_cast<uint8_t>(nv::vrot::ApId::AP1);
    switch (role_key) {
        case nv::flash::Key::NpdsActiveApFwAuthenticateData:
            return is_ap1 ? nv::flash::Key::NpdsActiveAp1FwAuthenticateData
                          : nv::flash::Key::NpdsActiveApFwAuthenticateData;
        case nv::flash::Key::NpdsUpdateApFwAuthenticateData:
            return is_ap1 ? nv::flash::Key::NpdsUpdateAp1FwAuthenticateData
                          : nv::flash::Key::NpdsUpdateApFwAuthenticateData;
        case nv::flash::Key::NpdsInactiveApFwAuthenticateData:
            return is_ap1 ? nv::flash::Key::NpdsInactiveAp1FwAuthenticateData
                          : nv::flash::Key::NpdsInactiveApFwAuthenticateData;
        default: return nv::flash::Key::NpdsInvalid;
    }
}

nv::flash::Status write_authenticate_data(uint8_t                 ap_id,
                                          nv::flash::Key          key,
                                          const AuthenticateData& authenticate_data)
{
    const auto npds_key     = authenticate_data_key_for_ap(ap_id, key);
    const auto flash_status = nv::flash::Flash::set_ap_fw_authenticate_data(authenticate_data,
                                                                            npds_key);
    if (flash_status != nv::flash::Status::Ok) {
        log_flash_write_fail(ap_id, npds_key, flash_status);
    }
    return flash_status;
}

nv::flash::Status persist_authenticate_data(uint8_t                 ap_id,
                                            nv::flash::Key          authenticate_data_key,
                                            const AuthenticateData& authenticate_data,
                                            AuthenticateData&       existing_update)
{
    nv::flash::Status flash_status = nv::flash::Status::Ok;
    if (authenticate_data_key == nv::flash::Key::NpdsActiveApFwAuthenticateData) {
        flash_status = write_authenticate_data(
            ap_id, nv::flash::Key::NpdsActiveApFwAuthenticateData, authenticate_data);

        // Seed Update from Active only if Update has no successful auth record.
        if (flash_status == nv::flash::Status::Ok) {
            existing_update        = AuthenticateData{};
            const auto read_status = nv::flash::Flash::get_ap_fw_authenticate_data(
                existing_update,
                authenticate_data_key_for_ap(ap_id,
                                             nv::flash::Key::NpdsUpdateApFwAuthenticateData));
            const bool
                update_already_authenticated = (read_status == nv::flash::Status::Ok)
                                            && (existing_update.ap_auth_result
                                                == nv::spdm::crypto::CryptoStatus::Success);
            // Busy means PLDM may be staging Update; don't seed over that write.
            if (read_status != nv::flash::Status::Busy && !update_already_authenticated) {
                flash_status = write_authenticate_data(
                    ap_id, nv::flash::Key::NpdsUpdateApFwAuthenticateData, authenticate_data);
            }
        }
    }
    else if (authenticate_data_key == nv::flash::Key::NpdsUpdateApFwAuthenticateData) {
        flash_status = write_authenticate_data(
            ap_id, nv::flash::Key::NpdsUpdateApFwAuthenticateData, authenticate_data);
    }
    else if (authenticate_data_key == nv::flash::Key::NpdsInactiveApFwAuthenticateData) {
        flash_status = write_authenticate_data(
            ap_id, nv::flash::Key::NpdsInactiveApFwAuthenticateData, authenticate_data);
    }

    return flash_status;
}

nv::flash::Status
persist_authenticate_result(uint8_t                        ap_id,
                            nv::flash::Key                 authenticate_data_key,
                            nv::spdm::crypto::CryptoStatus ap_auth_result,
                            const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data)
{
    AuthenticateData authenticate_data{};
    authenticate_data.ap_metadata_tbs_data = tbs_data;
    authenticate_data.ap_auth_result       = ap_auth_result;
    AuthenticateData existing_update{};

    return persist_authenticate_data(
        ap_id, authenticate_data_key, authenticate_data, existing_update);
}

void persist_authenticate_data_in_progress(
    uint8_t                                         ap_id,
    nv::flash::Key                                  authenticate_data_key,
    const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data)
{
    if (authenticate_data_key != nv::flash::Key::NpdsActiveApFwAuthenticateData) {
        return;
    }

    // Active slot only; do not clobber a prior successful Update auth record.
    AuthenticateData authenticate_data{};
    authenticate_data.ap_metadata_tbs_data = tbs_data;
    authenticate_data.ap_auth_result       = nv::spdm::crypto::CryptoStatus::ApAuthInProgress;
    (void)write_authenticate_data(
        ap_id, nv::flash::Key::NpdsActiveApFwAuthenticateData, authenticate_data);
}

}  // namespace

void SecureBoot::clear_auth_result_ready() const
{
    auto& event = nv::ipc::Event::make(SecureBootEventId);
    clear_event_bits(ap_id(), event, EventBits::AuthResultReady);
}

bool SecureBoot::receive_auth_result()
{
    auto& queue = nv::ipc::Queue::make(nv::ipc::QueueId::SecureBootAuthResult);
    // NOLINTNEXTLINE(misc-const-correctness) queue.recv() writes through Queue::Item.
    SecureBootAuthResult auth_result{};
    auto                 item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&auth_result),
                                     sizeof(auth_result));

    const auto status = queue.recv(item, std::chrono::microseconds{0});
    if (status == nv::ipc::Queue::Status::Empty || status == nv::ipc::Queue::Status::Timeout) {
        return false;
    }
    if (status != nv::ipc::Queue::Status::Ok) {
        log_queue_recv_fail(ap_id(), status);
        return false;
    }
    if (auth_result.ap_id != ap_id() || auth_result.slot != current_slot
        || auth_result.auth_request_id != auth_request_id) {
        return false;
    }

    this->auth_result = auth_result.result;
    return true;
}

void SecureBoot::set_status(nv::fw_parser::ap::ApFwStatus status) const
{
    const auto id  = ap_id();
    const auto key = id == 0 ? nv::flash::Key::NpdsAp0FwStatus
                   : id == 1 ? nv::flash::Key::NpdsAp1FwStatus
                             : nv::flash::Key::NpdsInvalid;
    if (key == nv::flash::Key::NpdsInvalid) {
        return;
    }

    const auto flash_status = nv::flash::Flash::set_data(key,
                                                         static_cast<nv::flash::Data>(status));
    if (flash_status != nv::flash::Status::Ok) {
        log_flash_write_fail(id, key, flash_status);
    }
}

// Preserve TBS data and only mark the auth result as failed.
void SecureBoot::mark_auth_result_failed()
{
    const auto key      = current_slot == nv::vrot::inactive_slot(ap_info)
                            ? nv::flash::Key::NpdsInactiveApFwAuthenticateData
                            : nv::flash::Key::NpdsActiveApFwAuthenticateData;
    const auto npds_key = authenticate_data_key_for_ap(ap_id(), key);

    auto& fail_data        = authenticate_data_scratch;
    fail_data              = AuthenticateData{};
    const auto read_status = nv::flash::Flash::get_ap_fw_authenticate_data(fail_data, npds_key);
    if (read_status != nv::flash::Status::Ok) {
        fail_data = AuthenticateData{};
    }
    fail_data.ap_auth_result = nv::spdm::crypto::CryptoStatus::FailUnknown;

    const auto flash_status = nv::flash::Flash::set_ap_fw_authenticate_data(fail_data,
                                                                            npds_key);
    if (flash_status != nv::flash::Status::Ok) {
        log_flash_write_fail(ap_id(), npds_key, flash_status);
    }
}

bool SecureBoot::clear_stale_update_auth_data()
{
    // Clear only when Update and Active share the same slot.
    if (nv::vrot::update_slot(ap_info) != nv::vrot::active_slot(ap_info)) {
        return true;
    }

    authenticate_data_scratch = AuthenticateData{};
    return write_authenticate_data(ap_id(),
                                   nv::flash::Key::NpdsUpdateApFwAuthenticateData,
                                   authenticate_data_scratch)
        == nv::flash::Status::Ok;
}

void SecureBoot::load_auth_result_from_update_record()
{
    authenticate_data_scratch = AuthenticateData{};
    if (nv::flash::Flash::get_ap_fw_authenticate_data(
            authenticate_data_scratch,
            authenticate_data_key_for_ap(ap_id(),
                                         nv::flash::Key::NpdsUpdateApFwAuthenticateData))
        != nv::flash::Status::Ok) {
        return;
    }

    if (authenticate_data_scratch.ap_auth_result != nv::spdm::crypto::CryptoStatus::Success) {
        return;
    }

    // Record the reused Update result as the Active authentication result.
    if (persist_authenticate_result(ap_id(),
                                    nv::flash::Key::NpdsActiveApFwAuthenticateData,
                                    authenticate_data_scratch.ap_auth_result,
                                    authenticate_data_scratch.ap_metadata_tbs_data)
        != nv::flash::Status::Ok) {
        return;
    }

    auth_result = authenticate_data_scratch.ap_auth_result;
}

void SecureBoot::wait_and_handle_idle_event()
{
    if (ap_reset_pending) {
        nv::flash::ProgressPercent progress{};
        const auto copy_status    = nv::vrot::ap_background_copy::query(ap_info, progress);
        const bool copy_timed_out = copy_status == nv::flash::Status::BackgroundCopyInprogress
                                 && background_copy_timed_out(ap_info,
                                                              *background_copy_started_ticks);

        if (copy_status == nv::flash::Status::BackgroundCopyInprogress && !copy_timed_out) {
            nv::ipc::Supervisor::inst().current_task().delay(BackgroundCopyPollInterval);
            return;
        }
        if (copy_timed_out) {
            nv::vrot::ap_background_copy::cancel(true);
        }
        ap_reset_pending = false;
        background_copy_started_ticks.reset();
        recovery_retries = 0;
        restart_secure_boot();
        return;
    }

    auto&      event       = nv::ipc::Event::make(SecureBootEventId);
    const auto active_bits = wait_event_bits(event, IdleWaitBits, IdleEventWaitTimeout);

    if ((active_bits & EventBits::AuthResultReady) != 0U) {
        clear_event_bits(ap_id(), event, EventBits::AuthResultReady);
        drain_auth_result_queue();
    }

    if ((active_bits & EventBits::ApResetRequested) == 0U) {
        return;
    }

    handle_ap_reset_request();
}

void SecureBoot::handle_ap_reset_request()
{
    auto& event = nv::ipc::Event::make(SecureBootEventId);
    clear_event_bits(ap_id(), event, EventBits::ApResetRequested);
    // Finish the current background copy before starting a new authentication flow.
    if (nv::vrot::ap_background_copy::is_in_progress()) {
        ap_reset_pending              = true;
        background_copy_started_ticks = nv::ipc::Supervisor::get_os_ticks();
        if (state != State::Idle) {
            // Idle's ap_reset_pending path polls the copy to completion.
            set_state(State::Idle);
        }
        return;
    }
    recovery_retries = 0;
    restart_secure_boot();
}

void SecureBoot::notify_ap_reset(const nv::vrot::ApInfo& ap)
{
    auto&      event  = nv::ipc::Event::make(SecureBootEventId);
    const auto status = event.set(EventBits::ApResetRequested, nv::ipc::CoreId::Core0);
    if (status != nv::ipc::Event::Status::Ok) {
        log_event_set_fail(static_cast<uint8_t>(ap.id), EventBits::ApResetRequested, status);
    }
}

void SecureBoot::set_state(State new_state)
{
    nv::logger::info(nv::logger::Event::SecureBootStateTransition,
                     nv::logger::EventData{
                         nv::common::to_underlying(state),
                         nv::common::to_underlying(new_state),
                     });
    state = new_state;
}

void SecureBoot::log_fatal_state() const
{
    const uint32_t reason_data = static_cast<uint32_t>(recovery_failure_reason)
                               | (static_cast<uint32_t>(auth_result) << 8U);
    const uint32_t context_data = static_cast<uint32_t>(ap_id())
                                | (static_cast<uint32_t>(current_slot) << 8U)
                                | (static_cast<uint32_t>(recovery_retries) << 16U);
    nv::logger::error_no_wait(nv::logger::Event::SecureBootFatal,
                              nv::logger::data_from_two_u32(reason_data, context_data));
}

void SecureBoot::enter_fatal(FailureReason reason, nv::fw_parser::ap::ApFwStatus status)
{
    set_status(status);
    recovery_failure_reason = reason;
    log_fatal_state();
    set_state(State::Fatal);
}

void SecureBoot::start_recovery_slot_authentication()
{
    current_slot           = nv::vrot::inactive_slot(ap_info);
    auth_result            = nv::spdm::crypto::CryptoStatus::FailUnknown;
    auth_request_id        = 0U;
    post_authenticate_done = false;

    set_status(nv::fw_parser::ap::ApFwStatus::Auth_In_Progress);
    clear_auth_result_ready();
    drain_auth_result_queue();

    const auto send_status = nv::spdm::crypto::send_authenticate_firmware_request(
        ap_info, current_slot, auth_request_id, nv::ipc::TaskId::Begin);
    if (send_status != nv::spdm::crypto::CryptoStatus::Success) {
        auth_result = send_status;
    }
}

void SecureBoot::reset_attempt_state()
{
    if (background_copy_started_ticks && nv::vrot::ap_background_copy::is_in_progress()) {
        nv::vrot::ap_background_copy::cancel(true);
    }
    background_copy_started_ticks.reset();
    ap_reset_pending        = false;
    auth_result             = nv::spdm::crypto::CryptoStatus::FailUnknown;
    auth_request_id         = 0;
    pre_authenticate_done   = false;
    post_authenticate_done  = false;
    recovery_failure_reason = FailureReason::None;
    drain_auth_result_queue();
    auto& event = nv::ipc::Event::make(SecureBootEventId);
    // Clear attempt-local events; AP reset notifications stay pending.
    clear_event_bits(ap_id(), event, EventBits::AuthResultReady);
}

void SecureBoot::restart_secure_boot()
{
    current_slot = nv::vrot::active_slot(ap_info);
    reset_attempt_state();
    set_state(State::Initialization);
}

void SecureBoot::start_slot_authentication(Slot slot)
{
    current_slot           = slot;
    auth_result            = nv::spdm::crypto::CryptoStatus::FailUnknown;
    auth_request_id        = 0U;
    post_authenticate_done = false;
    set_state(State::OnAuthenticate);
}

[[noreturn]] void SecureBoot::run()
{
    using nv::vrot::ApOpErrCode;
    namespace ap_op = nv::vrot;

    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::SecureBoot);

    // coverity[no_escape]
    while (true) {
        switch (state) {
            case State::Initialization: {
                // Init-stage failures do not change persisted auth_data.
                if (ap_op::hold_reset(ap_info) != ApOpErrCode::Success) {
                    enter_fatal(FailureReason::Boot,
                                nv::fw_parser::ap::ApFwStatus::Boot_Failed);
                    break;
                }
                if (ap_op::pre_authenticate(ap_info) != ApOpErrCode::Success) {
                    enter_fatal(FailureReason::Boot,
                                nv::fw_parser::ap::ApFwStatus::Boot_Failed);
                    break;
                }
                pre_authenticate_done = true;
                set_state(State::OnAuthenticate);
                break;
            }

            case State::OnAuthenticate: {
                set_status(nv::fw_parser::ap::ApFwStatus::Auth_In_Progress);
                clear_auth_result_ready();
                drain_auth_result_queue();
                load_auth_result_from_update_record();
                set_state(State::AuthRequestPending);
                break;
            }

            case State::AuthRequestPending: {
                if (auth_result != nv::spdm::crypto::CryptoStatus::Success) {
                    const auto
                        send_status = nv::spdm::crypto::send_authenticate_firmware_request(
                            ap_info, current_slot, auth_request_id, nv::ipc::TaskId::Begin);
                    if (send_status != nv::spdm::crypto::CryptoStatus::Success) {
                        auth_result = send_status;
                        set_state(State::AuthFailed);
                        break;
                    }
                }
                set_state(State::AuthPending);
                break;
            }

            case State::AuthPending: {
                if (auth_result == nv::spdm::crypto::CryptoStatus::Success
                    || receive_auth_result()) {
                    clear_auth_result_ready();
                    drain_auth_result_queue();
                    if (auth_result != nv::spdm::crypto::CryptoStatus::Success) {
                        mark_auth_result_failed();
                        set_state(State::AuthFailed);
                        break;
                    }
                    if (current_slot == nv::vrot::inactive_slot(ap_info)) {
                        // Restore the authenticated inactive image into the active slot
                        // before the AP reset can be released.
                        set_state(State::Recovery);
                        break;
                    }
                    set_state(State::Release);
                    break;
                }

                auto&      event       = nv::ipc::Event::make(SecureBootEventId);
                const auto active_bits = wait_event_bits(
                    event, EventBits::AuthResultReady, AuthEventWaitTimeout);

                if ((active_bits & EventBits::AuthResultReady) != 0U) {
                    clear_event_bits(ap_id(), event, EventBits::AuthResultReady);
                    break;
                }

                break;
            }

            case State::Release: {
                const auto cb          = ap_op::post_authenticate(ap_info, auth_result);
                post_authenticate_done = true;
                if (cb != ApOpErrCode::Success
                    || ap_op::release_reset(ap_info) != ApOpErrCode::Success) {
                    set_state(State::BootFailed);
                    break;
                }
                boot_wait_started_ticks = nv::ipc::Supervisor::get_os_ticks();
                set_state(State::WaitBoot);
                break;
            }

            case State::WaitBoot: {
                const auto boot_status = ap_op::check_booted(ap_info);
                if (boot_status == ApOpErrCode::Success) {
                    set_state(State::BootComplete);
                }
                else if (boot_status != ApOpErrCode::Busy) {
                    set_state(State::BootFailed);
                }
                else if (boot_wait_timed_out(boot_wait_started_ticks)) {
                    set_state(State::BootFailed);
                }
                else {
                    auto&      event       = nv::ipc::Event::make(SecureBootEventId);
                    const auto active_bits = wait_event_bits(
                        event, EventBits::ApResetRequested, BootPollDelay);
                    if ((active_bits & EventBits::ApResetRequested) != 0U) {
                        handle_ap_reset_request();
                    }
                }
                break;
            }

            case State::BootComplete: {
                const auto post_boot_status = ap_op::post_boot_complete(ap_info);
                if (post_boot_status != ApOpErrCode::Success) {
                    set_state(State::BootFailed);
                    break;
                }
                set_status(nv::fw_parser::ap::ApFwStatus::Boot_Complete);
                nv::debugtoken::sync_debug_token_features_on_boot();
                set_state(State::Idle);
                break;
            }

            case State::AuthFailed: {
                if (pre_authenticate_done && !post_authenticate_done) {
                    const auto cb          = ap_op::post_authenticate(ap_info, auth_result);
                    post_authenticate_done = true;
                    if (cb != ApOpErrCode::Success) {
                        enter_fatal(FailureReason::Auth,
                                    nv::fw_parser::ap::ApFwStatus::Auth_Failed);
                        break;
                    }
                }
                set_status(nv::fw_parser::ap::ApFwStatus::Auth_Failed);
                recovery_failure_reason = FailureReason::Auth;
                set_state(State::Recovery);
                break;
            }

            case State::BootFailed: {
                // Auth signature was good (or skipped); the physical
                // release/boot step failed. Distinct status so observers
                // don't confuse this with a signature failure.
                set_status(nv::fw_parser::ap::ApFwStatus::Boot_Failed);
                recovery_failure_reason = FailureReason::Boot;
                set_state(State::Recovery);
                break;
            }

            case State::Recovery: {
                if (recovery_retries >= MaxRecoveryRetries) {
                    log_fatal_state();
                    set_state(State::Fatal);
                    break;
                }

                if (!nv::vrot::ap_background_copy::supports(ap_info)) {
                    const auto status = recovery_failure_reason == FailureReason::Auth
                                          ? nv::fw_parser::ap::ApFwStatus::Auth_Failed
                                          : nv::fw_parser::ap::ApFwStatus::Boot_Failed;
                    enter_fatal(recovery_failure_reason, status);
                    break;
                }

                // Phase 1: authenticate the inactive slot before it can restore the active
                // slot. A boot failure may have returned flash ownership to the AP.
                if (current_slot != nv::vrot::inactive_slot(ap_info)) {
                    if (ap_op::hold_reset(ap_info) != ApOpErrCode::Success
                        || ap_op::pre_authenticate(ap_info) != ApOpErrCode::Success) {
                        enter_fatal(FailureReason::Restore,
                                    nv::fw_parser::ap::ApFwStatus::Boot_Failed);
                        break;
                    }
                    pre_authenticate_done = true;
                    start_recovery_slot_authentication();
                    break;
                }

                // A nonzero request ID means the recovery-slot authentication is pending.
                if (auth_request_id != 0U) {
                    if (!receive_auth_result()) {
                        auto&      event       = nv::ipc::Event::make(SecureBootEventId);
                        const auto active_bits = wait_event_bits(
                            event, EventBits::AuthResultReady, AuthEventWaitTimeout);
                        if ((active_bits & EventBits::AuthResultReady) != 0U) {
                            clear_event_bits(ap_id(), event, EventBits::AuthResultReady);
                        }
                        break;
                    }

                    auth_request_id = 0U;
                    clear_auth_result_ready();
                    drain_auth_result_queue();
                    if (auth_result != nv::spdm::crypto::CryptoStatus::Success) {
                        mark_auth_result_failed();
                    }
                }

                if (auth_result != nv::spdm::crypto::CryptoStatus::Success) {
                    // The recovery slot itself failed authentication.
                    enter_fatal(FailureReason::Auth,
                                nv::fw_parser::ap::ApFwStatus::Auth_Failed);
                    break;
                }

                // Phase 2: restore the authenticated image into the active physical slot.
                if (!background_copy_started_ticks) {
                    if (!start_recovery_copy(ap_info,
                                             nv::vrot::inactive_slot(ap_info),
                                             nv::vrot::active_slot(ap_info))) {
                        enter_fatal(FailureReason::Restore,
                                    nv::fw_parser::ap::ApFwStatus::Boot_Failed);
                        break;
                    }
                    background_copy_started_ticks = nv::ipc::Supervisor::get_os_ticks();
                    break;
                }

                // Phase 3: poll until the restore completes, fails, or times out.
                nv::flash::ProgressPercent progress{};
                const auto copy_status = nv::vrot::ap_background_copy::query(ap_info, progress);
                if (copy_status == nv::flash::Status::BackgroundCopyDone) {
                    background_copy_started_ticks.reset();
                    if (!clear_stale_update_auth_data()) {
                        enter_fatal(FailureReason::Restore,
                                    nv::fw_parser::ap::ApFwStatus::Boot_Failed);
                        break;
                    }
                    // Background copy releases the SPI flash after the last chunk.
                    // Reacquire it before authenticating the restored active slot.
                    if (ap_op::pre_authenticate(ap_info) != ApOpErrCode::Success) {
                        enter_fatal(FailureReason::Restore,
                                    nv::fw_parser::ap::ApFwStatus::Boot_Failed);
                        break;
                    }
                    ++recovery_retries;
                    start_slot_authentication(nv::vrot::active_slot(ap_info));
                    break;
                }

                const bool copy_in_progress = copy_status
                                           == nv::flash::Status::BackgroundCopyInprogress;
                const bool copy_timed_out = copy_in_progress
                                         && background_copy_timed_out(
                                                ap_info, *background_copy_started_ticks);
                if (copy_in_progress && !copy_timed_out) {
                    nv::ipc::Supervisor::inst().current_task().delay(
                        BackgroundCopyPollInterval);
                    break;
                }

                if (copy_timed_out) {
                    nv::vrot::ap_background_copy::cancel(true);
                }
                nv::logger::error_no_wait(
                    nv::logger::Event::SecureBootSlotRestoreFail,
                    nv::logger::data_from_two_u32(
                        static_cast<uint32_t>(copy_timed_out ? nv::flash::Status::Timeout
                                                             : copy_status),
                        (static_cast<uint32_t>(progress) << 16U)
                            | (static_cast<uint32_t>(nv::vrot::inactive_slot(ap_info)) << 8U)
                            | ap_id()));
                background_copy_started_ticks.reset();
                enter_fatal(FailureReason::Restore, nv::fw_parser::ap::ApFwStatus::Boot_Failed);
                break;
            }

            case State::Fatal: {
                set_state(State::Idle);
                break;
            }

            case State::Idle: {
                wait_and_handle_idle_event();
                break;
            }
        }
    }
}

nv::spdm::crypto::CryptoStatus
SecureBoot::secure_boot_auth_callback(const uint8_t        ap_id,
                                      const Slot           slot,
                                      const nv::flash::Key authenticate_data_key,
                                      const nv::spdm::crypto::CryptoStatus ap_auth_result,
                                      const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data,
                                      nv::ipc::TaskId request_task_id,
                                      uint8_t         auth_request_id)
{
    if (is_secure_boot_request(request_task_id)) {
        auto callback_result = ap_auth_result;
        if (callback_result == nv::spdm::crypto::CryptoStatus::Success
            && authenticate_data_key != nv::flash::Key::NpdsInvalid
            && persist_authenticate_result(
                   ap_id, authenticate_data_key, callback_result, tbs_data)
                   != nv::flash::Status::Ok) {
            callback_result = nv::spdm::crypto::CryptoStatus::FailUnknown;
        }
        if (!send_auth_result(ap_id, slot, callback_result, auth_request_id)) {
            return nv::spdm::crypto::CryptoStatus::FailUnknown;
        }
        return callback_result;
    }

    auto callback_result = ap_auth_result;
    if (authenticate_data_key != nv::flash::Key::NpdsInvalid
        && persist_authenticate_result(ap_id, authenticate_data_key, ap_auth_result, tbs_data)
               != nv::flash::Status::Ok) {
        callback_result = nv::spdm::crypto::CryptoStatus::FailUnknown;
    }
    return callback_result;
}

void SecureBoot::persist_ap_fw_authenticate_data_in_progress(
    const uint8_t                                   ap_id,
    const nv::flash::Key                            authenticate_data_key,
    const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data,
    nv::ipc::TaskId                                 request_task_id)
{
    if (is_secure_boot_request(request_task_id)) {
        return;
    }
    persist_authenticate_data_in_progress(ap_id, authenticate_data_key, tbs_data);
}

}  // namespace nv::secure_boot
