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
 * See the License for the license governing permissions and limitations
 * under the License.
 */

#pragma once

#include <array>
#include <cstdint>

#include "nv/mctp/nsm_type_ff.h"
#include "nv/soc_pwr_smoothing/runtime_cfg.h"

namespace nv::soc_pwr_smoothing {

// Preset configuration constants
constexpr uint8_t  NUM_PRESETS         = 4;
constexpr uint8_t  DEFAULT_PRESET      = 0;  // Read-only default preset
constexpr uint8_t  MIN_WRITABLE_PRESET = 1;  // First writable preset
constexpr uint8_t  MAX_WRITABLE_PRESET = 3;  // Last writable preset
constexpr uint8_t  INVALID_PRESET_ID   = 0xFF;
constexpr uint16_t MIN_DT_SCALER       = 1U;
constexpr uint16_t MAX_DT_SCALER       = 1000U;

// Array size = RackPwrSmoothParams::MaxParamCount (45); indices 20-39 are reserved (zero).
constexpr uint8_t PARAM_COUNT = static_cast<uint8_t>(
    nv::mctp::RackPwrSmoothParams::MaxParamCount);

// Validation helper - checks if param_id is valid (not in reserved gap)
constexpr bool is_valid_param_id(uint8_t param_id)
{
    return (param_id < static_cast<uint8_t>(nv::mctp::RackPwrSmoothParams::MaxTuningParams))
        || (param_id >= static_cast<uint8_t>(nv::mctp::RackPwrSmoothParams::TestHookParamsStart)
            && param_id < static_cast<uint8_t>(nv::mctp::RackPwrSmoothParams::MaxParamCount));
}

// Override parameter wire format (TestHook 40-44): {uint8 is_override, uint8 reserved, uint16
// value}. Params 40-42: value = percent x 100 (0-10000). Params 43-44: value = 12-bit DAC code;
// reserved must be 0.
struct OverrideParam
{
    uint8_t  is_override;
    uint8_t  reserved;
    uint16_t value;

    uint32_t             to_uint32() const;
    static OverrideParam from_uint32(uint32_t raw);
};

// Min/max limits for tuning parameters (SFXP22_10 or float wire encoding).
struct ParameterConstraints
{
    nv::mctp::RackPwrSmoothParams id;
    float                         min_value;
    float                         max_value;
};

// Validate a single rack power smoothing parameter (wire value).
bool validate_parameter(nv::mctp::RackPwrSmoothParams param_id, uint32_t raw_value);

class PowerManager;

// Manages all presets (RuntimeCfg per preset) and pending/active preset state.
class PresetManager
{
public:
    PresetManager() = default;

    // Initialize preset 0 with default RuntimeCfg; mark valid.
    void InitializeDefaultPreset();

    // Copy preset 0 to preset id; mark valid. No-op for preset 0 or invalid id.
    void InitializePresetFromDefault(uint8_t preset_id);

    // Clear pending preset (e.g. at startup).
    void ClearPending();

    // True if a preset switch is pending (to be applied at next ISR).
    bool ApplyPending() const { return apply_pending_; }

    // Apply pending preset to pm (config + resets), then clear pending.
    void ApplyPresetChange(PowerManager& pm);

    // Return preset cfg with runtime-controlled fields overlaid from current.
    RuntimeCfg GetCfgForApply(uint8_t preset_id, const RuntimeCfg& current) const;

    // Validate all params in the preset (for safety before switch).
    bool ValidatePreset(uint8_t preset_id) const;

    // Set one parameter; returns false if invalid. Initializes preset from default if needed.
    bool SetParameter(uint8_t preset_id, uint8_t param_id, uint32_t value);

    // Fill out_params from preset (wire format). Fills 0 if preset invalid.
    void GetPresetParameters(uint8_t                            preset_id,
                             std::array<uint32_t, PARAM_COUNT>& out_params) const;

    // Queue preset switch (applied at next ISR). Returns false if invalid or validation fails.
    bool SwitchToPreset(uint8_t preset_id);

    uint8_t GetActivePresetId() const { return active_preset_id_; }
    uint8_t GetSupportedPresetBitmask() const;

private:
    std::array<RuntimeCfg, NUM_PRESETS> presets_{};
    std::array<bool, NUM_PRESETS>       is_valid_{};
    uint8_t                             active_preset_id_{DEFAULT_PRESET};
    volatile uint8_t                    pending_preset_id_{INVALID_PRESET_ID};
    volatile bool                       apply_pending_{false};
};

}  // namespace nv::soc_pwr_smoothing
