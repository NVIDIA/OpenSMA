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

#include "nv/soc_pwr_smoothing/presets.h"
#include "nv/soc_pwr_smoothing/power_manager.h"
#include "nv/common/fixed_point.h"
#include "nv/mctp/nsm_type_ff.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace nv::soc_pwr_smoothing {

namespace {

constexpr uint16_t PERCENT_SCALE = 100;

constexpr uint32_t kOverrideIsOverrideShift = 24U;
constexpr uint32_t kOverrideReservedShift   = 16U;
constexpr uint32_t kByteMask                = 0xFFU;
constexpr uint32_t kUint16Mask              = 0xFFFFU;

using RackPwrSmoothParams = nv::mctp::RackPwrSmoothParams;
using nv::SFXP22_10;

}  // namespace

// -----------------------------------------------------------------------------
// OverrideParam
// -----------------------------------------------------------------------------

uint32_t OverrideParam::to_uint32() const
{
    return (static_cast<uint32_t>(is_override) << kOverrideIsOverrideShift)
         | (static_cast<uint32_t>(reserved) << kOverrideReservedShift)
         | static_cast<uint32_t>(value);
}

OverrideParam OverrideParam::from_uint32(uint32_t raw)
{
    return OverrideParam{
        .is_override = static_cast<uint8_t>((raw >> kOverrideIsOverrideShift) & kByteMask),
        .reserved    = static_cast<uint8_t>((raw >> kOverrideReservedShift) & kByteMask),
        .value       = static_cast<uint16_t>(raw & kUint16Mask)};
}

// -----------------------------------------------------------------------------
// Parameter validation (tuning + override wire values)
// -----------------------------------------------------------------------------

namespace {

constexpr uint16_t MAX_OVERRIDE_VALUE = 10000;  // Max override param value (100.00%)

// Validation constraints for tuning parameters 0 .. MaxTuningParams-1
static constexpr std::array<ParameterConstraints,
                            static_cast<size_t>(nv::mctp::RackPwrSmoothParams::MaxTuningParams)>
    param_constraints = {
        {
         // EDPP Critical PID (Primary) - SoC targets are percentages (0-100%)
            {nv::mctp::RackPwrSmoothParams::EdppSoCTargetPrimary, 0.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::EdppSocCriticalLow, 0.0f, 100.0f},
         // PID gains - reasonable ranges to prevent instability
            {nv::mctp::RackPwrSmoothParams::EdppPrimaryKp, -1000.0f, 1000.0f},
         {nv::mctp::RackPwrSmoothParams::EdppPrimaryKi, -100.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::EdppPrimaryKd, -10000.0f, 10000.0f},
         {nv::mctp::RackPwrSmoothParams::EdppLowSocResidencyThreshold, 0.0f, 100.0f},

         // EDPP Residency PID (Secondary)
            {nv::mctp::RackPwrSmoothParams::EdppResidencyTargetSecondary, 0.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::EdppSecondaryKp, -1000.0f, 1000.0f},
         {nv::mctp::RackPwrSmoothParams::EdppSecondaryKi, -100.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::EdppSecondaryKd, -10000.0f, 10000.0f},

         // ISINK Critical PID (Primary)
            {nv::mctp::RackPwrSmoothParams::IsinkSoCTargetPrimary, 0.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkSoCCriticalHigh, 0.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkPrimaryKp, -1000.0f, 1000.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkPrimaryKi, -100.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkPrimaryKd, -10000.0f, 10000.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkHighSoCResidencyThreshold, 0.0f, 100.0f},

         // ISINK Residency PID (Secondary)
            {nv::mctp::RackPwrSmoothParams::IskinkResidencyTargetSecondary, 0.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkSecondaryKp, -1000.0f, 1000.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkSecondaryKi, -100.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkSecondaryKd, -10000.0f, 10000.0f},

         {nv::mctp::RackPwrSmoothParams::EdppResidencyEwmaTau, 1.0f, 60.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkResidencyEwmaTau, 1.0f, 60.0f},

         {nv::mctp::RackPwrSmoothParams::EdppIntegralMin, -2000000.0f, 2000000.0f},
         {nv::mctp::RackPwrSmoothParams::EdppIntegralMax, -2000000.0f, 2000000.0f},
         {nv::mctp::RackPwrSmoothParams::EdppOutputMax, 0.0f, 100.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkIntegralMin, -2000000.0f, 2000000.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkIntegralMax, -2000000.0f, 2000000.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkOutputMax, 0.0f, 100.0f},

         {nv::mctp::RackPwrSmoothParams::EdppPrimaryPidDtDivisor, 1.0f, 1000.0f},
         {nv::mctp::RackPwrSmoothParams::EdppSecondaryPidDtDivisor, 1.0f, 1000.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkPrimaryPidDtDivisor, 1.0f, 1000.0f},
         {nv::mctp::RackPwrSmoothParams::IsinkSecondaryPidDtDivisor, 1.0f, 1000.0f},
         }
};

}  // namespace

