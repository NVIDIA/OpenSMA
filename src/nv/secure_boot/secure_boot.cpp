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
#include "nv/logger/log.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"

namespace nv::secure_boot {

namespace {
struct SecureBootAuthResult
{
    uint8_t                            ap_index{};
    nv::fw_parser::ap::ParsingApFwType slot{nv::fw_parser::ap::ParsingApFwType::End};
    nv::spdm::crypto::CryptoStatus     result{nv::spdm::crypto::CryptoStatus::FailUnknown};
    uint8_t                            auth_request_id{};
};

static_assert(sizeof(SecureBootAuthResult) == nv::ipc::SecureBootAuthResultQueueSize);

constexpr auto SecureBootTaskId  = nv::ipc::TaskId::SecureBoot;
constexpr auto SecureBootEventId = nv::ipc::EventId::SecureBootTask;
static_assert(nv::vrot::ApList.size() <= 1,
              "SecureBoot uses one AuthResultReady bit; multi-AP needs per-AP signaling");
// Cleared between authentication attempts and used as the Idle wait mask.
constexpr nv::ipc::Event::Bits PendingEventBits = EventBits::AuthResultReady
                                                | EventBits::ApResetRequested;
constexpr nv::ipc::Event::Bits AuthPendingWaitBits = EventBits::AuthResultReady;
constexpr nv::ipc::Event::Bits IdleWaitBits        = PendingEventBits;
constexpr uint32_t AuthTimeoutTicks     = static_cast<uint32_t>(configTICK_RATE_HZ) * 120U;
constexpr auto     AuthEventWaitTimeout = std::chrono::milliseconds{10};
constexpr auto     IdleEventWaitTimeout = std::chrono::seconds{60};

bool is_secure_boot_request(nv::ipc::TaskId request_task_id)
{
    return request_task_id == SecureBootTaskId;
}

void log_flash_write_fail(uint8_t ap_index, nv::flash::Key key, nv::flash::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootFlashWriteFail,
        nv::logger::data_from_two_u32(static_cast<uint32_t>(key),
                                      (static_cast<uint32_t>(status) << 8U) | ap_index));
}

void log_event_set_fail(uint8_t                ap_index,
                        nv::ipc::Event::Bits   bits,
                        nv::ipc::Event::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootEventSetFail,
        nv::logger::data_from_two_u32(bits, (static_cast<uint32_t>(status) << 8U) | ap_index));
}

void log_queue_send_fail(uint8_t ap_index, nv::ipc::Queue::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootQueueSendFail,
        nv::logger::data_from_u32((static_cast<uint32_t>(status) << 8U) | ap_index));
}

void log_queue_recv_fail(uint8_t ap_index, nv::ipc::Queue::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootQueueRecvFail,
        nv::logger::data_from_u32((static_cast<uint32_t>(status) << 8U) | ap_index));
}

void signal_auth_result_ready(uint8_t ap_index)
{
    auto&      event  = nv::ipc::Event::make(SecureBootEventId);
    const auto status = event.set(EventBits::AuthResultReady, nv::ipc::CoreId::Core0);
    if (status != nv::ipc::Event::Status::Ok) {
        log_event_set_fail(ap_index, EventBits::AuthResultReady, status);
    }
}

void log_event_clear_fail(uint8_t                ap_index,
                          nv::ipc::Event::Bits   bits,
                          nv::ipc::Event::Status status)
{
    nv::logger::error_no_wait(
        nv::logger::Event::SecureBootEventClearFail,
        nv::logger::data_from_two_u32(bits, (static_cast<uint32_t>(status) << 8U) | ap_index));
}

