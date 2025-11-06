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
#pragma once
#include <array>
#include <functional>
#include <span>
#include <stdint.h>

// include libspdm header
#include "industry_standard/spdm.h"
#include "internal/libspdm_responder_lib.h"
#include "library/spdm_common_lib.h"
#include "library/spdm_transport_mctp_lib.h"

namespace pdk::spdm::app::res::library {

// Error codes for spdm_engine_setting function
enum class SpdmEngineSettingError : uint32_t
{
    SUCCESS                               = 0,
    SPDM_CONTEXT_INIT_FAILED              = 1,
    SPDM_VERSION_SETTING_FAILED           = 2,
    CAPABILITY_CT_EXPONENT_SETTING_FAILED = 3,
    CAPABILITY_FLAGS_SETTING_FAILED       = 4,
    MEASUREMENT_SPEC_SETTING_FAILED       = 5,
    MEASUREMENT_HASH_ALGO_SETTING_FAILED  = 6,
    BASE_ASYM_ALGO_SETTING_FAILED         = 7,
    BASE_HASH_ALGO_SETTING_FAILED         = 8,
    DHE_NAME_GROUP_SETTING_FAILED         = 9,
    AEAD_CIPHER_SUITE_SETTING_FAILED      = 10,
    REQ_BASE_ASYM_ALG_SETTING_FAILED      = 11,
    KEY_SCHEDULE_SETTING_FAILED           = 12,
    CERTIFICATE_CHAIN_SETTING_FAILED      = 13,
    NO_SPDM_VERSION_SUPPORTED             = 14,
    SPDM_CONTEXT_ALLOCATE_FAILED          = 15,
    SPDM_SCRATCH_BUFFER_ALLOCATE_FAILED   = 16
};

struct SpdmEngineContext
{
    void*  spdm_context_ptr;
    size_t spdm_context_size;
    void*  spdm_scratch_buffer_ptr;
    size_t spdm_scratch_buffer_size;

    // the function is used to set the spdm engine context
    SpdmEngineSettingError spdm_engine_initialize();

    // the function is used to process the spdm message
    libspdm_return_t process_message();
};

}  // namespace pdk::spdm::app::res::library
