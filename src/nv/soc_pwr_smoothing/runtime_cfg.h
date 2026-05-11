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

#include "nv/common/fixed_point.h"
#include "nv/soc_pwr_smoothing/offset_policy.h"
#include "nv/soc_pwr_smoothing/power_brake_policy.h"
#include "nv/soc_pwr_smoothing/thermal_brake_policy.h"

namespace nv::soc_pwr_smoothing {

// Enums for runtime policy control
enum class PowerBrakeState
{
    PowerBrakeDisabled = 0,
    PowerBrakeEnabled  = 1
};

enum class ThermBrakeState
{
    ThermBrakeDisabled = 0,
    ThermBrakeEnabled  = 1
};

enum class ConstantPowerMode
{
    ConstantPowerModeOff = 0,  // Offset policy disabled
    ConstantPowerModeOn  = 1   // Offset policy enabled
};

struct RuntimeCfg
{
    float                        max_ac_ramp_rate{0.0f};  // AC ramp rate limit (0 = disabled)
    PowerBrakePolicy::RuntimeCfg power_brake_policy{.entry_threshold = to_sfxp22_10(0.5),
                                                    .exit_threshold  = to_sfxp22_10(0.8),
                                                    .enabled         = false};
    ThermBrakePolicy::RuntimeCfg therm_brake_policy{.enabled = true};
    OffsetPolicy::RuntimeCfg     isink_offset_policy{
            .enabled = false,
            .residency_pid =
                {
                                .target         = to_sfxp22_10(2),
                                .kp             = to_sfxp22_10(-0.4),
                                .ki             = to_sfxp22_10(-0.001),
                                .kd             = to_sfxp22_10(0),
                                .integral_min   = to_sfxp22_10(-1000000),
                                .integral_max   = to_sfxp22_10(10),
                                .output_min     = to_sfxp22_10(0),
                                .output_max     = to_sfxp22_10(100),
                                .pid_dt_divisor = 150,
                                },
            .critical_pid =
                {
                                .target         = to_sfxp22_10(80),
                                .kp             = to_sfxp22_10(-180.0 / 20.0),
                                .ki             = to_sfxp22_10(0),
                                .kd             = to_sfxp22_10(0),
                                .integral_min   = to_sfxp22_10(-1000000),
                                .integral_max   = to_sfxp22_10(1000000),
                                .output_min     = to_sfxp22_10(0),
                                .output_max     = to_sfxp22_10(100),
                                .pid_dt_divisor = 150,
                                },
            .residency_threshold = to_sfxp22_10(75)
    };
    OffsetPolicy::RuntimeCfg edpp_offset_policy{
        .enabled = false,
        .residency_pid =
            {
                            .target         = to_sfxp22_10(2),
                            .kp             = to_sfxp22_10(-0.4),
                            .ki             = to_sfxp22_10(-0.001),
                            .kd             = to_sfxp22_10(0),
                            .integral_min   = to_sfxp22_10(-1000000),
                            .integral_max   = to_sfxp22_10(10),
                            .output_min     = to_sfxp22_10(0),
                            .output_max     = to_sfxp22_10(100),
                            .pid_dt_divisor = 150,
                            },
        .critical_pid =
            {
                            .target         = to_sfxp22_10(20),
                            .kp             = to_sfxp22_10(180.0 / 20.0),
                            .ki             = to_sfxp22_10(0),
                            .kd             = to_sfxp22_10(0),
                            .integral_min   = to_sfxp22_10(-1000000),
                            .integral_max   = to_sfxp22_10(1000000),
                            .output_min     = to_sfxp22_10(0),
                            .output_max     = to_sfxp22_10(100),
                            .pid_dt_divisor = 150,
                            },
        .residency_threshold = to_sfxp22_10(25)
    };

    // Override parameters (test hooks for debugging/validation)
    // These allow direct control of inputs/outputs, bypassing normal operation
    struct OverrideConfig
    {
        bool      enabled{false};  // Override enable flag
        SFXP22_10 value{0};        // Override value in SFXP22_10 format
    };

    OverrideConfig override_soc_input;     // Override SoC input (param 40)
    OverrideConfig override_edpp_offset;   // Override EDPP output percent (param 41)
    OverrideConfig override_isink_offset;  // Override ISINK output percent (param 42)

    struct OverrideDacRawConfig
    {
        bool     enabled{false};
        uint16_t dac_raw{0};  // Code passed to Dac::set (12-bit effective)
    };
    OverrideDacRawConfig override_edpp_dac_raw;   // Param 43; if enabled, overrides param 41 +
                                                  // PID path to DAC
    OverrideDacRawConfig override_isink_dac_raw;  // Param 44; if enabled, overrides param 42 +
                                                  // PID path to DAC
};

}  // namespace nv::soc_pwr_smoothing
