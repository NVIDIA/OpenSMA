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

#include <cstdint>

// Forward declarations to avoid including full nsm.h
namespace nv::mctp {
struct NsmPktResp;
}  // namespace nv::mctp

// Forward declare Ccode from enums.h
namespace pdk::mctp::platforms {
enum class Ccode : uint8_t;
}  // namespace pdk::mctp::platforms

namespace nv::mctp {
using Ccode = pdk::mctp::platforms::Ccode;
}  // namespace nv::mctp

namespace nv::mctp::nsm_pwr_smoothing_handlers {

/**
 * @brief Handler interface for SoC Power Smoothing feature NSM commands.
 *
 * These functions have weak default implementations that return false (feature
 * not available). Projects that include the soc_pwr_smoothing module will
 * automatically override these with strong implementations via the linker.
 *
 * Supports both NSM Type-5 (Device Configuration) and Type-FF (NV Internal)
 * commands related to power smoothing.
 *
 * Return value:
 *   true  - Command handled successfully, response populated in ntx
 *   false - Feature not available, caller should return ErrorUnsupportedCmd
 */

// ============================================================================
// Type-5: Get Device Mode Settings handlers (populate ntx with response data)
// ============================================================================

/**
 * @brief Get Max AC Power Ramp Rate setting
 * @param[out] ntx Response packet to populate with current ramp rate (float)
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_get_max_ac_ramp_rate(NsmPktResp& ntx);

/**
 * @brief Get SoC Power Smooth Enabled setting
 * @param[out] ntx Response packet to populate with enabled state (uint8_t)
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_get_soc_power_smooth_enabled(NsmPktResp& ntx);

/**
 * @brief Get SoC Power Smooth Current Preset Index setting
 * @param[out] ntx Response packet to populate with preset ID (uint8_t)
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_get_soc_power_smooth_current_preset(NsmPktResp& ntx);

/**
 * @brief Get SoC Power Brake Enabled setting
 * @param[out] ntx Response packet to populate with brake state (uint8_t)
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_get_soc_power_brake_enabled(NsmPktResp& ntx);

/**
 * @brief Get SoC Thermal Brake Enabled setting
 * @param[out] ntx Response packet to populate with brake state (uint8_t)
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_get_soc_therm_brake_enabled(NsmPktResp& ntx);

// ============================================================================
// Type-5: Set Device Mode Settings handlers (apply settings from request)
// ============================================================================

/**
 * @brief Set Max AC Power Ramp Rate (also persists to PDS)
 * @param[in] ramp_rate New ramp rate value (W/s)
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_set_max_ac_ramp_rate(float ramp_rate);

/**
 * @brief Set SoC Power Smooth Enabled state (also persists to PDS)
 * @param[in] enabled true to enable, false to disable
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_set_soc_power_smooth_enabled(bool enabled);

/**
 * @brief Set SoC Power Smooth Current Preset Index (also persists to PDS)
 * @param[in] preset_id Preset ID to switch to (0-3)
 * @return Ccode::Success if handled and preset valid, Ccode::ErrorUnsupportedCmd if feature not
 * available or invalid preset
 */
Ccode handle_set_soc_power_smooth_current_preset(uint8_t preset_id);

/**
 * @brief Set SoC Power Brake Enabled state (also persists to PDS)
 * @param[in] enabled true to enable, false to disable
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_set_soc_power_brake_enabled(bool enabled);

/**
 * @brief Set SoC Thermal Brake Enabled state (also persists to PDS)
 * @param[in] enabled true to enable, false to disable
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_set_soc_therm_brake_enabled(bool enabled);

// ============================================================================
// Type-FF (NV Internal) Rack Power Smoothing handlers
// ============================================================================

/**
 * @brief Get Rack Power Smoothing Parameters (tuning params 0 .. MaxTuningParams-1)
 * @param[out] ntx Response packet to populate with parameters
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_get_rack_power_smoothing_param(NsmPktResp& ntx);

/**
 * @brief Set Rack Power Smoothing Parameter (tuning params 0 .. MaxTuningParams-1)
 * @param[in] preset_id Preset to modify (1-3, 0 is read-only)
 * @param[in] param_id Parameter ID (see RackPwrSmoothParams)
 * @param[in] param_value SFXP22_10 for most tuning params; IEEE-754 bits as uint32_t for float
 * params 20-21; plain integer 1-1000 for PID dt divisors 28-31 (see RackPwrSmoothParams)
 * @return Ccode::Success if handled successfully, Ccode::ErrorUnsupportedCmd if feature not
 * available, Ccode::ErrorInvalidData for invalid params
 */
Ccode handle_set_rack_power_smoothing_param(uint8_t  preset_id,
                                            uint8_t  param_id,
                                            uint32_t param_value);

/**
 * @brief Get Rack Power Smoothing TestHook Parameters (params 40-44, requires debug token)
 * @param[out] ntx Response packet to populate with parameters
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_get_rack_power_smoothing_testhook(NsmPktResp& ntx);

/**
 * @brief Set Rack Power Smoothing TestHook Parameter (params 40-44, requires debug token)
 * @param[in] preset_id Preset to modify (1-3, 0 is read-only)
 * @param[in] param_id Parameter ID (40-44; 43-44 raw DAC override if enabled wins vs 41-42)
 * @param[in] param_value Parameter value
 * @return Ccode::Success if handled successfully, Ccode::ErrorUnsupportedCmd if feature not
 * available, Ccode::ErrorInvalidData for invalid params
 */
Ccode handle_set_rack_power_smoothing_testhook(uint8_t  preset_id,
                                               uint8_t  param_id,
                                               uint32_t param_value);

/**
 * @brief Get Debug Telemetry data
 * @param[in] telem_type Telemetry type to retrieve
 * @param[out] ntx Response packet to populate with telemetry data
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available,
 * Ccode::ErrorInvalidData for invalid type
 */
Ccode handle_get_debug_telemetry(uint16_t telem_type, NsmPktResp& ntx);

// ============================================================================
// Type-FF: ADC Calibration Test Mode handlers
// ============================================================================

/**
 * @brief Trigger ADC Calibration Test sequence
 * Starts a 10-point calibration loop (0V to 2.5V) using external I2C DAC.
 * Results are stored in flash upon completion (~11 seconds).
 * @return Ccode::Success if calibration started, Ccode::ErrorUnsupportedCmd if feature not
 * available
 */
Ccode handle_trigger_adc_calibration();

/**
 * @brief Get ADC Calibration Results
 * Returns stored calibration data (DAC code vs ADC readback for 10 voltage points).
 * @param[out] ntx Response packet to populate with calibration results
 * @return Ccode::Success if handled, Ccode::ErrorUnsupportedCmd if feature not available
 */
Ccode handle_get_adc_calibration_results(NsmPktResp& ntx);

/**
 * @brief Set loopback DAC code for ADC calibration path (I2C).
 * @param dac_code Raw 16-bit DAC code
 * @return Ccode::Success, Ccode::ErrorI2CError on bus failure, Ccode::ErrorUnsupportedCmd if
 * unavailable
 */
Ccode handle_adc_calib_set_loopback_dac_code(uint16_t dac_code);

/**
 * @brief Return last raw code for SoC ADC or EDPP/ISINK DAC (see PwrSmoothRawReadbackId).
 * @param readback_id Selector (0=SoC ADC, 1=EDPP DAC shadow, 2=ISINK DAC shadow)
 * @param[out] ntx Response packet (4-byte payload)
 * @return Ccode::Success, ErrorInvalidData if readback_id unsupported, ErrorUnsupportedCmd if
 * unavailable
 */
Ccode handle_get_power_smooth_raw_readback(uint8_t readback_id, NsmPktResp& ntx);

/**
 * @brief Write one voltage calibration coefficient to PDS (IEEE-754 float32 as uint32_t).
 * @param coeff_id PwrSmoothVoltageCalibCoeffId (0-14)
 * @param coefficient_value Float bit pattern (little-endian)
 * @return Ccode::Success, ErrorInvalidData if coeff_id invalid, ErrorGeneral on PDS failure
 */
Ccode handle_set_soc_calib_coefficient(uint8_t coeff_id, uint32_t coefficient_value);

/**
 * @brief Read one voltage calibration coefficient from PDS.
 * @param coeff_id PwrSmoothVoltageCalibCoeffId (0-14)
 * @param[out] ntx Response with NsmTFFGetSocCalibCoefficientRes payload
 */
Ccode handle_get_soc_calib_coefficient(uint8_t coeff_id, NsmPktResp& ntx);

}  // namespace nv::mctp::nsm_pwr_smoothing_handlers
