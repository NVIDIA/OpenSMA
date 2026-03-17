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
#include "nv/bootloader.h"
#include NV_IPC_CONFIG_H
#include "nv/common/preproc.h"
#include "nv/soc_pwr_smoothing/soc_pwr_smoothing.h"
#include "nv/soc_pwr_smoothing/power_manager.h"
#include "nv/soc_pwr_smoothing/presets.h"
#include "nv/ctimer/ctimer.h"
#include "sys/adc/adc.h"
#include "nv/common/system.h"
#include "nv/logger/log.h"
#include "nv/mctp/nsm_type_ff.h"
#include "nv/flash/flash.h"
#include "sys/lpcac/lpcac.h"
#include "sys/i2c/utils.h"

#include <array>
#include <algorithm>
#include <bit>
#include <chrono>

#include <FreeRTOS.h>
#include <task.h>

using namespace std::chrono_literals;

namespace nv::soc_pwr_smoothing {

namespace {

// ============================================================================
// Module-level state (not part of PowerSmoothing class)
// ============================================================================

// NOLINTBEGIN(*-avoid-non-const-global-variables)
constexpr bool SocPowerSmoothingDebugEnabled = false;
// NOLINTEND(*-avoid-non-const-global-variables)

// ============================================================================
// Constants
// ============================================================================

constexpr uint16_t MAX_OVERRIDE_VALUE = 10000;  // Max override param value (100.00%)
constexpr uint16_t PERCENT_SCALE      = 100;    // Percentage scaling factor
constexpr uint8_t  INVALID_PRESET_ID  = 0xFF;   // Marker for invalid/unset preset ID

// Validation constraints for parameters 0-19
// TODO: Review and tune these min/max limits based on system requirements
// and stability analysis. Current values are conservative defaults.
static constexpr std::array<ParameterConstraints, 20> param_constraints = {
    {
     // EDPP Critical PID (Primary) - SoC targets are percentages (0-100%)
        {nv::mctp::RackPwrSmoothParams::EdppSoCTargetPrimary, 0.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::EdppSocCriticalLow, 0.0f, 100.0f},
     // PID gains - reasonable ranges to prevent instability
        {nv::mctp::RackPwrSmoothParams::EdppPrimaryKp, -1000.0f, 1000.0f},
     {nv::mctp::RackPwrSmoothParams::EdppPrimaryKi, -100.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::EdppPrimaryKd, -1000.0f, 1000.0f},
     {nv::mctp::RackPwrSmoothParams::EdppLowSocResidencyThreshold, 0.0f, 100.0f},

     // EDPP Residency PID (Secondary)
        {nv::mctp::RackPwrSmoothParams::EdppResidencyTargetSecondary, 0.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::EdppSecondaryKp, -1000.0f, 1000.0f},
     {nv::mctp::RackPwrSmoothParams::EdppSecondaryKi, -100.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::EdppSecondaryKd, -1000.0f, 1000.0f},

     // ISINK Critical PID (Primary)
        {nv::mctp::RackPwrSmoothParams::IsinkSoCTargetPrimary, 0.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::IsinkSoCCriticalHigh, 0.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::IsinkPrimaryKp, -1000.0f, 1000.0f},
     {nv::mctp::RackPwrSmoothParams::IsinkPrimaryKi, -100.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::IsinkPrimaryKd, -1000.0f, 1000.0f},
     {nv::mctp::RackPwrSmoothParams::IsinkHighSoCResidencyThreshold, 0.0f, 100.0f},

     // ISINK Residency PID (Secondary)
        {nv::mctp::RackPwrSmoothParams::IskinkResidencyTargetSecondary, 0.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::IsinkSecondaryKp, -1000.0f, 1000.0f},
     {nv::mctp::RackPwrSmoothParams::IsinkSecondaryKi, -100.0f, 100.0f},
     {nv::mctp::RackPwrSmoothParams::IsinkSecondaryKd, -1000.0f, 1000.0f},
     }
};

// ============================================================================
// ADC Calibration Test Mode - I2C DAC Control
// ============================================================================

// AD5693 DAC constants
constexpr uint8_t Ad5693CmdWriteAndUpdate = 0x30;  // Write to and update DAC register
constexpr uint8_t ByteMaskLsb             = 0xFF;  // Mask for extracting LSB

/// @brief Write a 16-bit code to the external AD5693-compatible DAC
/// @param dac_code 16-bit DAC code (0-65535 maps to 0V-2.5V output)
/// @return true on success, false on I2C error
bool write_ad5693_dac(uint16_t dac_code)
{
    // AD5693 I2C protocol: [Command byte] [Data MSB] [Data LSB]
    // NOLINTNEXTLINE(misc-const-correctness) - i2c_write takes non-const span
    std::array<uint8_t, 3> buffer = {Ad5693CmdWriteAndUpdate,
                                     static_cast<uint8_t>(dac_code >> 8),
                                     static_cast<uint8_t>(dac_code & ByteMaskLsb)};
    auto status = sys::i2c::i2c_write(nv::soc_pwr_smoothing::AdcCalibDacI2cPort,
                                      nv::soc_pwr_smoothing::AdcCalibDacI2cAddr,
                                      buffer);
    return (status == nv::i2c::I2cStatus::Ok);
}

// ============================================================================
// ADC Calibration Test Mode - State Machine
// ============================================================================

// Calibration timing: 5ms per step
constexpr uint32_t ADC_CALIB_STEP_DELAY_MS = 5;

// DAC codes for 26 calibration voltage points (0V to 2.5V, 100mV increments)
// Formula: dac_code = (voltage / 2.5V) * 65535 = voltage * 26214
// External DAC: 16-bit, 0-2.5V full scale
// Signal path: DAC -> Op-amp (gain 4.322) -> Connector -> Divider (0.2492) -> ADC
constexpr std::array<uint16_t, nv::mctp::ADC_CALIBRATION_NUM_POINTS> CalibrationDacCodes = {
    0,      // Point 0:  0.0V
    2621,   // Point 1:  0.1V
    5243,   // Point 2:  0.2V
    7864,   // Point 3:  0.3V
    10486,  // Point 4:  0.4V
    13107,  // Point 5:  0.5V
    15729,  // Point 6:  0.6V
    18350,  // Point 7:  0.7V
    20972,  // Point 8:  0.8V
    23593,  // Point 9:  0.9V
    26214,  // Point 10: 1.0V
    28836,  // Point 11: 1.1V
    31457,  // Point 12: 1.2V
    34079,  // Point 13: 1.3V
    36700,  // Point 14: 1.4V
    39321,  // Point 15: 1.5V
    41943,  // Point 16: 1.6V
    44564,  // Point 17: 1.7V
    47186,  // Point 18: 1.8V
    49807,  // Point 19: 1.9V
    52428,  // Point 20: 2.0V
    55050,  // Point 21: 2.1V
    57671,  // Point 22: 2.2V
    60293,  // Point 23: 2.3V
    62914,  // Point 24: 2.4V
    65535,  // Point 25: 2.5V (full scale)
};

// ADC calibration points are handled as local buffers only.
using AdcCalibrationPoints = std::array<nv::mctp::AdcCalibrationPoint,
                                        nv::mctp::ADC_CALIBRATION_NUM_POINTS>;

// Forward declaration for flash save (implemented later)
void save_calibration_to_flash(const AdcCalibrationPoints& points);
bool load_calibration_from_flash(AdcCalibrationPoints& points);

/// @brief Save calibration results to flash (PDS)
void save_calibration_to_flash(const AdcCalibrationPoints& points)
{
    for (uint8_t i = 0; i < nv::mctp::ADC_CALIBRATION_NUM_POINTS; i++) {
        const auto&    pt     = points.at(i);
        const uint32_t packed = (static_cast<uint32_t>(pt.expected_dac_code) << 16)
                              | static_cast<uint32_t>(pt.actual_adc_code);
        const auto key = static_cast<nv::flash::Key>(
            static_cast<uint32_t>(nv::flash::Key::PdsAdcCalibrationData0) + i);
        (void)nv::flash::Flash::set_data(key, packed);
    }

    // Commit completion marker last so power loss cannot advertise partial data as complete.
    (void)nv::flash::Flash::set_data(nv::flash::Key::PdsAdcCalibrationComplete, 1U);
}

bool load_calibration_from_flash(AdcCalibrationPoints& points)
{
    points.fill(nv::mctp::AdcCalibrationPoint{0U, 0U});

    uint32_t flash_complete = 0;
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsAdcCalibrationComplete, flash_complete)
        != nv::flash::Status::Ok) {
        flash_complete = 0;
    }

