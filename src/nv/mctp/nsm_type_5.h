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
#include <climits>
#include <cstdint>
#include <cstring>

namespace nv::mctp {

constexpr uint8_t NsmDevCfgSupportedErrorInjectionTypesNum = 8;
constexpr uint8_t NsmDevCfgCommonSupportedNum              = 16;

// NVIDIA TYPE 5 Device Configuration Command Code
enum class NsmDevCfgCmdCode : uint8_t
{
    SetErrorInjectionMode           = 0x03,
    GetErrorInjectionMode           = 0x04,
    GetSupportedErrorInjectionTypes = 0x05,
    SetCurrentErrorInjectionTypes   = 0x06,
    GetCurrentErrorInjectionTypes   = 0x07,
    GetErrorInjectionPayload        = 0x0A,
    SetErrorInjectionPayload        = 0x0B,
    ActivateErrorInjection          = 0x0C,
};

/** Type 5 Supported Error Injection Types v1 */
enum class NsmDevCfgSupportedErrorInjectionTypes : uint8_t
{
    FatalError = 0x4,  // Device Fatal error injection
};

/** Common Enable Disable enums */
enum NsmDevCfgEnablingMode : uint8_t
{
    Disable = 0x00,
    Enable  = 0x01,
};

enum EIPayloadList : uint8_t
{
    FatalFaultEI = 0x04
};

enum FatalFaultEIPayloadValues : uint32_t
{
    FatalFaultEIMCUException    = 0,  // bit 0
    FatalFaultEIWatchdogTimeout = 1,  // bit 1
    FatalFaultEIMask = (1u << FatalFaultEIMCUException) | (1u << FatalFaultEIWatchdogTimeout)
};

struct [[gnu::packed]] NsmDevCfgErrorInjectionModeResponse
{
    uint8_t  mode;                          //<! initial response
    uint32_t persistent_data_modified : 1;  //<! data modified by error injection
    uint32_t other_flags              : 31;
    // Creator
    NsmDevCfgErrorInjectionModeResponse()
    : mode{NsmDevCfgEnablingMode::Disable}
    , persistent_data_modified{0}
    , other_flags{0}
    {
        // Empty
    }
};

constexpr std::array<uint8_t, NsmDevCfgCommonSupportedNum> gen_type5_code_bitmask()
{
    std::array<uint8_t, NsmDevCfgCommonSupportedNum> bitmask = {0};

    constexpr uint8_t bit_positions[] = {
        static_cast<uint8_t>(NsmDevCfgCmdCode::SetErrorInjectionMode),
        static_cast<uint8_t>(NsmDevCfgCmdCode::GetErrorInjectionMode),
        static_cast<uint8_t>(NsmDevCfgCmdCode::GetSupportedErrorInjectionTypes),
        static_cast<uint8_t>(NsmDevCfgCmdCode::SetCurrentErrorInjectionTypes),
        static_cast<uint8_t>(NsmDevCfgCmdCode::GetCurrentErrorInjectionTypes),
        static_cast<uint8_t>(NsmDevCfgCmdCode::GetErrorInjectionPayload),
        static_cast<uint8_t>(NsmDevCfgCmdCode::SetErrorInjectionPayload),
        static_cast<uint8_t>(NsmDevCfgCmdCode::ActivateErrorInjection),
    };

    for (uint8_t pos : bit_positions) {
        const size_t byte_index = pos / 8;
        const size_t bit_offset = pos % 8;
        const size_t value      = (1u << bit_offset);
        if (value <= UCHAR_MAX) {
            bitmask.at(byte_index) |= static_cast<uint8_t>(value);
        }
    }

    return bitmask;
}

constexpr std::array<uint8_t, NsmDevCfgSupportedErrorInjectionTypesNum>
gen_type5_supported_errors_injection_bitmask()
{
    std::array<uint8_t, NsmDevCfgSupportedErrorInjectionTypesNum> bitmask = {0};

    constexpr uint8_t bit_positions[] = {
        static_cast<uint8_t>(NsmDevCfgSupportedErrorInjectionTypes::FatalError)};

    for (uint8_t pos : bit_positions) {
        const size_t byte_index = pos / 8;
        const size_t bit_offset = pos % 8;
        const size_t value      = (1u << bit_offset);
        if (value <= UCHAR_MAX) {
            bitmask.at(byte_index) |= static_cast<uint8_t>(value);
        }
    }
    return bitmask;
}

struct [[gnu::packed]] FatalFaultPayloadResponse
{
    uint32_t offset;
    uint32_t error_code;
    uint32_t bitmap;
    FatalFaultPayloadResponse() : offset{0}, error_code{FatalFaultEI}, bitmap{0} {}
};

/** single data structure for type persistent data */
struct [[gnu::packed]] NsmDevCfgPersistentData
{
    NsmDevCfgErrorInjectionModeResponse errorInjectionModeResponse;
    static constexpr std::array<uint8_t, NsmDevCfgCommonSupportedNum>
        suppCmdCode = gen_type5_code_bitmask();
    static constexpr std::array<uint8_t, NsmDevCfgSupportedErrorInjectionTypesNum>
        suppErrorTypes = gen_type5_supported_errors_injection_bitmask();
    std::array<uint8_t, NsmDevCfgSupportedErrorInjectionTypesNum>
        current_errors_injection_bitmask{};
    // keep General Error Injection Payload response: error and bitmap
    FatalFaultPayloadResponse fatalFaultResp;

    bool isErrorInjectionModeEnabled()
    {
        return errorInjectionModeResponse.mode == NsmDevCfgEnablingMode::Enable;
    }

    bool isErrorTypeEnabled(uint8_t error_type)
    {
        const uint8_t byte_index = error_type / 8;
        const uint8_t bit_offset = error_type % 8;
        if (byte_index < current_errors_injection_bitmask.size()) {
            if (current_errors_injection_bitmask.at(byte_index) & (1u << bit_offset)) {
                return true;
            }
        }
        return false;
    }

    void clearErrorInjection()
    {
        void* data = current_errors_injection_bitmask.data();
        std::memset(data, 0, current_errors_injection_bitmask.size());
        fatalFaultResp.bitmap = 0;
    }
};

namespace nsm_type5 {

/**
 * @brief This is the callback for the NSM T5 EI Payload Activate
 * @param bitmap - The NSM T5 EI Payload bitmap
 *                 It identifies which Fatal Fault is going to be launched
 */
void on_nsm_t5_fatal_fault_ei(uint8_t bitmap);

}  // namespace nsm_type5

}  // namespace nv::mctp