void clear_event_bits(uint8_t ap_index, nv::ipc::Event& event, nv::ipc::Event::Bits bits)
{
    const auto status = event.clear(bits);
    if (status != nv::ipc::Event::Status::Ok) {
        log_event_clear_fail(ap_index, bits, status);
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

bool send_auth_result(uint8_t                            ap_index,
                      nv::fw_parser::ap::ParsingApFwType slot,
                      nv::spdm::crypto::CryptoStatus     result,
                      uint8_t                            auth_request_id)
{
    auto& queue = nv::ipc::Queue::make(nv::ipc::QueueId::SecureBootAuthResult);

    const SecureBootAuthResult auth_result{
        .ap_index        = ap_index,
        .slot            = slot,
        .result          = result,
        .auth_request_id = auth_request_id,
    };
    auto item = nv::ipc::Queue::ConstItem(std::bit_cast<const uint8_t*>(&auth_result),
                                          sizeof(auth_result));

    const auto status = queue.send(item, std::chrono::microseconds{0});
    if (status != nv::ipc::Queue::Status::Ok) {
        log_queue_send_fail(ap_index, status);
        return false;
    }
    signal_auth_result_ready(ap_index);
    return true;
}

nv::flash::Status write_authenticate_data(uint8_t                 ap_index,
                                          nv::flash::Key          key,
                                          const AuthenticateData& authenticate_data)
{
    const auto flash_status = nv::flash::Flash::set_ap_fw_authenticate_data(authenticate_data,
                                                                            key);
    if (flash_status != nv::flash::Status::Ok) {
        log_flash_write_fail(ap_index, key, flash_status);
    }
    return flash_status;
}

nv::flash::Status persist_authenticate_data(uint8_t                            ap_index,
                                            nv::fw_parser::ap::ParsingApFwType auth_slot,
                                            const AuthenticateData& authenticate_data,
                                            AuthenticateData&       existing_update)
{
    nv::flash::Status flash_status = nv::flash::Status::Ok;
    if (auth_slot == nv::fw_parser::ap::ParsingApFwType::ActiveSlot) {
        flash_status = write_authenticate_data(
            ap_index, nv::flash::Key::NpdsActiveApFwAuthenticateData, authenticate_data);

        // Seed Update from Active only if Update has no successful auth record.
        if (flash_status == nv::flash::Status::Ok) {
            existing_update        = AuthenticateData{};
            const auto read_status = nv::flash::Flash::get_ap_fw_authenticate_data(
                existing_update, nv::flash::Key::NpdsUpdateApFwAuthenticateData);
            const bool
                update_already_authenticated = (read_status == nv::flash::Status::Ok)
                                            && (existing_update.ap_auth_result
                                                == nv::spdm::crypto::CryptoStatus::Success);
            // Busy means PLDM may be staging Update; don't seed over that write.
            if (read_status != nv::flash::Status::Busy && !update_already_authenticated) {
                flash_status = write_authenticate_data(
                    ap_index,
                    nv::flash::Key::NpdsUpdateApFwAuthenticateData,
                    authenticate_data);
            }
        }
    }
    else if (auth_slot == nv::fw_parser::ap::ParsingApFwType::UpdateSlot) {
        flash_status = write_authenticate_data(
            ap_index, nv::flash::Key::NpdsUpdateApFwAuthenticateData, authenticate_data);
    }

    return flash_status;
}

nv::flash::Status
persist_authenticate_result(uint8_t                                         ap_index,
                            nv::fw_parser::ap::ParsingApFwType              auth_slot,
                            nv::spdm::crypto::CryptoStatus                  ap_auth_result,
                            const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data)
{
    AuthenticateData authenticate_data{};
    authenticate_data.ap_metadata_tbs_data = tbs_data;
    authenticate_data.ap_auth_result       = ap_auth_result;
    AuthenticateData existing_update{};

    return persist_authenticate_data(ap_index, auth_slot, authenticate_data, existing_update);
}

void persist_authenticate_data_in_progress(
    uint8_t                                         ap_index,
    nv::fw_parser::ap::ParsingApFwType              auth_slot,
    const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data)
{
    if (auth_slot != nv::fw_parser::ap::ParsingApFwType::ActiveSlot) {
        return;
    }

    // Active slot only; do not clobber a prior successful Update auth record.
    AuthenticateData authenticate_data{};
    authenticate_data.ap_metadata_tbs_data = tbs_data;
    authenticate_data.ap_auth_result       = nv::spdm::crypto::CryptoStatus::ApAuthInProgress;
    (void)write_authenticate_data(
        ap_index, nv::flash::Key::NpdsActiveApFwAuthenticateData, authenticate_data);
}

}  // namespace

void SecureBoot::clear_auth_result_ready() const
{
    auto& event = nv::ipc::Event::make(SecureBootEventId);
    clear_event_bits(ap_index(), event, EventBits::AuthResultReady);
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
        log_queue_recv_fail(ap_index(), status);
        return false;
    }
    if (auth_result.ap_index != ap_index() || auth_result.slot != current_slot
        || auth_result.auth_request_id != auth_request_id) {
        return false;
    }

    this->auth_result = auth_result.result;
    return true;
}