    constexpr uint16_t kWordMaskLsb = 0xFFFF;
    for (uint8_t i = 0; i < nv::mctp::ADC_CALIBRATION_NUM_POINTS; i++) {
        const auto key = static_cast<nv::flash::Key>(
            static_cast<uint32_t>(nv::flash::Key::PdsAdcCalibrationData0) + i);
        uint32_t packed = 0;
        if (nv::flash::Flash::get_data(key, packed) == nv::flash::Status::Ok) {
            points.at(i).expected_dac_code = static_cast<uint16_t>(packed >> 16);
            points.at(i).actual_adc_code   = static_cast<uint16_t>(packed & kWordMaskLsb);
        }
    }
    return flash_complete == 1U;
}

}  // anonymous namespace

// ============================================================================
// ADC Calibration Test Mode - Public API
// ============================================================================

void execute_adc_calibration()
{
    AdcCalibrationPoints points{};
    (void)nv::flash::Flash::set_data(nv::flash::Key::PdsAdcCalibrationComplete, 0U);

    for (auto& pt : points) {
        pt.expected_dac_code = 0;
        pt.actual_adc_code   = 0;
    }

    // Run all 26 calibration steps (blocking, ~130ms: 26 steps * 5ms). Per step: set DAC,
    // wait 5ms for settling, read ADC.

    for (uint8_t idx = 0; idx < nv::mctp::ADC_CALIBRATION_NUM_POINTS; idx++) {
        const uint16_t dac_code          = CalibrationDacCodes.at(idx);
        points.at(idx).expected_dac_code = dac_code;
        if (!write_ad5693_dac(dac_code)) {
            // Abort calibration if DAC I2C write fails to avoid persisting invalid points.
            nv::logger::error(nv::logger::Event::SocPwrSmoothingAdcCalibDacWriteFail,
                              nv::logger::data_from_two_u32(static_cast<uint32_t>(idx),
                                                            static_cast<uint32_t>(dac_code)));
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(ADC_CALIB_STEP_DELAY_MS));
        points.at(idx).actual_adc_code = PowerSmoothing::get_last_adc_raw();
    }
    write_ad5693_dac(0);  // Reset DAC to 0V

    save_calibration_to_flash(points);
}

