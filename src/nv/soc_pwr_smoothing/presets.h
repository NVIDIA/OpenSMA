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
#include <cstdint>
#include "nv/mctp/nsm_type_ff.h"

namespace nv::soc_pwr_smoothing {

// Preset configuration constants
constexpr uint8_t NUM_PRESETS         = 4;
constexpr uint8_t DEFAULT_PRESET      = 0;  // Read-only default preset
constexpr uint8_t MIN_WRITABLE_PRESET = 1;  // First writable preset
constexpr uint8_t MAX_WRITABLE_PRESET = 3;  // Last writable preset

// Array size = MaxParamCount (43)
// Note: Array indices 20-39 are reserved gap (unused, will be zero)
constexpr uint8_t PARAM_COUNT = static_cast<uint8_t>(
    nv::mctp::RackPwrSmoothParams::MaxParamCount);  // 43 total (includes gap)

// Validation helper - checks if param_id is valid (not in reserved gap)
constexpr bool is_valid_param_id(uint8_t param_id)
{
    return (param_id < static_cast<uint8_t>(nv::mctp::RackPwrSmoothParams::MaxTuningParams))
        || (param_id >= static_cast<uint8_t>(nv::mctp::RackPwrSmoothParams::TestHookParamsStart)
            && param_id < static_cast<uint8_t>(nv::mctp::RackPwrSmoothParams::MaxParamCount));
}

// Override parameter structure (used for parameters 40-42)
// Wire format: {uint8 is_override, uint8 reserved, uint16 value_scaled_by_100}
// Example: value=7550 represents 75.50%
struct OverrideParam
{
    uint8_t  is_override;  // 0 = disabled, 1 = enabled
    uint8_t  reserved;     // Reserved for future use
    uint16_t value;        // Actual value scaled by 100 (e.g., 7550 = 75.50%)

    // Pack into uint32_t for transmission
    uint32_t to_uint32() const
    {
        return (static_cast<uint32_t>(is_override) << 24)
             | (static_cast<uint32_t>(reserved) << 16) | static_cast<uint32_t>(value);
    }

    // Unpack from uint32_t received via MCTP
    static OverrideParam from_uint32(uint32_t raw)
    {
        return OverrideParam{.is_override = static_cast<uint8_t>((raw >> 24) & 0xFF),
                             .reserved    = static_cast<uint8_t>((raw >> 16) & 0xFF),
                             .value       = static_cast<uint16_t>(raw & 0xFFFF)};
    }
};

// Raw parameter storage (as received via MCTP)
// Parameters 0-19: SFXP22_10 encoded (cast to/from uint32_t for transmission)
// Parameters 20-39: RESERVED (gap, initialized to zero, not accessible)
// Parameters 40-42: OverrideParam encoded as uint32_t (use
// OverrideParam::to_uint32/from_uint32)
struct ParameterPreset
{
    std::array<uint32_t, PARAM_COUNT> params;           // All 23 parameters as uint32_t
    bool                              is_valid{false};  // Has this preset been initialized?
};

// Global preset manager
// Manages all 4 presets and tracks active/pending preset switches
struct PresetManager
{
    std::array<ParameterPreset, NUM_PRESETS> presets;  // All preset storage
    uint8_t active_preset_id{DEFAULT_PRESET};          // Currently active preset
    uint8_t pending_preset_id{0xFF};                   // 0xFF = no pending change
    bool    apply_pending{false};  // Flag to apply pending preset at ISR start
};

}  // namespace nv::soc_pwr_smoothing