void SecureBoot::set_status(nv::fw_parser::ap::ApFwStatus status) const
{
    const auto idx = ap_index();
    const auto key = idx == 0 ? nv::flash::Key::NpdsAp0FwStatus
                   : idx == 1 ? nv::flash::Key::NpdsAp1FwStatus
                              : nv::flash::Key::NpdsInvalid;
    if (key == nv::flash::Key::NpdsInvalid) {
        return;
    }

    const auto flash_status = nv::flash::Flash::set_data(key,
                                                         static_cast<nv::flash::Data>(status));
    if (flash_status != nv::flash::Status::Ok) {
        log_flash_write_fail(idx, key, flash_status);
    }
}

// Preserve TBS data and only mark the auth result as failed.
void SecureBoot::mark_auth_result_failed()
{
    const auto key = (current_slot == nv::fw_parser::ap::ParsingApFwType::UpdateSlot)
                       ? nv::flash::Key::NpdsUpdateApFwAuthenticateData
                       : nv::flash::Key::NpdsActiveApFwAuthenticateData;

    auto& fail_data        = authenticate_data_scratch;
    fail_data              = AuthenticateData{};
    const auto read_status = nv::flash::Flash::get_ap_fw_authenticate_data(fail_data, key);
    if (read_status != nv::flash::Status::Ok) {
        fail_data = AuthenticateData{};
    }
    fail_data.ap_auth_result = nv::spdm::crypto::CryptoStatus::FailUnknown;

    const auto flash_status = nv::flash::Flash::set_ap_fw_authenticate_data(fail_data, key);
    if (flash_status != nv::flash::Status::Ok) {
        log_flash_write_fail(ap_index(), key, flash_status);
    }
}

bool SecureBoot::auth_timed_out() const
{
    return (nv::ipc::Supervisor::get_os_ticks() - auth_state_started_ticks) >= AuthTimeoutTicks;
}

void SecureBoot::wait_and_handle_idle_event()
{
    auto&      event       = nv::ipc::Event::make(SecureBootEventId);
    const auto active_bits = wait_event_bits(event, IdleWaitBits, IdleEventWaitTimeout);
    if ((active_bits & EventBits::AuthResultReady) != 0U) {
        clear_event_bits(ap_index(), event, EventBits::AuthResultReady);
        drain_auth_result_queue();
    }
    if ((active_bits & EventBits::ApResetRequested) == 0U) {
        return;
    }

    recovery_retries = 0;
    current_slot     = nv::fw_parser::ap::ParsingApFwType::ActiveSlot;
    reset_attempt_state();
    set_state(State::Initialization);
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
    const uint32_t context_data = static_cast<uint32_t>(ap_index())
                                | (static_cast<uint32_t>(current_slot) << 8U)
                                | (static_cast<uint32_t>(recovery_retries) << 16U);
    nv::logger::error_no_wait(nv::logger::Event::SecureBootFatal,
                              nv::logger::data_from_two_u32(reason_data, context_data));
}