void get_adc_calibration_results(nv::mctp::NsmTFFGetAdcCalibResultsRes& response)
{
    AdcCalibrationPoints points{};
    const bool           flash_loaded = load_calibration_from_flash(points);

    response.testComplete   = flash_loaded ? 1U : 0U;
    response.testInProgress = 0U;
    response.numPoints      = nv::mctp::ADC_CALIBRATION_NUM_POINTS;
    response.resvd          = 0U;

    static_assert(sizeof(response.points) == sizeof(points),
                  "ADC calibration point buffers must stay size-compatible");
    std::copy(points.begin(), points.end(), std::begin(response.points));
}

// ============================================================================
// PowerSmoothing Class Implementation
// ============================================================================

// Validate a single parameter
bool PowerSmoothing::validate_parameter(nv::mctp::RackPwrSmoothParams param_id,
                                        uint32_t                      raw_value)
{
    using namespace nv::mctp;

    // Override parameters (20-22): validate scaled value 0-10000 (0-100.00%)
    if (param_id >= RackPwrSmoothParams::OverrideSoCInput) {
        auto override = OverrideParam::from_uint32(raw_value);
        if (override.value > MAX_OVERRIDE_VALUE) {
            return false;
        }
        return true;
    }

    // Regular parameters (0-19): SFXP22_10 encoded, convert to float for validation
    const auto  fixed_val = static_cast<SFXP22_10>(raw_value);
    const float float_val = sfxp22_10_to_float(fixed_val);

    // Find constraint for this parameter
    for (const auto& c : param_constraints) {
        if (c.id == param_id) {
            if (float_val < c.min_value || float_val > c.max_value) {
                return false;
            }
            return true;
        }
    }

    return false;
}

