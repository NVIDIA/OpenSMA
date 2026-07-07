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
#pragma once

#include "nv/secure_boot/authenticate_data.h"
#include "nv/vrot/interface/types.h"
#include "nv/spdm/crypto_status.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/common/utils.h"
#include "nv/common/preproc.h"

#include NV_IPC_CONFIG_H

namespace nv::secure_boot {

constexpr uint8_t MaxApCount = 2;

enum class State : uint8_t
{
    Initialization = 0,
    OnAuthenticate,
    AuthRequestPending,
    AuthPending,
    Release,
    WaitBoot,
    BootComplete,
    AuthFailed,
    BootFailed,
    Recovery,
    Fatal,
    Idle,
};

enum EventBits : uint32_t
{
    AuthResultReady  = nv::common::bit(0U),
    ApResetRequested = nv::common::bit(1U),
};

class SecureBoot
{
public:
    explicit SecureBoot(const nv::vrot::ApInfo& ap_info) : ap_info(ap_info) {}
    ~SecureBoot() = default;

    [[noreturn]] void run();

    static nv::spdm::crypto::CryptoStatus
    secure_boot_auth_callback(uint8_t                                         ap_index,
                              const nv::fw_parser::ap::ParsingApFwType        auth_slot,
                              const nv::spdm::crypto::CryptoStatus            ap_auth_result,
                              const nv::fw_parser::ap::ApFwMetadata::TbsData& authenticate_data,
                              nv::ipc::TaskId                                 request_task_id,
                              uint8_t                                         auth_request_id);

    // Persist Active-slot metadata before the slow per-image hash loop.
    static void persist_ap_fw_authenticate_data_in_progress(
        uint8_t                                         ap_index,
        nv::fw_parser::ap::ParsingApFwType              auth_slot,
        const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data,
        nv::ipc::TaskId                                 request_task_id);

    static void notify_ap_reset(const nv::vrot::ApInfo& ap);

private:
    enum class FailureReason : uint8_t
    {
        None = 0,
        Auth,
        Boot,
    };

    void set_state(State new_state);
    void reset_attempt_state();
    void clear_auth_result_ready() const;
    bool receive_auth_result();
    void set_status(nv::fw_parser::ap::ApFwStatus status) const;
    void mark_auth_result_failed();
    bool auth_timed_out() const;
    void wait_and_handle_idle_event();
    void log_fatal_state() const;

    uint8_t ap_index() const { return static_cast<uint8_t>(ap_info.id); }

    // Total attempts per boot = 1 + MaxRecoveryRetries.
    static constexpr uint8_t MaxRecoveryRetries = 1;

    const nv::vrot::ApInfo         ap_info;
    State                          state{State::Initialization};
    nv::spdm::crypto::CryptoStatus auth_result{nv::spdm::crypto::CryptoStatus::FailUnknown};
    AuthenticateData               authenticate_data_scratch{};
    uint8_t                        recovery_retries{0};
    FailureReason                  recovery_failure_reason{FailureReason::None};
    // Most-recent AP auth request id assigned by spdm_crypto_helper; used in
    // receive_auth_result() to match an incoming response to this attempt.
    // 0 means "no request issued yet" (assigned ids are 1..255).
    uint8_t                            auth_request_id{0};
    uint32_t                           auth_state_started_ticks{0};
    nv::fw_parser::ap::ParsingApFwType current_slot{
        nv::fw_parser::ap::ParsingApFwType::ActiveSlot};
    bool pre_authenticate_done{false};
    bool post_authenticate_done{false};
    bool complete_status_recorded{false};
};

}  // namespace nv::secure_boot
