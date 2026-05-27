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

/**
 * @file nsm_pwr_smoothing_handlers_weak.cpp
 * @brief Weak default implementations for SoC Power Smoothing NSM handlers.
 *
 * This file provides weak symbol implementations that return false (feature not
 * available) for all SoC Power Smoothing related handlers (both Type-5 and
 * Type-FF commands). These are compiled for ALL projects.
 *
 * Projects that include the nv/soc_pwr_smoothing module will have strong
 * implementations that automatically override these weak symbols at link time.
 *
 * This pattern eliminates scattered #ifdef ENABLE_SOC_PWR_SMOOTHING guards
 * throughout the codebase while maintaining zero runtime overhead.
 */

#include "nv/mctp/nsm_pwr_smoothing_handlers.h"
#include "nv/mctp/nsm.h"

namespace nv::mctp::nsm_pwr_smoothing_handlers {

// ============================================================================
// Weak Type-5 Get handlers - return ErrorUnsupportedCmd to indicate feature not available
// ============================================================================

__attribute__((weak)) Ccode handle_get_max_ac_ramp_rate([[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode
handle_get_soc_power_smooth_enabled([[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode
handle_get_soc_power_smooth_current_preset([[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_get_soc_power_brake_enabled([[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_get_soc_therm_brake_enabled([[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

// ============================================================================
// Weak Type-5 Set handlers - return ErrorUnsupportedCmd to indicate feature not available
// ============================================================================

__attribute__((weak)) Ccode handle_set_max_ac_ramp_rate([[maybe_unused]] float ramp_rate)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_set_soc_power_smooth_enabled([[maybe_unused]] bool enabled)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode
handle_set_soc_power_smooth_current_preset([[maybe_unused]] uint8_t preset_id)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_set_soc_power_brake_enabled([[maybe_unused]] bool enabled)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_set_soc_therm_brake_enabled([[maybe_unused]] bool enabled)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

// ============================================================================
// Weak Type-FF handlers - return ErrorUnsupportedCmd to indicate feature not available
// ============================================================================

__attribute__((weak)) Ccode
handle_get_rack_power_smoothing_param([[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode
handle_set_rack_power_smoothing_param([[maybe_unused]] uint8_t  preset_id,
                                      [[maybe_unused]] uint8_t  param_id,
                                      [[maybe_unused]] uint32_t param_value)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode
handle_get_rack_power_smoothing_testhook([[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode
handle_set_rack_power_smoothing_testhook([[maybe_unused]] uint8_t  preset_id,
                                         [[maybe_unused]] uint8_t  param_id,
                                         [[maybe_unused]] uint32_t param_value)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_get_debug_telemetry([[maybe_unused]] uint16_t    telem_type,
                                                       [[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

// ============================================================================
// Weak Type-FF ADC Calibration handlers - return ErrorUnsupportedCmd for unavailable feature
// ============================================================================

__attribute__((weak)) Ccode handle_trigger_adc_calibration()
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_get_adc_calibration_results([[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode
handle_adc_calib_set_loopback_dac_code([[maybe_unused]] uint16_t dac_code)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_get_power_smooth_raw_readback(
    [[maybe_unused]] uint8_t readback_id, [[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_set_soc_calib_coefficient(
    [[maybe_unused]] uint8_t coeff_id, [[maybe_unused]] uint32_t coefficient_value)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

__attribute__((weak)) Ccode handle_get_soc_calib_coefficient([[maybe_unused]] uint8_t coeff_id,
                                                             [[maybe_unused]] NsmPktResp& ntx)
{
    return Ccode::ErrorUnsupportedCmd;  // Feature not available
}

}  // namespace nv::mctp::nsm_pwr_smoothing_handlers