void SecureBoot::reset_attempt_state()
{
    auth_result              = nv::spdm::crypto::CryptoStatus::FailUnknown;
    auth_request_id          = 0;
    auth_state_started_ticks = 0;
    pre_authenticate_done    = false;
    post_authenticate_done   = false;
    complete_status_recorded = false;
    recovery_failure_reason  = FailureReason::None;
    drain_auth_result_queue();
    auto& event = nv::ipc::Event::make(SecureBootEventId);
    clear_event_bits(ap_index(), event, PendingEventBits);
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
                    set_state(State::AuthFailed);
                    break;
                }
                if (ap_op::pre_authenticate(ap_info) != ApOpErrCode::Success) {
                    set_state(State::AuthFailed);
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
                auth_state_started_ticks = nv::ipc::Supervisor::get_os_ticks();
                set_state(State::AuthRequestPending);
                break;
            }

            case State::AuthRequestPending: {
                const auto send_status = nv::spdm::crypto::send_authenticate_firmware_request(
                    ap_info, current_slot, auth_request_id, nv::ipc::TaskId::Begin);
                if (send_status != nv::spdm::crypto::CryptoStatus::Success) {
                    auth_result = send_status;
                    set_state(State::AuthFailed);
                    break;
                }
                auth_state_started_ticks = nv::ipc::Supervisor::get_os_ticks();
                set_state(State::AuthPending);
                break;
            }

            case State::AuthPending: {
                if (receive_auth_result()) {
                    clear_auth_result_ready();
                    drain_auth_result_queue();
                    if (auth_result == nv::spdm::crypto::CryptoStatus::Success) {
                        set_state(State::Release);
                    }
                    else {
                        mark_auth_result_failed();
                        set_state(State::AuthFailed);
                    }
                    break;
                }

                auto&      event       = nv::ipc::Event::make(SecureBootEventId);
                const auto active_bits = wait_event_bits(
                    event, AuthPendingWaitBits, AuthEventWaitTimeout);

                if ((active_bits & EventBits::AuthResultReady) != 0U) {
                    clear_event_bits(ap_index(), event, EventBits::AuthResultReady);
                    break;
                }

                if (auth_timed_out()) {
                    auth_result = nv::spdm::crypto::CryptoStatus::FailUnknown;
                    mark_auth_result_failed();
                    set_state(State::AuthFailed);
                }
                break;
            }

            case State::Release: {
                auto cb                = ap_op::post_authenticate(ap_info, auth_result);
                post_authenticate_done = true;
                // Post-auth physical failures are boot failures, not auth failures.
                if (cb != ApOpErrCode::Success
                    || ap_op::release_reset(ap_info) != ApOpErrCode::Success) {
                    set_state(State::BootFailed);
                }
                else {
                    set_state(State::WaitBoot);
                }
                break;
            }

            case State::WaitBoot: {
                if (ap_op::check_booted(ap_info) != ApOpErrCode::Success) {
                    // TODO: Add a timeout and set the state to BootFailed if the timeout is
                    // reached
                    set_state(State::BootFailed);
                }
                else {
                    set_state(State::BootComplete);
                }
                break;
            }

            case State::BootComplete: {
                if (!complete_status_recorded) {
                    set_status(nv::fw_parser::ap::ApFwStatus::Boot_Complete);
                    nv::debugtoken::sync_debug_token_features_on_boot();
                    complete_status_recorded = true;
                }
                set_state(State::Idle);
                break;
            }

            case State::AuthFailed: {
                if (pre_authenticate_done && !post_authenticate_done) {
                    (void)ap_op::post_authenticate(ap_info, auth_result);
                    post_authenticate_done = true;
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
                current_slot = (current_slot == nv::fw_parser::ap::ParsingApFwType::ActiveSlot)
                                 ? nv::fw_parser::ap::ParsingApFwType::UpdateSlot
                                 : nv::fw_parser::ap::ParsingApFwType::ActiveSlot;
                ++recovery_retries;
                reset_attempt_state();
                set_state(State::Initialization);
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
SecureBoot::secure_boot_auth_callback(const uint8_t                            ap_index,
                                      const nv::fw_parser::ap::ParsingApFwType auth_slot,
                                      const nv::spdm::crypto::CryptoStatus     ap_auth_result,
                                      const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data,
                                      nv::ipc::TaskId request_task_id,
                                      uint8_t         auth_request_id)
{
    if (is_secure_boot_request(request_task_id)) {
        auto callback_result = ap_auth_result;
        if (callback_result == nv::spdm::crypto::CryptoStatus::Success
            && persist_authenticate_result(ap_index, auth_slot, callback_result, tbs_data)
                   != nv::flash::Status::Ok) {
            callback_result = nv::spdm::crypto::CryptoStatus::FailUnknown;
        }
        if (!send_auth_result(ap_index, auth_slot, callback_result, auth_request_id)) {
            return nv::spdm::crypto::CryptoStatus::FailUnknown;
        }
        return callback_result;
    }

    auto callback_result = ap_auth_result;
    if (persist_authenticate_result(ap_index, auth_slot, ap_auth_result, tbs_data)
        != nv::flash::Status::Ok) {
        callback_result = nv::spdm::crypto::CryptoStatus::FailUnknown;
    }
    return callback_result;
}

void SecureBoot::persist_ap_fw_authenticate_data_in_progress(
    const uint8_t                                   ap_index,
    const nv::fw_parser::ap::ParsingApFwType        auth_slot,
    const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data,
    nv::ipc::TaskId                                 request_task_id)
{
    if (is_secure_boot_request(request_task_id)) {
        return;
    }
    persist_authenticate_data_in_progress(ap_index, auth_slot, tbs_data);
}

}  // namespace nv::secure_boot