// Validate all parameters in a preset
// Called before switching to a preset to ensure entire configuration is valid
// This provides an additional safety check even though individual params are validated on write
bool PowerSmoothing::validate_preset(const ParameterPreset& preset)
{
    // Only validate non-gap parameters
    for (uint8_t param_id = 0; param_id < PARAM_COUNT; param_id++) {
        // Skip reserved gap (20-39)
        if (!is_valid_param_id(param_id)) {
            continue;
        }
        auto param_enum = static_cast<nv::mctp::RackPwrSmoothParams>(param_id);
        if (!PowerSmoothing::validate_parameter(param_enum, preset.params.at(param_id))) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Preset Initialization and Management
// ============================================================================

// Initialize a writable preset by copying from default Preset 0
// Called automatically on first parameter write to an uninitialized preset
void PowerSmoothing::initialize_preset_from_default(uint8_t preset_id)
{
    if (preset_id == DEFAULT_PRESET) {
        return;
    }

    if (preset_id >= NUM_PRESETS) {
        return;
    }

    // Copy all parameters from Preset 0
    preset_manager.presets.at(preset_id).params = preset_manager.presets.at(DEFAULT_PRESET)
                                                      .params;
    preset_manager.presets.at(preset_id).is_valid = true;
}

// Build RuntimeCfg from preset parameters
// Converts the 23 stored parameters into the RuntimeCfg structure used by the algorithm
// Note: Starts with default RuntimeCfg, then overlays only the preset parameters
RuntimeCfg PowerSmoothing::build_runtime_cfg_from_preset(uint8_t preset_id)
{
    using namespace nv::mctp;

    if (preset_id >= NUM_PRESETS) {
        return RuntimeCfg{};  // Return default config
    }

    const auto& params = preset_manager.presets.at(preset_id).params;

    // Start with default RuntimeCfg (gets defaults from runtime_cfg.h)
    // This includes integral_min/max, power_brake_policy defaults, etc.
    RuntimeCfg cfg;

    // Helper to extract SFXP22_10 from stored uint32_t
    auto get_sfxp22_10 = [&](RackPwrSmoothParams id) -> SFXP22_10 {
        return static_cast<SFXP22_10>(params.at(static_cast<uint8_t>(id)));
    };

    // ========================================================================
    // EDPP Offset Policy Configuration (params 0-9)
    // Override only the configurable PID parameters, keep integral limits as defaults
    // ========================================================================

    // EDPP Critical PID (Primary) - controls critical SoC regions
    cfg.edpp_offset_policy.critical_pid.target = get_sfxp22_10(
        RackPwrSmoothParams::EdppSoCTargetPrimary);
    cfg.edpp_offset_policy.critical_pid.kp = get_sfxp22_10(RackPwrSmoothParams::EdppPrimaryKp);
    cfg.edpp_offset_policy.critical_pid.ki = get_sfxp22_10(RackPwrSmoothParams::EdppPrimaryKi);
    cfg.edpp_offset_policy.critical_pid.kd = get_sfxp22_10(RackPwrSmoothParams::EdppPrimaryKd);
    // integral_min/max: kept as defaults from RuntimeCfg initialization

    // EDPP Residency threshold - determines when residency PID activates
    cfg.edpp_offset_policy.residency_threshold = get_sfxp22_10(
        RackPwrSmoothParams::EdppLowSocResidencyThreshold);

    // EDPP Residency PID (Secondary) - controls residency in threshold region
    cfg.edpp_offset_policy.residency_pid.target = get_sfxp22_10(
        RackPwrSmoothParams::EdppResidencyTargetSecondary);
    cfg.edpp_offset_policy.residency_pid.kp = get_sfxp22_10(
        RackPwrSmoothParams::EdppSecondaryKp);
    cfg.edpp_offset_policy.residency_pid.ki = get_sfxp22_10(
        RackPwrSmoothParams::EdppSecondaryKi);
    cfg.edpp_offset_policy.residency_pid.kd = get_sfxp22_10(
        RackPwrSmoothParams::EdppSecondaryKd);
    // integral_min/max: kept as defaults from RuntimeCfg initialization

    // ========================================================================
    // ISINK Offset Policy Configuration (params 10-19)
    // Override only the configurable PID parameters, keep integral limits as defaults
    // ========================================================================

    // ISINK Critical PID (Primary) - controls critical SoC regions
    cfg.isink_offset_policy.critical_pid.target = get_sfxp22_10(
        RackPwrSmoothParams::IsinkSoCTargetPrimary);
    cfg.isink_offset_policy.critical_pid.kp = get_sfxp22_10(
        RackPwrSmoothParams::IsinkPrimaryKp);
    cfg.isink_offset_policy.critical_pid.ki = get_sfxp22_10(
        RackPwrSmoothParams::IsinkPrimaryKi);
    cfg.isink_offset_policy.critical_pid.kd = get_sfxp22_10(
        RackPwrSmoothParams::IsinkPrimaryKd);
    // integral_min/max: kept as defaults from RuntimeCfg initialization

    // ISINK Residency threshold - determines when residency PID activates
    cfg.isink_offset_policy.residency_threshold = get_sfxp22_10(
        RackPwrSmoothParams::IsinkHighSoCResidencyThreshold);

    // ISINK Residency PID (Secondary) - controls residency in threshold region
    cfg.isink_offset_policy.residency_pid.target = get_sfxp22_10(
        RackPwrSmoothParams::IskinkResidencyTargetSecondary);
    cfg.isink_offset_policy.residency_pid.kp = get_sfxp22_10(
        RackPwrSmoothParams::IsinkSecondaryKp);
    cfg.isink_offset_policy.residency_pid.ki = get_sfxp22_10(
        RackPwrSmoothParams::IsinkSecondaryKi);
    cfg.isink_offset_policy.residency_pid.kd = get_sfxp22_10(
        RackPwrSmoothParams::IsinkSecondaryKd);
    // integral_min/max: kept as defaults from RuntimeCfg initialization

    // ========================================================================
    // Override Parameters (Test Hooks) (params 20-22)
    // ========================================================================

    // SoC Input Override (param 20)
    auto soc_override = OverrideParam::from_uint32(
        params.at(static_cast<uint8_t>(RackPwrSmoothParams::OverrideSoCInput)));
    cfg.override_soc_input.enabled = (soc_override.is_override != 0);
    // Value is scaled by 100, convert to SFXP22_10: value/100 * 1024 = value * 10.24
    cfg.override_soc_input.value = (static_cast<int32_t>(soc_override.value) * 1024)
                                 / PERCENT_SCALE;

    // EDPP Output Override (param 21)
    auto edpp_override = OverrideParam::from_uint32(
        params.at(static_cast<uint8_t>(RackPwrSmoothParams::OverrideEdppOffsetOutput)));
    cfg.override_edpp_offset.enabled = (edpp_override.is_override != 0);
    cfg.override_edpp_offset.value   = (static_cast<int32_t>(edpp_override.value) * 1024)
                                   / PERCENT_SCALE;

    // ISINK Output Override (param 22)
    auto isink_override = OverrideParam::from_uint32(
        params.at(static_cast<uint8_t>(RackPwrSmoothParams::OverrideIsinkOffsetOutput)));
    cfg.override_isink_offset.enabled = (isink_override.is_override != 0);
    cfg.override_isink_offset.value   = (static_cast<int32_t>(isink_override.value) * 1024)
                                    / PERCENT_SCALE;

    // ========================================================================
    // Preserve runtime operational controls (separate from preset tuning)
    // These are controlled by dedicated commands and should persist across preset switches
    // ========================================================================

    // Preserve power brake enable state (set via SetPowerBrakePolicyState)
    cfg.power_brake_policy.enabled = power_manager.config.power_brake_policy.enabled;

    // Preserve thermal brake enable state (set via SetThermBrakePolicyState)
    cfg.therm_brake_policy.enabled = power_manager.config.therm_brake_policy.enabled;

    // Preserve offset policy enable states (set via SetOffsetPolicyState or SetMaxACRampRate)
    cfg.edpp_offset_policy.enabled  = power_manager.config.edpp_offset_policy.enabled;
    cfg.isink_offset_policy.enabled = power_manager.config.isink_offset_policy.enabled;

    // Preserve max AC ramp rate (set via SetMaxACRampRate)
    cfg.max_ac_ramp_rate = power_manager.config.max_ac_ramp_rate;

    return cfg;
}

// Initialize Preset 0 (read-only defaults) from current RuntimeCfg defaults
// Called once at module startup to establish the baseline configuration
void PowerSmoothing::initialize_default_preset()
{
    using namespace nv::mctp;

    // Create a default RuntimeCfg to extract defaults from
    const RuntimeCfg defaults;

    auto& preset0 = preset_manager.presets.at(DEFAULT_PRESET);

    // ========================================================================
    // EDPP Parameters (0-9)
    // ========================================================================

    // EDPP Critical PID (Primary)
    preset0
        .params[static_cast<uint8_t>(RackPwrSmoothParams::EdppSoCTargetPrimary)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.critical_pid.target);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::EdppPrimaryKp)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.critical_pid.kp);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::EdppPrimaryKi)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.critical_pid.ki);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::EdppPrimaryKd)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.critical_pid.kd);

    // EDPP Residency threshold
    preset0.params
        [static_cast<uint8_t>(RackPwrSmoothParams::EdppLowSocResidencyThreshold)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.residency_threshold);

    // EDPP Residency PID (Secondary)
    preset0.params
        [static_cast<uint8_t>(RackPwrSmoothParams::EdppResidencyTargetSecondary)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.residency_pid.target);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::EdppSecondaryKp)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.residency_pid.kp);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::EdppSecondaryKi)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.residency_pid.ki);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::EdppSecondaryKd)] = static_cast<
        uint32_t>(defaults.edpp_offset_policy.residency_pid.kd);

    // ========================================================================
    // ISINK Parameters (10-19)
    // ========================================================================

    // ISINK Critical PID (Primary)
    preset0
        .params[static_cast<uint8_t>(RackPwrSmoothParams::IsinkSoCTargetPrimary)] = static_cast<
        uint32_t>(defaults.isink_offset_policy.critical_pid.target);
    preset0
        .params[static_cast<uint8_t>(RackPwrSmoothParams::IsinkSoCCriticalHigh)] = static_cast<
        uint32_t>(defaults.isink_offset_policy.critical_pid.target);  // Note: using same target
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::IsinkPrimaryKp)] = static_cast<
        uint32_t>(defaults.isink_offset_policy.critical_pid.kp);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::IsinkPrimaryKi)] = static_cast<
        uint32_t>(defaults.isink_offset_policy.critical_pid.ki);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::IsinkPrimaryKd)] = static_cast<
        uint32_t>(defaults.isink_offset_policy.critical_pid.kd);

    // ISINK Residency threshold
    preset0.params[static_cast<uint8_t>(
        RackPwrSmoothParams::
            IsinkHighSoCResidencyThreshold)] = static_cast<uint32_t>(defaults
                                                                         .isink_offset_policy
                                                                         .residency_threshold);

    // ISINK Residency PID (Secondary)
    preset0.params[static_cast<uint8_t>(
        RackPwrSmoothParams::
            IskinkResidencyTargetSecondary)] = static_cast<uint32_t>(defaults
                                                                         .isink_offset_policy
                                                                         .residency_pid.target);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::IsinkSecondaryKp)] = static_cast<
        uint32_t>(defaults.isink_offset_policy.residency_pid.kp);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::IsinkSecondaryKi)] = static_cast<
        uint32_t>(defaults.isink_offset_policy.residency_pid.ki);
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::IsinkSecondaryKd)] = static_cast<
        uint32_t>(defaults.isink_offset_policy.residency_pid.kd);

    // ========================================================================
    // Override Parameters (40-42) - Disabled by default
    // ========================================================================

    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::OverrideSoCInput)]          = 0;
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::OverrideEdppOffsetOutput)]  = 0;
    preset0.params[static_cast<uint8_t>(RackPwrSmoothParams::OverrideIsinkOffsetOutput)] = 0;

    // Note: Gap parameters [20-39] remain zero-initialized (reserved for future use)

    // Mark Preset 0 as valid and read-only
    preset0.is_valid = true;
}