bool validate_parameter(nv::mctp::RackPwrSmoothParams param_id, uint32_t raw_value)
{
    using namespace nv::mctp;

    if (param_id == RackPwrSmoothParams::OverrideEdppDacRaw
        || param_id == RackPwrSmoothParams::OverrideIsinkDacRaw) {
        const auto override = OverrideParam::from_uint32(raw_value);
        if (override.is_override > 1 || override.reserved != 0) {
            return false;
        }
        constexpr auto max_dac = static_cast<uint16_t>((1U << sys::dac::Dac::ResolutionBits)
                                                       - 1U);
        if (override.value > max_dac) {
            return false;
        }
        return true;
    }

    if (param_id >= RackPwrSmoothParams::OverrideSoCInput
        && param_id <= RackPwrSmoothParams::OverrideIsinkOffsetOutput) {
        const auto override = OverrideParam::from_uint32(raw_value);
        if (override.value > MAX_OVERRIDE_VALUE) {
            return false;
        }
        return true;
    }

    for (const auto& c : param_constraints) {
        if (c.id != param_id) {
            continue;
        }
        if (param_id == RackPwrSmoothParams::EdppPrimaryPidDtDivisor
            || param_id == RackPwrSmoothParams::EdppSecondaryPidDtDivisor
            || param_id == RackPwrSmoothParams::IsinkPrimaryPidDtDivisor
            || param_id == RackPwrSmoothParams::IsinkSecondaryPidDtDivisor) {
            return raw_value >= MIN_DT_SCALER && raw_value <= MAX_DT_SCALER;
        }
        if (param_id == RackPwrSmoothParams::EdppResidencyEwmaTau
            || param_id == RackPwrSmoothParams::IsinkResidencyEwmaTau) {
            const auto float_val = std::bit_cast<float>(raw_value);
            if (float_val < c.min_value || float_val > c.max_value) {
                return false;
            }
            return true;
        }
        const auto  fixed_val = static_cast<nv::SFXP22_10>(raw_value);
        const float float_val = sfxp22_10_to_float(fixed_val);
        if (float_val < c.min_value || float_val > c.max_value) {
            return false;
        }
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
// PresetManager - param index <-> RuntimeCfg helpers
// -----------------------------------------------------------------------------

namespace {

// Byte offsets into RuntimeCfg for each tuning param (0-31). Params 0-19, 22-27 are SFXP22_10;
// params 20-21 are float (stored as uint32_t bit pattern); 28-31 are uint32_t PID dt divisors
//  1-1000.  Params 0/1 and 10/11 share the same field (critical_pid.target).
constexpr std::array<size_t, static_cast<size_t>(RackPwrSmoothParams::MaxTuningParams)>
    kTuningParamOffset{
        {
         offsetof(RuntimeCfg,
         edpp_offset_policy.critical_pid.target),  // 0 EdppSoCTargetPrimary
            offsetof(RuntimeCfg, edpp_offset_policy.critical_pid.target),            // 1
            offsetof(RuntimeCfg, edpp_offset_policy.critical_pid.kp),                // 2
            offsetof(RuntimeCfg, edpp_offset_policy.critical_pid.ki),                // 3
            offsetof(RuntimeCfg, edpp_offset_policy.critical_pid.kd),                // 4
            offsetof(RuntimeCfg, edpp_offset_policy.residency_threshold),            // 5
            offsetof(RuntimeCfg, edpp_offset_policy.residency_pid.target),           // 6
            offsetof(RuntimeCfg, edpp_offset_policy.residency_pid.kp),               // 7
            offsetof(RuntimeCfg, edpp_offset_policy.residency_pid.ki),               // 8
            offsetof(RuntimeCfg, edpp_offset_policy.residency_pid.kd),               // 9
            offsetof(RuntimeCfg, isink_offset_policy.critical_pid.target),           // 10
            offsetof(RuntimeCfg, isink_offset_policy.critical_pid.target),           // 11
            offsetof(RuntimeCfg, isink_offset_policy.critical_pid.kp),               // 12
            offsetof(RuntimeCfg, isink_offset_policy.critical_pid.ki),               // 13
            offsetof(RuntimeCfg, isink_offset_policy.critical_pid.kd),               // 14
            offsetof(RuntimeCfg, isink_offset_policy.residency_threshold),           // 15
            offsetof(RuntimeCfg, isink_offset_policy.residency_pid.target),          // 16
            offsetof(RuntimeCfg, isink_offset_policy.residency_pid.kp),              // 17
            offsetof(RuntimeCfg, isink_offset_policy.residency_pid.ki),              // 18
            offsetof(RuntimeCfg, isink_offset_policy.residency_pid.kd),              // 19
            offsetof(RuntimeCfg, edpp_offset_policy.residency_sma.ewma_tau),         // 20
            offsetof(RuntimeCfg, isink_offset_policy.residency_sma.ewma_tau),        // 21
            offsetof(RuntimeCfg, edpp_offset_policy.residency_pid.integral_min),     // 22
            offsetof(RuntimeCfg, edpp_offset_policy.residency_pid.integral_max),     // 23
            offsetof(RuntimeCfg, edpp_offset_policy.residency_pid.output_max),       // 24
            offsetof(RuntimeCfg, isink_offset_policy.residency_pid.integral_min),    // 25
            offsetof(RuntimeCfg, isink_offset_policy.residency_pid.integral_max),    // 26
            offsetof(RuntimeCfg, isink_offset_policy.residency_pid.output_max),      // 27
            offsetof(RuntimeCfg, edpp_offset_policy.critical_pid.pid_dt_divisor),    // 28
            offsetof(RuntimeCfg, edpp_offset_policy.residency_pid.pid_dt_divisor),   // 29
            offsetof(RuntimeCfg, isink_offset_policy.critical_pid.pid_dt_divisor),   // 30
            offsetof(RuntimeCfg, isink_offset_policy.residency_pid.pid_dt_divisor),  // 31
        }
};

static void refresh_pid_dt_fp(OffsetPolicy::PidController::RuntimeCfg& cfg)
{
    cfg.pid_dt_fp = OffsetPolicy::PidController::dt_fp_from_divisor(cfg.pid_dt_divisor);
}

static void refresh_residency_sma_fp(OffsetPolicy::ResidencySma::RuntimeCfg& cfg)
{
    cfg.ewma_alpha           = OffsetPolicy::ResidencySma::ewma_alpha_from_tau(cfg.ewma_tau);
    cfg.one_minus_ewma_alpha = OffsetPolicy::ResidencySma::one_minus_ewma_alpha_from_tau(
        cfg.ewma_tau);
}

static void refresh_pid_dt_fp_for_param(RuntimeCfg& cfg, uint8_t param_id)
{
    switch (static_cast<RackPwrSmoothParams>(param_id)) {
        case RackPwrSmoothParams::EdppPrimaryPidDtDivisor:
            refresh_pid_dt_fp(cfg.edpp_offset_policy.critical_pid);
            break;
        case RackPwrSmoothParams::EdppSecondaryPidDtDivisor:
            refresh_pid_dt_fp(cfg.edpp_offset_policy.residency_pid);
            break;
        case RackPwrSmoothParams::IsinkPrimaryPidDtDivisor:
            refresh_pid_dt_fp(cfg.isink_offset_policy.critical_pid);
            break;
        case RackPwrSmoothParams::IsinkSecondaryPidDtDivisor:
            refresh_pid_dt_fp(cfg.isink_offset_policy.residency_pid);
            break;
        default: break;
    }
}

static void refresh_residency_sma_fp_for_param(RuntimeCfg& cfg, uint8_t param_id)
{
    switch (static_cast<RackPwrSmoothParams>(param_id)) {
        case RackPwrSmoothParams::EdppResidencyEwmaTau:
            refresh_residency_sma_fp(cfg.edpp_offset_policy.residency_sma);
            break;
        case RackPwrSmoothParams::IsinkResidencyEwmaTau:
            refresh_residency_sma_fp(cfg.isink_offset_policy.residency_sma);
            break;
        default: break;
    }
}

// Override params 40-42: pointer-to-member so we can index by (param_id - 40).
using OverrideCfgPtr = RuntimeCfg::OverrideConfig RuntimeCfg::*;
constexpr std::array<OverrideCfgPtr, 3U>          kOverrideCfgMember{
             {
              &RuntimeCfg::override_soc_input,
              &RuntimeCfg::override_edpp_offset,
              &RuntimeCfg::override_isink_offset,
              }
};

static uint32_t get_override_as_uint32(const RuntimeCfg::OverrideConfig& oc)
{
    return OverrideParam{
        .is_override = static_cast<uint8_t>(oc.enabled ? 1U : 0U),
        .reserved    = 0,
        .value = static_cast<uint16_t>((static_cast<int32_t>(oc.value) * PERCENT_SCALE) / 1024)}
        .to_uint32();
}

static void set_override_from_uint32(RuntimeCfg::OverrideConfig& oc, uint32_t value)
{
    const auto ov = OverrideParam::from_uint32(value);
    oc.enabled    = (ov.is_override != 0);
    oc.value = static_cast<SFXP22_10>((static_cast<int32_t>(ov.value) * 1024) / PERCENT_SCALE);
}

static uint32_t get_dac_raw_override_as_uint32(const RuntimeCfg::OverrideDacRawConfig& oc)
{
    return OverrideParam{.is_override = static_cast<uint8_t>(oc.enabled ? 1U : 0U),
                         .reserved    = 0,
                         .value       = oc.dac_raw}
        .to_uint32();
}

static void set_dac_raw_override_from_uint32(RuntimeCfg::OverrideDacRawConfig& oc,
                                             uint32_t                          value)
{
    const auto ov = OverrideParam::from_uint32(value);
    oc.enabled    = (ov.is_override != 0);
    oc.dac_raw    = ov.value;
}

uint32_t get_param_from_cfg(const RuntimeCfg& cfg, uint8_t param_id)
{
    if (param_id >= static_cast<uint8_t>(RackPwrSmoothParams::TestHookParamsStart)) {
        const uint32_t idx = static_cast<uint32_t>(param_id)
                           - static_cast<uint32_t>(RackPwrSmoothParams::TestHookParamsStart);
        if (idx < kOverrideCfgMember.size()) {
            return get_override_as_uint32(cfg.*(kOverrideCfgMember.at(idx)));
        }
        if (param_id == static_cast<uint8_t>(RackPwrSmoothParams::OverrideEdppDacRaw)) {
            return get_dac_raw_override_as_uint32(cfg.override_edpp_dac_raw);
        }
        if (param_id == static_cast<uint8_t>(RackPwrSmoothParams::OverrideIsinkDacRaw)) {
            return get_dac_raw_override_as_uint32(cfg.override_isink_dac_raw);
        }
        return 0;
    }
    if (param_id >= static_cast<uint8_t>(RackPwrSmoothParams::MaxTuningParams)) {
        return 0;
    }
    const size_t off  = kTuningParamOffset.at(static_cast<size_t>(param_id));
    const auto*  base = static_cast<const char*>(static_cast<const void*>(&cfg));
    uint32_t     out{};
    std::memcpy(&out, base + off, sizeof(out));
    return out;
}

void set_param_in_cfg(RuntimeCfg& cfg, uint8_t param_id, uint32_t value)
{
    if (param_id >= static_cast<uint8_t>(RackPwrSmoothParams::TestHookParamsStart)) {
        const uint32_t idx = static_cast<uint32_t>(param_id)
                           - static_cast<uint32_t>(RackPwrSmoothParams::TestHookParamsStart);
        if (idx < kOverrideCfgMember.size()) {
            set_override_from_uint32(cfg.*(kOverrideCfgMember.at(idx)), value);
            return;
        }
        if (param_id == static_cast<uint8_t>(RackPwrSmoothParams::OverrideEdppDacRaw)) {
            set_dac_raw_override_from_uint32(cfg.override_edpp_dac_raw, value);
            return;
        }
        if (param_id == static_cast<uint8_t>(RackPwrSmoothParams::OverrideIsinkDacRaw)) {
            set_dac_raw_override_from_uint32(cfg.override_isink_dac_raw, value);
            return;
        }
        return;
    }
    if (param_id >= static_cast<uint8_t>(RackPwrSmoothParams::MaxTuningParams)) {
        return;
    }
    const size_t off  = kTuningParamOffset.at(static_cast<size_t>(param_id));
    auto*        base = static_cast<char*>(static_cast<void*>(&cfg));
    std::memcpy(base + off, &value, sizeof(value));
    refresh_pid_dt_fp_for_param(cfg, param_id);
    refresh_residency_sma_fp_for_param(cfg, param_id);
}

}  // namespace

// -----------------------------------------------------------------------------
// PresetManager
// -----------------------------------------------------------------------------

void PresetManager::InitializeDefaultPreset()
{
    presets_.at(DEFAULT_PRESET)  = RuntimeCfg{};
    is_valid_.at(DEFAULT_PRESET) = true;
}

void PresetManager::InitializePresetFromDefault(uint8_t preset_id)
{
    if (preset_id == DEFAULT_PRESET || preset_id >= NUM_PRESETS) {
        return;
    }
    presets_.at(preset_id)  = presets_.at(DEFAULT_PRESET);
    is_valid_.at(preset_id) = true;
}

void PresetManager::ClearPending()
{
    pending_preset_id_ = INVALID_PRESET_ID;
    apply_pending_     = false;
}

void PresetManager::ApplyPresetChange(PowerManager& pm)
{
    const uint8_t new_id = pending_preset_id_;
    if (new_id >= NUM_PRESETS || !is_valid_.at(new_id)) {
        apply_pending_     = false;
        pending_preset_id_ = INVALID_PRESET_ID;
        return;
    }
    const RuntimeCfg new_cfg = GetCfgForApply(new_id, pm.config);
    pm.config                = new_cfg;
    pm.isink_offset_policy.reset();
    pm.edpp_offset_policy.reset();
    pm.power_brake_policy.reset();
    active_preset_id_  = new_id;
    apply_pending_     = false;
    pending_preset_id_ = INVALID_PRESET_ID;
}

RuntimeCfg PresetManager::GetCfgForApply(uint8_t preset_id, const RuntimeCfg& current) const
{
    if (preset_id >= NUM_PRESETS || !is_valid_.at(preset_id)) {
        return current;
    }
    RuntimeCfg out                  = presets_.at(preset_id);
    out.power_brake_policy.enabled  = current.power_brake_policy.enabled;
    out.edpp_offset_policy.enabled  = current.edpp_offset_policy.enabled;
    out.isink_offset_policy.enabled = current.isink_offset_policy.enabled;
    out.therm_brake_policy.enabled  = current.therm_brake_policy.enabled;
    out.max_ac_ramp_rate            = current.max_ac_ramp_rate;
    return out;
}

bool PresetManager::ValidatePreset(uint8_t preset_id) const
{
    if (preset_id >= NUM_PRESETS || !is_valid_.at(preset_id)) {
        return false;
    }
    const RuntimeCfg& cfg = presets_.at(preset_id);
    for (uint8_t param_id = 0; param_id < PARAM_COUNT; param_id++) {
        if (!is_valid_param_id(param_id)) {
            continue;
        }
        const uint32_t raw = get_param_from_cfg(cfg, param_id);
        const auto     id  = static_cast<nv::mctp::RackPwrSmoothParams>(param_id);
        if (!validate_parameter(id, raw)) {
            return false;
        }
    }
    return true;
}

bool PresetManager::SetParameter(uint8_t preset_id, uint8_t param_id, uint32_t value)
{
    if (preset_id < MIN_WRITABLE_PRESET || preset_id > MAX_WRITABLE_PRESET) {
        return false;
    }
    if (!is_valid_param_id(param_id)) {
        return false;
    }
    const auto param_enum = static_cast<nv::mctp::RackPwrSmoothParams>(param_id);
    if (!validate_parameter(param_enum, value)) {
        return false;
    }
    if (!is_valid_.at(preset_id)) {
        InitializePresetFromDefault(preset_id);
    }
    set_param_in_cfg(presets_.at(preset_id), param_id, value);

    if (preset_id == active_preset_id_) {
        pending_preset_id_ = preset_id;
        apply_pending_     = true;
    }
    return true;
}

void PresetManager::GetPresetParameters(uint8_t                            preset_id,
                                        std::array<uint32_t, PARAM_COUNT>& out_params) const
{
    out_params.fill(0);
    if (preset_id >= NUM_PRESETS || !is_valid_.at(preset_id)) {
        return;
    }
    const RuntimeCfg& cfg = presets_.at(preset_id);
    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        if (!is_valid_param_id(i)) {
            continue;
        }
        out_params.at(i) = get_param_from_cfg(cfg, i);
    }
}

bool PresetManager::SwitchToPreset(uint8_t preset_id)
{
    if (preset_id >= NUM_PRESETS || !is_valid_.at(preset_id)) {
        return false;
    }
    if (!ValidatePreset(preset_id)) {
        return false;
    }
    pending_preset_id_ = preset_id;
    apply_pending_     = true;
    return true;
}

uint8_t PresetManager::GetSupportedPresetBitmask() const
{
    uint8_t bitmask = 0;
    for (uint8_t i = 0; i < NUM_PRESETS; i++) {
        if (is_valid_.at(i)) {
            bitmask |= (1u << i);
        }
    }
    return bitmask;
}

}  // namespace nv::soc_pwr_smoothing
