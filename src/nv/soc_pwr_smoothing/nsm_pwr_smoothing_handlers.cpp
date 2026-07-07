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
 * @file nsm_pwr_smoothing_handlers.cpp
 * @brief Strong implementations for SoC Power Smoothing NSM handlers.
 *
 * This file provides the actual implementations for SoC Power Smoothing
 * handlers (both Type-5 and Type-FF commands). It is only compiled for projects
 * that include the soc_pwr_smoothing module, and the linker will automatically
 * override the weak symbols from nsm_pwr_smoothing_handlers_weak.cpp.
 */

#include <array>
#include <cstdint>
#include <cstring>

#include "nv/flash/flash.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/nsm_pwr_smoothing_handlers.h"
#include "nv/soc_pwr_smoothing/soc_pwr_smoothing.h"

namespace nv::mctp::nsm_pwr_smoothing_handlers {

namespace {

// Type-5 Get Device Mode response:
//   NvU16 current_length, NvU16 pending_length (0 — changes apply immediately), then payload.
constexpr uint16_t kDeviceModePendingLength = 0;

void fill_device_mode_get_response(NsmPktResp& ntx,
                                   const void* current_payload,
                                   uint16_t    current_payload_size)
{
    memcpy(&ntx.data[0], &current_payload_size, sizeof(uint16_t));
    memcpy(&ntx.data[2], &kDeviceModePendingLength, sizeof(uint16_t));
    if (current_payload_size > 0) {
        memcpy(&ntx.data[4], current_payload, current_payload_size);
    }
    ntx.data_size = (2U * sizeof(uint16_t)) + current_payload_size;
}

void fill_device_mode_get_u8(NsmPktResp& ntx, uint8_t value)
{
    fill_device_mode_get_response(ntx, &value, sizeof(value));
}

void fill_device_mode_get_u32(NsmPktResp& ntx, uint32_t value)
{
    fill_device_mode_get_response(ntx, &value, sizeof(value));
}

enum class RackParamSlice : bool
{
    Tuning   = false,
    TestHook = true,
};

Ccode get_rack_power_smoothing_params(NsmPktResp& ntx, RackParamSlice slice)
{
    using namespace nv::soc_pwr_smoothing;
    using nv::mctp::RackPwrSmoothParams;

    auto param_start = (slice == RackParamSlice::Tuning)
                         ? 0U
                         : static_cast<uint8_t>(RackPwrSmoothParams::TestHookParamsStart);
    auto param_count = (slice == RackParamSlice::Tuning)
                         ? static_cast<uint8_t>(RackPwrSmoothParams::MaxTuningParams)
                         : static_cast<uint8_t>(RackPwrSmoothParams::MaxParamCount)
                               - param_start;

    auto& response = *std::bit_cast<nv::mctp::NsmTFFGetRackPwrSmoothParamRes*>(&ntx.data[0]);

    const uint8_t                     active_preset = PowerSmoothing::GetActivePresetId();
    std::array<uint32_t, PARAM_COUNT> params{};
    PowerSmoothing::GetPresetParameters(active_preset, params);

    response.numParams       = param_count;
    response.currentPresetId = active_preset;
    response.resvd           = 0;

    for (uint8_t i = 0; i < param_count; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        response.paramArray[i] = params[param_start + i];
    }

    ntx.data_size = sizeof(response) + (sizeof(uint32_t) * response.numParams);
    return Ccode::Success;
}

Ccode set_rack_power_smoothing_param(uint8_t        preset_id,
                                     uint8_t        param_id,
                                     uint32_t       param_value,
                                     RackParamSlice slice)
{
    using nv::mctp::RackPwrSmoothParams;

    if (slice == RackParamSlice::Tuning) {
        if (param_id >= static_cast<uint8_t>(RackPwrSmoothParams::MaxTuningParams)) {
            return Ccode::ErrorInvalidData;
        }
    }
    else if (param_id < static_cast<uint8_t>(RackPwrSmoothParams::TestHookParamsStart)
             || param_id >= static_cast<uint8_t>(RackPwrSmoothParams::MaxParamCount)) {
        return Ccode::ErrorInvalidData;
    }

    return nv::soc_pwr_smoothing::PowerSmoothing::SetParameter(preset_id, param_id, param_value)
             ? Ccode::Success
             : Ccode::ErrorGeneral;
}

}  // namespace

// ============================================================================
// Strong Type-5 Get handlers - actual implementations
// ============================================================================

Ccode handle_get_max_ac_ramp_rate(NsmPktResp& ntx)
{
    fill_device_mode_get_u32(ntx, nv::soc_pwr_smoothing::PowerSmoothing::GetMaxACRampRate());
    return Ccode::Success;
}

Ccode handle_get_soc_power_smooth_enabled(NsmPktResp& ntx)
{
    auto enabled = static_cast<uint8_t>(
        nv::soc_pwr_smoothing::PowerSmoothing::GetOffsetPolicyState());
    fill_device_mode_get_u8(ntx, enabled);
    return Ccode::Success;
}

Ccode handle_get_soc_power_smooth_current_preset(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    const std::array<uint8_t, 2> preset_payload{PowerSmoothing::GetActivePresetId(),
                                                PowerSmoothing::GetSupportedPresetBitmask()};
    fill_device_mode_get_response(ntx, preset_payload.data(), preset_payload.size());
    return Ccode::Success;
}

Ccode handle_get_soc_power_brake_enabled(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto enabled = (PowerSmoothing::GetPowerBrakePolicyState()
                    == PowerBrakeState::PowerBrakeEnabled)
                     ? 1U
                     : 0U;
    fill_device_mode_get_u8(ntx, enabled);
    return Ccode::Success;
}

Ccode handle_get_soc_therm_brake_enabled(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto enabled = (PowerSmoothing::GetThermBrakePolicyState()
                    == ThermBrakeState::ThermBrakeEnabled)
                     ? 1U
                     : 0U;
    fill_device_mode_get_u8(ntx, enabled);
    return Ccode::Success;
}

// ============================================================================
// Type-5: Set Device Mode Settings handlers (strong implementations)
// ============================================================================

Ccode handle_set_max_ac_ramp_rate(uint32_t ramp_rate)
{
    nv::soc_pwr_smoothing::PowerSmoothing::SetMaxACRampRate(ramp_rate);
    // Persist to PDS for survival across power cycles
    nv::soc_pwr_smoothing::PowerSmoothing::PersistMaxACPowerRampRate(ramp_rate);
    return Ccode::Success;
}

Ccode handle_set_soc_power_smooth_enabled(bool enabled)
{
    using namespace nv::soc_pwr_smoothing;

    PowerSmoothing::SetOffsetPolicyState(enabled ? ConstantPowerMode::ConstantPowerModeOn
                                                 : ConstantPowerMode::ConstantPowerModeOff);
    // Persist to PDS for survival across power cycles
    PowerSmoothing::PersistSoCPowerSmoothEnabled(enabled);
    return Ccode::Success;
}

Ccode handle_set_soc_power_smooth_current_preset(uint8_t preset_id)
{
    using namespace nv::soc_pwr_smoothing;

    // Validate preset ID is within valid range (0-3)
    if (preset_id >= NUM_PRESETS) {
        return Ccode::ErrorInvalidData;
    }

    // Switch to the requested preset
    const bool success = PowerSmoothing::SwitchToPreset(preset_id);
    if (success) {
        // Persist to PDS for survival across power cycles
        PowerSmoothing::PersistSoCPowerSmoothCurrentPresetIndex(preset_id);
    }

    return success ? Ccode::Success : Ccode::ErrorGeneral;
}

Ccode handle_set_soc_therm_brake_enabled(bool enabled)
{
    using namespace nv::soc_pwr_smoothing;

    const ThermBrakeState state = enabled ? ThermBrakeState::ThermBrakeEnabled
                                          : ThermBrakeState::ThermBrakeDisabled;
    PowerSmoothing::SetThermBrakePolicyState(state);
    // Persist to PDS for survival across power cycles
    PowerSmoothing::PersistSoCThermBrakeEnabled(enabled);
    return Ccode::Success;
}

Ccode handle_set_soc_power_brake_enabled(bool enabled)
{
    using namespace nv::soc_pwr_smoothing;

    const PowerBrakeState state = enabled ? PowerBrakeState::PowerBrakeEnabled
                                          : PowerBrakeState::PowerBrakeDisabled;
    PowerSmoothing::SetPowerBrakePolicyState(state);
    // Persist to PDS for survival across power cycles
    PowerSmoothing::PersistSoCPowerBrakeEnabled(enabled);
    return Ccode::Success;
}

// ============================================================================
// Strong Type-FF handlers - actual implementations
// ============================================================================

Ccode handle_get_rack_power_smoothing_param(NsmPktResp& ntx)
{
    return get_rack_power_smoothing_params(ntx, RackParamSlice::Tuning);
}

Ccode handle_set_rack_power_smoothing_param(uint8_t  preset_id,
                                            uint8_t  param_id,
                                            uint32_t param_value)
{
    return set_rack_power_smoothing_param(
        preset_id, param_id, param_value, RackParamSlice::Tuning);
}

Ccode handle_get_rack_power_smoothing_testhook(NsmPktResp& ntx)
{
    return get_rack_power_smoothing_params(ntx, RackParamSlice::TestHook);
}

Ccode handle_set_rack_power_smoothing_testhook(uint8_t  preset_id,
                                               uint8_t  param_id,
                                               uint32_t param_value)
{
    return set_rack_power_smoothing_param(
        preset_id, param_id, param_value, RackParamSlice::TestHook);
}

Ccode handle_get_debug_telemetry(uint16_t telem_type, NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto type = static_cast<RackPwrSmoothTelemType>(telem_type);

    // Validate telemetry type range
    if (type < RackPwrSmoothTelemType::SocTelemetryRunningSum
        || type > RackPwrSmoothTelemType::IsinkRecentHistory) {
        return Ccode::ErrorInvalidData;
    }

    auto& response    = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    response.resvd    = 0;
    uint8_t* data_ptr = &response.telemetryData[0];

    // Single switch for all telemetry types
    switch (type) {
        // Running sum types: timestamp (8 bytes) + sum (8 bytes)
        case RackPwrSmoothTelemType::SocTelemetryRunningSum:
        case RackPwrSmoothTelemType::PwrBrakeRunningSumMs:
        case RackPwrSmoothTelemType::EdppOffsetRunningSum:
        case RackPwrSmoothTelemType::ISinkOffsetRunningSum : {
            auto snapshot = PowerSmoothing::get_telemetry_snapshot();
            std::memcpy(data_ptr, &snapshot.timestamp_ms, sizeof(uint64_t));

            const uint64_t* sum_ptr = (type == RackPwrSmoothTelemType::SocTelemetryRunningSum)
                                        ? &snapshot.soc_percent_filtered_sum
                                    : (type == RackPwrSmoothTelemType::PwrBrakeRunningSumMs)
                                        ? &snapshot.pwr_brake_time_ms
                                    : (type == RackPwrSmoothTelemType::EdppOffsetRunningSum)
                                        ? &snapshot.edpp_offset_sum
                                        : &snapshot.isink_offset_sum;
            std::memcpy(data_ptr + sizeof(uint64_t), sum_ptr, sizeof(uint64_t));
            response.dataSizeBytes = 16;
            break;
        }

        // History types: timestamp (8 bytes) + sample_count (4 bytes) + samples (100 bytes)
        case RackPwrSmoothTelemType::SocRecentHistory:
        case RackPwrSmoothTelemType::EdppRecentHistory:
        case RackPwrSmoothTelemType::IsinkRecentHistory: {
            auto history = PowerSmoothing::get_telemetry_history_snapshot();
            std::memcpy(data_ptr, &history.timestamp_ms, sizeof(uint64_t));
            std::memcpy(data_ptr + sizeof(uint64_t), &history.sample_count, sizeof(uint32_t));

            constexpr size_t kHistorySamplesSize = sizeof(
                TelemetryHistorySnapshot::soc_percent_filtered);
            constexpr size_t kHistoryDataSize = sizeof(uint64_t) + sizeof(uint32_t)
                                              + kHistorySamplesSize;

            const uint8_t* samples_ptr = (type == RackPwrSmoothTelemType::SocRecentHistory)
                                           ? history.soc_percent_filtered.data()
                                       : (type == RackPwrSmoothTelemType::EdppRecentHistory)
                                           ? history.edpp_offset.data()
                                           : history.isink_offset.data();
            std::memcpy(data_ptr + sizeof(uint64_t) + sizeof(uint32_t),
                        samples_ptr,
                        kHistorySamplesSize);
            response.dataSizeBytes = kHistoryDataSize;
            break;
        }

        default: response.dataSizeBytes = 0; break;
    }

    ntx.data_size = sizeof(response) + response.dataSizeBytes;
    return Ccode::Success;
}

// ============================================================================
// Strong Type-FF ADC Calibration handlers - actual implementations
// ============================================================================

Ccode handle_trigger_adc_calibration()
{
    // Run full 26-step calibration blocking (~130ms) in MCTP task context.
    using nv::soc_pwr_smoothing::AdcCalibrationExecuteResult;
    switch (nv::soc_pwr_smoothing::execute_adc_calibration()) {
        case AdcCalibrationExecuteResult::Success        : return Ccode::Success;
        case AdcCalibrationExecuteResult::I2cFailure     : return Ccode::ErrorI2CError;
        case AdcCalibrationExecuteResult::PdsWriteFailure: return Ccode::ErrorUpdateDbFail;
    }
    return Ccode::ErrorGeneral;
}

Ccode handle_get_adc_calibration_results(NsmPktResp& ntx)
{
    auto& response = *std::bit_cast<NsmTFFGetAdcCalibResultsRes*>(&ntx.data[0]);

    // Populate the response from current state or flash
    nv::soc_pwr_smoothing::get_adc_calibration_results(response);

    ntx.data_size = sizeof(NsmTFFGetAdcCalibResultsRes);
    return Ccode::Success;
}

Ccode handle_adc_calib_set_loopback_dac_code(uint16_t dac_code)
{
    if (!nv::soc_pwr_smoothing::adc_calib_set_loopback_dac_code(dac_code)) {
        return Ccode::ErrorI2CError;
    }
    return Ccode::Success;
}

namespace {

nv::flash::Key pds_key_for_voltage_calib_coeff(uint8_t coeff_id)
{
    static_assert(static_cast<uint32_t>(nv::flash::Key::PdsPwrSmoothCalib14)
                      - static_cast<uint32_t>(nv::flash::Key::PdsPwrSmoothCalib0)
                  == static_cast<uint32_t>(PwrSmoothVoltageCalibCoeffId::MaxCount) - 1U);
    return static_cast<nv::flash::Key>(static_cast<uint32_t>(nv::flash::Key::PdsPwrSmoothCalib0)
                                       + coeff_id);
}

}  // namespace

Ccode handle_set_soc_calib_coefficient(uint8_t coeff_id, uint32_t coefficient_value)
{
    if (coeff_id >= static_cast<uint8_t>(PwrSmoothVoltageCalibCoeffId::MaxCount)) {
        return Ccode::ErrorInvalidData;
    }

    const auto key = pds_key_for_voltage_calib_coeff(coeff_id);
    if (nv::flash::Flash::set_data(key, coefficient_value) != nv::flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }
    return Ccode::Success;
}

Ccode handle_get_soc_calib_coefficient(uint8_t coeff_id, NsmPktResp& ntx)
{
    if (coeff_id >= static_cast<uint8_t>(PwrSmoothVoltageCalibCoeffId::MaxCount)) {
        return Ccode::ErrorInvalidData;
    }

    const auto      key  = pds_key_for_voltage_calib_coeff(coeff_id);
    nv::flash::Data data = 0;
    if (nv::flash::Flash::get_data(key, data) != nv::flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }

    auto& response             = *std::bit_cast<NsmTFFGetSocCalibCoefficientRes*>(&ntx.data[0]);
    response.coefficient_value = data;
    ntx.data_size              = sizeof(NsmTFFGetSocCalibCoefficientRes);
    return Ccode::Success;
}

Ccode handle_get_power_smooth_raw_readback(uint8_t readback_id, NsmPktResp& ntx)
{
    using nv::mctp::PwrSmoothRawReadbackId;

    uint16_t raw = 0;
    switch (static_cast<PwrSmoothRawReadbackId>(readback_id)) {
        case PwrSmoothRawReadbackId::SocAdcRaw:
            raw = nv::soc_pwr_smoothing::PowerSmoothing::get_last_adc_raw();
            break;
        case PwrSmoothRawReadbackId::EdppDacRaw:
            raw = nv::soc_pwr_smoothing::PowerSmoothing::get_last_edpp_dac_raw();
            break;
        case PwrSmoothRawReadbackId::IsinkDacRaw:
            raw = nv::soc_pwr_smoothing::PowerSmoothing::get_last_isink_dac_raw();
            break;
        default: return Ccode::ErrorInvalidData;
    }

    auto& response    = *std::bit_cast<NsmTFFGetPowerSmoothRawReadbackRes*>(&ntx.data[0]);
    response.raw_code = raw;
    response.resvd    = 0;
    ntx.data_size     = sizeof(NsmTFFGetPowerSmoothRawReadbackRes);
    return Ccode::Success;
}

}  // namespace nv::mctp::nsm_pwr_smoothing_handlers