// Apply pending preset change (called at ISR start)
// Converts preset parameters to RuntimeCfg and updates power_manager
void PowerSmoothing::apply_preset_change()
{
    const uint8_t new_preset = preset_manager.pending_preset_id;

    // Build RuntimeCfg from the new preset
    const RuntimeCfg new_cfg = PowerSmoothing::build_runtime_cfg_from_preset(new_preset);

    // Update power manager configuration
    power_manager.config = new_cfg;

    // Reset all PID controllers and moving averages since parameters changed
    // This clears integral terms and error history to prevent transients
    power_manager.isink_offset_policy.reset();
    power_manager.edpp_offset_policy.reset();
    power_manager.power_brake_policy.reset();

    // Update active preset tracking
    preset_manager.active_preset_id  = new_preset;
    preset_manager.apply_pending     = false;
    preset_manager.pending_preset_id = INVALID_PRESET_ID;
}

TelemetrySnapshot PowerSmoothing::get_telemetry_snapshot()
{
    const uint32_t ticks_us     = ctimer::Driver::read_ticks();
    const uint64_t timestamp_ms = static_cast<uint64_t>(ticks_us) / 1000;

    return TelemetrySnapshot{
        .timestamp_ms             = timestamp_ms,
        .soc_percent_filtered_sum = power_manager.telemetry_accumulators
                                        .soc_percent_filtered_sum,
        .pwr_brake_time_ms = power_manager.telemetry_accumulators.pwr_brake_time_ms,
        .edpp_offset_sum   = power_manager.telemetry_accumulators.edpp_offset_sum,
        .isink_offset_sum  = power_manager.telemetry_accumulators.isink_offset_sum,
    };
}

TelemetryHistorySnapshot PowerSmoothing::get_telemetry_history_snapshot()
{
    const uint32_t ticks_us     = ctimer::Driver::read_ticks();
    const uint64_t timestamp_ms = static_cast<uint64_t>(ticks_us) / 1000;

    TelemetryHistorySnapshot snapshot{
        .timestamp_ms = timestamp_ms,
        .sample_count = PowerManager::TelemetryHistory::BUFFER_SIZE,
    };

    // Reorder circular buffer from oldest to newest
    // write_index points to the next write position, so it's the oldest data
    const uint8_t start_idx = power_manager.telemetry_history.write_index;

    for (size_t i = 0; i < PowerManager::TelemetryHistory::BUFFER_SIZE; i++) {
        const uint8_t src_idx = (start_idx + i) % PowerManager::TelemetryHistory::BUFFER_SIZE;
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
        snapshot.soc_percent_filtered[i] = power_manager.telemetry_history
                                               .soc_percent_filtered[src_idx];
        snapshot.edpp_offset[i]  = power_manager.telemetry_history.edpp_offset[src_idx];
        snapshot.isink_offset[i] = power_manager.telemetry_history.isink_offset[src_idx];
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
    }

    return snapshot;
}

uint16_t PowerSmoothing::get_last_adc_raw()
{
    return power_manager.public_connectors.last_adc_raw;
}

// The controller runs every 100us. It is not feasible to increase the RTOS tick
// rate to support running this in a task as the context switch overhead would
// be too high. Instead, the controller is run in the ADC interrupt handler. The
// ADC measuring the state of charge signal must be configured to run
// continuously such that it performs a sample every 100us and raises an
// interrupt when the sample is ready. The performace of this function is
// critical due to the frequency that it is called. Thus, all functions called
// by it must implemented inline such that the use of the [[gnu::flatten]]
// attribute ensures that the functions are inlined. To ensure instruction
// fetching is not a bottleneck, the LPCAC is disabled during the execution of
// this function and the function is placed in the RAMX SRAM block. Note that
// this approach assumes core1 is not fetching instructions from the RAMX SRAM
// as well, or else there will be contention which will degrade performance. See
// the MCXNx4x datasheet for more details about the memory subsystem.

// NOLINTBEGIN
NV_SRAMX_CODE void PowerSmoothing::adc_isr()
{
    sys::lpcac::Lpcac::disable();
    const uint32_t current_tick = ctimer::Driver::read_ticks_inline();
    time_since_last_callback    = current_tick - last_callback_time;
    if (time_since_last_callback > 200) {
        callback_time_exceeded++;
    }
    last_callback_time = current_tick;

    // Apply pending preset change BEFORE running the algorithm
    // This ensures new parameters take effect immediately
    if (preset_manager.apply_pending) {
        apply_preset_change();
    }

    // Process power smoothing calculation
    // Note: we assume data is ready since interrupt fires on FIFO watermark
    PowerSmoothing::power_manager.run_iteration();

    callback_exec_time = ctimer::Driver::read_ticks_inline() - current_tick;
    if (callback_exec_time > 25) {
        callback_exec_time_exceeded++;
    }
    sys::lpcac::Lpcac::enable();
}
// NOLINTEND

void PowerSmoothing::start_power_manager()
{
    nv::logger::info(nv::logger::Event::SocPwrSmoothingStarted,
                     nv::logger::data_from_u32(static_cast<uint32_t>(1)));
    sys::adc::ADC::trigger_read(SocAdcPeripheral, SocAdcInitialTriggerCommand);
}

void PowerSmoothing::SetPowerBrakePolicyState(PowerBrakeState state)
{
    power_manager.config.power_brake_policy.enabled = (state
                                                       == PowerBrakeState::PowerBrakeEnabled);
}

PowerBrakeState PowerSmoothing::GetPowerBrakePolicyState()
{
    return power_manager.config.power_brake_policy.enabled
             ? PowerBrakeState::PowerBrakeEnabled
             : PowerBrakeState::PowerBrakeDisabled;
}

void PowerSmoothing::SetThermBrakePolicyState(ThermBrakeState state)
{
    power_manager.config.therm_brake_policy.enabled = (state
                                                       == ThermBrakeState::ThermBrakeEnabled);
}

ThermBrakeState PowerSmoothing::GetThermBrakePolicyState()
{
    return power_manager.config.therm_brake_policy.enabled
             ? ThermBrakeState::ThermBrakeEnabled
             : ThermBrakeState::ThermBrakeDisabled;
}

void PowerSmoothing::SetOffsetPolicyState(ConstantPowerMode mode)
{
    const bool enabled = (mode == ConstantPowerMode::ConstantPowerModeOn);
    power_manager.config.isink_offset_policy.enabled = enabled;
    power_manager.config.edpp_offset_policy.enabled  = enabled;
}

ConstantPowerMode PowerSmoothing::GetOffsetPolicyState()
{
    // Both policies should have the same state, so we check either one
    return power_manager.config.isink_offset_policy.enabled
             ? ConstantPowerMode::ConstantPowerModeOn
             : ConstantPowerMode::ConstantPowerModeOff;
}

void PowerSmoothing::SetMaxACRampRate(float rate)
{
    power_manager.config.max_ac_ramp_rate = rate;

    // Automatically enable/disable offset policies based on ramp rate
    // Non-zero rate enables policies, zero rate disables them
    const bool enabled                               = (rate != 0.0f);
    power_manager.config.isink_offset_policy.enabled = enabled;
    power_manager.config.edpp_offset_policy.enabled  = enabled;
}

float PowerSmoothing::GetMaxACRampRate()
{
    return power_manager.config.max_ac_ramp_rate;
}

// ============================================================================
// Persistence Helper Functions (PDS read/write)
// Uses nv::flash::Flash::get_data() and set_data() for PDS access
// ============================================================================

// Persist SoC Power Smoothing enabled state to PDS
bool PowerSmoothing::PersistSoCPowerSmoothEnabled(bool enabled)
{
    const nv::flash::Data value = enabled ? 1 : 0;
    auto status = nv::flash::Flash::set_data(nv::flash::Key::PdsSoCPowerSmoothEnabled, value);
    if (status != nv::flash::Status::Ok) {
        return false;
    }
    return true;
}

// Persist SoC Power Brake enabled state to PDS
bool PowerSmoothing::PersistSoCPowerBrakeEnabled(bool enabled)
{
    const nv::flash::Data value = enabled ? 1 : 0;
    auto status = nv::flash::Flash::set_data(nv::flash::Key::PdsSoCPowerBrakeEnabled, value);
    if (status != nv::flash::Status::Ok) {
        return false;
    }
    return true;
}

// Persist current preset index to PDS
bool PowerSmoothing::PersistSoCPowerSmoothCurrentPresetIndex(uint8_t preset_id)
{
    const auto value  = static_cast<nv::flash::Data>(preset_id);
    auto       status = nv::flash::Flash::set_data(
        nv::flash::Key::PdsSoCPowerSmoothCurrentPresetIndex, value);
    if (status != nv::flash::Status::Ok) {
        return false;
    }
    return true;
}

// Persist max AC power ramp rate to PDS (float stored as uint32_t via bit_cast)
bool PowerSmoothing::PersistMaxACPowerRampRate(float rate)
{
    const auto value = std::bit_cast<nv::flash::Data>(rate);
    auto status      = nv::flash::Flash::set_data(nv::flash::Key::PdsMaxACPowerRampRate, value);
    if (status != nv::flash::Status::Ok) {
        return false;
    }
    return true;
}

// Persist SoC Thermal Brake enabled state to PDS
bool PowerSmoothing::PersistSoCThermBrakeEnabled(bool enabled)
{
    const nv::flash::Data value = enabled ? 1 : 0;
    auto status = nv::flash::Flash::set_data(nv::flash::Key::PdsSoCThermBrakeEnabled, value);
    if (status != nv::flash::Status::Ok) {
        return false;
    }
    return true;
}

// Load all persisted settings from PDS and apply them
// Called at startup after flash task is initialized
void PowerSmoothing::LoadPersistedSettings()
{
    nv::flash::Data value = 0;

    // Load SoC Power Smooth Enabled
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsSoCPowerSmoothEnabled, value)
        == nv::flash::Status::Ok) {
        const bool enabled = (value == 1);
        PowerSmoothing::SetOffsetPolicyState(enabled ? ConstantPowerMode::ConstantPowerModeOn
                                                     : ConstantPowerMode::ConstantPowerModeOff);
    }

    // Load SoC Power Brake Enabled
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsSoCPowerBrakeEnabled, value)
        == nv::flash::Status::Ok) {
        const bool enabled = (value == 1);
        PowerSmoothing::SetPowerBrakePolicyState(enabled ? PowerBrakeState::PowerBrakeEnabled
                                                         : PowerBrakeState::PowerBrakeDisabled);
    }

    // Load Max AC Power Ramp Rate
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsMaxACPowerRampRate, value)
        == nv::flash::Status::Ok) {
        const auto rate = std::bit_cast<float>(value);
        // Set ramp rate directly without triggering auto-enable logic
        // (the enabled state was already loaded above)
        power_manager.config.max_ac_ramp_rate = rate;
    }

    // Load Current Preset Index
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsSoCPowerSmoothCurrentPresetIndex, value)
        == nv::flash::Status::Ok) {
        const auto preset_id = static_cast<uint8_t>(value);
        if (preset_id < NUM_PRESETS) {
            // Switch to the persisted preset (will be applied at next ISR)
            PowerSmoothing::SwitchToPreset(preset_id);
        }
    }

    // Load Thermal Brake Enabled
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsSoCThermBrakeEnabled, value)
        == nv::flash::Status::Ok) {
        const bool enabled = (value == 1);
        PowerSmoothing::SetThermBrakePolicyState(enabled ? ThermBrakeState::ThermBrakeEnabled
                                                         : ThermBrakeState::ThermBrakeDisabled);
    }
}

// ============================================================================
// Preset Parameter Management API
// ============================================================================

bool PowerSmoothing::SetParameter(uint8_t preset_id, uint8_t param_id, uint32_t value)
{
    using namespace nv::mctp;

    // Validate preset ID (must be writable: 1-3, not read-only 0)
    if (preset_id < MIN_WRITABLE_PRESET || preset_id > MAX_WRITABLE_PRESET) {
        return false;
    }

    // Validate parameter ID (check for valid range, excluding reserved gap)
    if (!is_valid_param_id(param_id)) {
        return false;
    }

    // Validate parameter value
    auto param_enum = static_cast<RackPwrSmoothParams>(param_id);
    if (!PowerSmoothing::validate_parameter(param_enum, value)) {
        return false;
    }

    // Initialize preset from defaults if not yet initialized
    if (!preset_manager.presets.at(preset_id).is_valid) {
        PowerSmoothing::initialize_preset_from_default(preset_id);
    }

    // Update the parameter - param_id is array index directly
    preset_manager.presets.at(preset_id).params.at(param_id) = value;

    // If modifying the currently active preset, re-apply it immediately
    // This ensures changes take effect without requiring an explicit SwitchToPreset call
    if (preset_id == preset_manager.active_preset_id) {
        // Note: SwitchToPreset will validate the entire preset before applying
        PowerSmoothing::SwitchToPreset(preset_id);
    }

    return true;
}

void PowerSmoothing::GetPresetParameters(uint8_t                            preset_id,
                                         std::array<uint32_t, PARAM_COUNT>& new_params)
{
    // Return empty array for invalid preset
    if (preset_id >= NUM_PRESETS) {
        new_params.fill(0);
        return;
    }

    // Return empty array for uninitialized preset
    if (!preset_manager.presets.at(preset_id).is_valid) {
        new_params.fill(0);
        return;
    }

    new_params = preset_manager.presets.at(preset_id).params;
}

bool PowerSmoothing::SwitchToPreset(uint8_t preset_id)
{
    // Validate preset ID
    if (preset_id >= NUM_PRESETS) {
        return false;
    }

    // Check if preset is initialized
    if (!preset_manager.presets.at(preset_id).is_valid) {
        return false;
    }

    // Validate entire preset before switching (safety check)
    if (!PowerSmoothing::validate_preset(preset_manager.presets.at(preset_id))) {
        return false;
    }

    // Mark for application at next ISR iteration
    preset_manager.pending_preset_id = preset_id;
    preset_manager.apply_pending     = true;

    return true;
}

uint8_t PowerSmoothing::GetActivePresetId()
{
    return preset_manager.active_preset_id;
}

uint8_t PowerSmoothing::GetSupportedPresetBitmask()
{
    uint8_t bitmask = 0;
    for (uint8_t i = 0; i < NUM_PRESETS; i++) {
        if (preset_manager.presets.at(i).is_valid) {
            bitmask |= (1u << i);
        }
    }
    return bitmask;
}

void PowerSmoothing::initialize()
{
    // Initialize Preset 0 (read-only defaults) at startup
    PowerSmoothing::initialize_default_preset();

    // Initialize preset_manager state (required for BSS zero-initialization)
    // pending_preset_id must be INVALID_PRESET_ID (0xFF) to indicate no pending change
    preset_manager.pending_preset_id = INVALID_PRESET_ID;

    // Apply default preset to power_manager.config immediately
    // This ensures valid configuration even if PDS read fails in LoadPersistedSettings()
    // Critical for BSS: RuntimeCfg has non-zero defaults that won't be set by zero-init
    power_manager.config = build_runtime_cfg_from_preset(DEFAULT_PRESET);

    // Note: ADC sampling and persisted settings are loaded in periodic_update()
    // after scheduler is running and flash task is active
}

void PowerSmoothing::timer_callback([[maybe_unused]] ipc::Timer& timer)
{
    periodic_update();
}

void PowerSmoothing::periodic_update()
{
    static bool        initialized               = false;
    static bool        power_brake_asserted      = false;
    static bool        callback_exec_time_logged = false;
    static bool        callback_time_logged      = false;
    constexpr uint32_t exe_threshold             = 9;
    constexpr uint32_t time_threshold            = 9;

    // One-time initialization on first call (after scheduler and flash task are running)
    if (!initialized) {
        // Load persisted settings from flash (requires flash task to be running)
        LoadPersistedSettings();

        // Start ADC sampling (requires interrupts to be enabled)
        start_power_manager();

        initialized = true;
    }

    if (SocPowerSmoothingDebugEnabled) {
        nv::logger::info(nv::logger::Event::SocPwrSmoothingSocPercent,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.public_connectors.soc_percent_avg
                             >> sfxp32_0_to_sfxp22_10_lshift)));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingAssertPowerBrake,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.public_connectors.assert_power_brake)));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingEdppOffset,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.public_connectors.edpp_offset_avg
                             >> sfxp32_0_to_sfxp22_10_lshift)));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingIsinkOffset,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.public_connectors.isink_offset_avg
                             >> sfxp32_0_to_sfxp22_10_lshift)));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingTimeSinceLastCallback,
                         nv::logger::data_from_u32(PowerSmoothing::time_since_last_callback));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingCallbackExecutionTime,
                         nv::logger::data_from_u32(PowerSmoothing::callback_exec_time));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingEdppResidency,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.edpp_offset_policy.get_residency())));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingIsinkResidency,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.isink_offset_policy.get_residency())));
    }

    // The following are single event loggers that are only logged once per event.
    if (power_manager.public_connectors.assert_power_brake && !power_brake_asserted) {
        power_brake_asserted = true;
        nv::logger::info(nv::logger::Event::SocPwrSmoothingAssertPowerBrake,
                         nv::logger::data_from_u32(static_cast<uint32_t>(1)));
    }
    else if (!power_manager.public_connectors.assert_power_brake && power_brake_asserted) {
        power_brake_asserted = false;
        nv::logger::info(nv::logger::Event::SocPwrSmoothingAssertPowerBrake,
                         nv::logger::data_from_u32(static_cast<uint32_t>(0)));
    }
    if (PowerSmoothing::callback_exec_time_exceeded > exe_threshold
        && !callback_exec_time_logged) {
        nv::logger::info(
            nv::logger::Event::SocPwrSmoothingCallbackExecutionTimeExceeded,
            nv::logger::data_from_u32(PowerSmoothing::callback_exec_time_exceeded));
        callback_exec_time_logged = true;
    }
    else {
        PowerSmoothing::callback_exec_time_exceeded = 0;
    }
    if (PowerSmoothing::callback_time_exceeded > time_threshold && !callback_time_logged) {
        // One callback time violation is expected on startup
        nv::logger::info(nv::logger::Event::SocPwrSmoothingCallbackTimeExceeded,
                         nv::logger::data_from_u32(PowerSmoothing::callback_time_exceeded));
        callback_time_logged = true;
    }
    else {
        PowerSmoothing::callback_time_exceeded = 0;
    }
}

}  // namespace nv::soc_pwr_smoothing
