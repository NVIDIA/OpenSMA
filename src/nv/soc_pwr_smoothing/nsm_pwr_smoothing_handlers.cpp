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

#include <cstring>
#include "nv/logger/log.h"
#include "nv/mctp/nsm_pwr_smoothing_handlers.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/nsm_type_ff.h"
#include "nv/soc_pwr_smoothing/soc_pwr_smoothing.h"
#include "nv/soc_pwr_smoothing/presets.h"
#include "nv/nv.h"

namespace nv::mctp::nsm_pwr_smoothing_handlers {

// ============================================================================
// Strong Type-5 Get handlers - actual implementations
// ============================================================================

// Response format per spec:
//   NvU16: Current Device Mode length in Bytes
//   NvU16: Pending Device Mode length in Bytes (0 if not available)
//   Variable: Current Device Mode
//   Variable: Pending Device Mode (omitted if pending_length = 0)
// Since we apply changes immediately and persist them, pending_length is always 0.

Ccode handle_get_max_ac_ramp_rate(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    const float    current_value  = PowerSmoothing::GetMaxACRampRate();
    const uint16_t current_length = sizeof(float);
    const uint16_t pending_length = 0;  // No pending changes different from current

    // Write response: [current_length, pending_length, current_value]
    memcpy(&ntx.data[0], &current_length, sizeof(uint16_t));
    memcpy(&ntx.data[2], &pending_length, sizeof(uint16_t));
    memcpy(&ntx.data[4], &current_value, sizeof(float));

    ntx.data_size = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(float);  // 8 bytes
    return Ccode::Success;
}

Ccode handle_get_soc_power_smooth_enabled(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    const auto     current_value = static_cast<uint8_t>(PowerSmoothing::GetOffsetPolicyState());
    const uint16_t current_length = sizeof(uint8_t);
    const uint16_t pending_length = 0;  // No pending changes different from current

    // Write response: [current_length, pending_length, current_value]
    memcpy(&ntx.data[0], &current_length, sizeof(uint16_t));
    memcpy(&ntx.data[2], &pending_length, sizeof(uint16_t));
    ntx.data[4] = current_value;

    ntx.data_size = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint8_t);  // 5 bytes
    return Ccode::Success;
}

Ccode handle_get_soc_power_smooth_current_preset(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    const uint8_t  current_preset_index  = PowerSmoothing::GetActivePresetId();
    const uint8_t  supported_preset_mask = PowerSmoothing::GetSupportedPresetBitmask();
    const uint16_t current_length        = 2 * sizeof(uint8_t);  // preset index + bitmask
    const uint16_t pending_length        = 0;  // No pending changes different from current

    // Write response: [current_length, pending_length, current_preset_index,
    // supported_preset_mask]
    memcpy(&ntx.data[0], &current_length, sizeof(uint16_t));
    memcpy(&ntx.data[2], &pending_length, sizeof(uint16_t));
    ntx.data[4] = current_preset_index;
    ntx.data[5] = supported_preset_mask;

    ntx.data_size = sizeof(uint16_t) + sizeof(uint16_t) + 2 * sizeof(uint8_t);  // 6 bytes
    return Ccode::Success;
}

Ccode handle_get_soc_power_brake_enabled(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    const auto     brake_state    = PowerSmoothing::GetPowerBrakePolicyState();
    const uint8_t  current_value  = (brake_state == PowerBrakeState::PowerBrakeEnabled) ? 1 : 0;
    const uint16_t current_length = sizeof(uint8_t);
    const uint16_t pending_length = 0;  // No pending changes different from current

    // Write response: [current_length, pending_length, current_value]
    memcpy(&ntx.data[0], &current_length, sizeof(uint16_t));
    memcpy(&ntx.data[2], &pending_length, sizeof(uint16_t));
    ntx.data[4] = current_value;

    ntx.data_size = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint8_t);  // 5 bytes
    return Ccode::Success;
}

Ccode handle_get_soc_therm_brake_enabled(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    const auto     brake_state    = PowerSmoothing::GetThermBrakePolicyState();
    const uint8_t  current_value  = (brake_state == ThermBrakeState::ThermBrakeEnabled) ? 1 : 0;
    const uint16_t current_length = sizeof(uint8_t);
    const uint16_t pending_length = 0;  // No pending changes different from current

    // Write response: [current_length, pending_length, current_value]
    memcpy(&ntx.data[0], &current_length, sizeof(uint16_t));
    memcpy(&ntx.data[2], &pending_length, sizeof(uint16_t));
    ntx.data[4] = current_value;

    ntx.data_size = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint8_t);  // 5 bytes
    return Ccode::Success;
}

Ccode handle_get_supported_device_modes(NsmPktResp& ntx)
{
    // Response structure for GetSupportedDeviceModes (0x84)
    struct [[gnu::packed]] Response
    {
        uint32_t                handle;
        uint32_t                mode_count;
        std::array<uint32_t, 5> modes;
    };

    auto& response = *std::bit_cast<Response*>(&ntx.data[0]);

    response.handle     = 0;  // All modes fit in one response
    response.mode_count = 5;
    response.modes[0]   = static_cast<uint32_t>(DeviceModeIndex::MaxACPowerRampRate);
    response.modes[1]   = static_cast<uint32_t>(DeviceModeIndex::SoCPowerSmoothEnabled);
    response.modes[2]   = static_cast<uint32_t>(
        DeviceModeIndex::SoCPowerSmoothCurrentPresetIndex);
    response.modes[3] = static_cast<uint32_t>(DeviceModeIndex::SoCPowerBrakeEnabled);
    response.modes[4] = static_cast<uint32_t>(DeviceModeIndex::SoCThermBrakeEnabled);

    ntx.data_size = sizeof(Response);
    return Ccode::Success;
}

// ============================================================================
// Type-5: Set Device Mode Settings handlers (strong implementations)
// ============================================================================

Ccode handle_set_max_ac_ramp_rate(float ramp_rate)
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
    using namespace nv::soc_pwr_smoothing;

    auto& response = *std::bit_cast<NsmTFFGetRackPwrSmoothParamRes*>(&ntx.data[0]);

    // Get the currently active preset ID
    const uint8_t active_preset = PowerSmoothing::GetActivePresetId();

    // Get all parameters for the active preset
    std::array<uint32_t, PARAM_COUNT> params{};
    PowerSmoothing::GetPresetParameters(active_preset, params);

    // Only return non-TestHook tuning parameters (0-19)
    // TestHook parameters (40-42) require TestHook command with debug token
    const auto max_tuning = static_cast<uint8_t>(RackPwrSmoothParams::MaxTuningParams);

    response.numParams       = max_tuning;  // Return 20 params (0-19)
    response.currentPresetId = active_preset;
    response.resvd           = 0;

    for (uint8_t i = 0; i < max_tuning; i++) {
        // Array indices 0-19 map directly to param IDs 0-19
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        response.paramArray[i] = params[i];
    }

    ntx.data_size = sizeof(response) + (sizeof(uint32_t) * response.numParams);
    return Ccode::Success;
}

Ccode handle_set_rack_power_smoothing_param(uint8_t  preset_id,
                                            uint8_t  param_id,
                                            uint32_t param_value)
{
    using namespace nv::soc_pwr_smoothing;

    // Reject TestHook parameters (40-42) - they require TestHook command with debug token
    // Also rejects reserved gap (20-39)
    if (param_id >= static_cast<uint8_t>(RackPwrSmoothParams::MaxTuningParams)) {
        return Ccode::ErrorInvalidData;
    }

    // Call the SoC power smoothing API to update the parameter
    return PowerSmoothing::SetParameter(preset_id, param_id, param_value) ? Ccode::Success
                                                                          : Ccode::ErrorGeneral;
}

Ccode handle_get_rack_power_smoothing_testhook(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto& response = *std::bit_cast<NsmTFFGetRackPwrSmoothParamRes*>(&ntx.data[0]);

    // Get the currently active preset ID
    const uint8_t active_preset = PowerSmoothing::GetActivePresetId();

    // Get all parameters for the active preset
    std::array<uint32_t, PARAM_COUNT> params{};
    PowerSmoothing::GetPresetParameters(active_preset, params);

    // Only return TestHook parameters (40-42)
    const auto testhook_start = static_cast<uint8_t>(RackPwrSmoothParams::TestHookParamsStart);
    const auto testhook_end   = static_cast<uint8_t>(RackPwrSmoothParams::MaxParamCount);
    const auto testhook_count = testhook_end - testhook_start;

    response.numParams       = testhook_count;  // 3 params (40-42)
    response.currentPresetId = active_preset;
    response.resvd           = 0;

    for (uint8_t i = 0; i < testhook_count; i++) {
        // Array indices 40-42 map directly to param IDs 40-42
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        response.paramArray[i] = params[testhook_start + i];
    }

    ntx.data_size = sizeof(response) + (sizeof(uint32_t) * response.numParams);
    return Ccode::Success;
}

Ccode handle_set_rack_power_smoothing_testhook(uint8_t  preset_id,
                                               uint8_t  param_id,
                                               uint32_t param_value)
{
    using namespace nv::soc_pwr_smoothing;

    // Validate parameter ID is in TestHook range (40-42)
    if (param_id < static_cast<uint8_t>(RackPwrSmoothParams::TestHookParamsStart)
        || param_id >= static_cast<uint8_t>(RackPwrSmoothParams::MaxParamCount)) {
        return Ccode::ErrorInvalidData;
    }

    // Call the SoC power smoothing API to update the parameter
    return PowerSmoothing::SetParameter(preset_id, param_id, param_value) ? Ccode::Success
                                                                          : Ccode::ErrorGeneral;
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
// Type-3 Debug Telemetry handlers - strong implementations
// ============================================================================

namespace {

// Helper function to populate response header
void populate_response_header(NsmPktResp& ntx, uint16_t data_size)
{
    auto& response         = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    response.resvd         = 0;
    response.dataSizeBytes = data_size;
    ntx.data_size          = sizeof(response) + data_size;
}

// Telemetry data size for history responses (timestamp + count + samples)
constexpr uint16_t TelemetryHistoryDataSize = 112;  // 8 + 4 + 100

}  // anonymous namespace

Ccode handle_get_soc_telemetry_running_sum(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto     snapshot = PowerSmoothing::get_telemetry_snapshot();
    auto&    response = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    uint8_t* data_ptr = &response.telemetryData[0];

    // Format: timestamp (8 bytes) + sum (8 bytes)
    std::memcpy(data_ptr, &snapshot.timestamp_ms, sizeof(uint64_t));
    std::memcpy(
        data_ptr + sizeof(uint64_t), &snapshot.soc_percent_filtered_sum, sizeof(uint64_t));

    populate_response_header(ntx, 16);
    return Ccode::Success;
}

Ccode handle_get_power_brake_running_sum(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto     snapshot = PowerSmoothing::get_telemetry_snapshot();
    auto&    response = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    uint8_t* data_ptr = &response.telemetryData[0];

    // Format: timestamp (8 bytes) + sum (8 bytes)
    std::memcpy(data_ptr, &snapshot.timestamp_ms, sizeof(uint64_t));
    std::memcpy(data_ptr + sizeof(uint64_t), &snapshot.pwr_brake_time_ms, sizeof(uint64_t));

    populate_response_header(ntx, 16);
    return Ccode::Success;
}

Ccode handle_get_edpp_offset_running_sum(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto     snapshot = PowerSmoothing::get_telemetry_snapshot();
    auto&    response = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    uint8_t* data_ptr = &response.telemetryData[0];

    // Format: timestamp (8 bytes) + sum (8 bytes)
    std::memcpy(data_ptr, &snapshot.timestamp_ms, sizeof(uint64_t));
    std::memcpy(data_ptr + sizeof(uint64_t), &snapshot.edpp_offset_sum, sizeof(uint64_t));

    populate_response_header(ntx, 16);
    return Ccode::Success;
}

Ccode handle_get_isink_offset_running_sum(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto     snapshot = PowerSmoothing::get_telemetry_snapshot();
    auto&    response = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    uint8_t* data_ptr = &response.telemetryData[0];

    // Format: timestamp (8 bytes) + sum (8 bytes)
    std::memcpy(data_ptr, &snapshot.timestamp_ms, sizeof(uint64_t));
    std::memcpy(data_ptr + sizeof(uint64_t), &snapshot.isink_offset_sum, sizeof(uint64_t));

    populate_response_header(ntx, 16);
    return Ccode::Success;
}

Ccode handle_get_soc_recent_history(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto     snapshot = PowerSmoothing::get_telemetry_history_snapshot();
    auto&    response = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    uint8_t* data_ptr = &response.telemetryData[0];

    // Format: timestamp (8 bytes) + sample_count (4 bytes) + samples (100 bytes)
    std::memcpy(data_ptr, &snapshot.timestamp_ms, sizeof(uint64_t));
    std::memcpy(data_ptr + sizeof(uint64_t), &snapshot.sample_count, sizeof(uint32_t));
    std::memcpy(data_ptr + sizeof(uint64_t) + sizeof(uint32_t),
                snapshot.soc_percent_filtered.data(),
                snapshot.soc_percent_filtered.size());

    populate_response_header(ntx, TelemetryHistoryDataSize);
    return Ccode::Success;
}

Ccode handle_get_edpp_recent_history(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto     snapshot = PowerSmoothing::get_telemetry_history_snapshot();
    auto&    response = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    uint8_t* data_ptr = &response.telemetryData[0];

    // Format: timestamp (8 bytes) + sample_count (4 bytes) + samples (100 bytes)
    std::memcpy(data_ptr, &snapshot.timestamp_ms, sizeof(uint64_t));
    std::memcpy(data_ptr + sizeof(uint64_t), &snapshot.sample_count, sizeof(uint32_t));
    std::memcpy(data_ptr + sizeof(uint64_t) + sizeof(uint32_t),
                snapshot.edpp_offset.data(),
                snapshot.edpp_offset.size());

    populate_response_header(ntx, TelemetryHistoryDataSize);
    return Ccode::Success;
}

Ccode handle_get_isink_recent_history(NsmPktResp& ntx)
{
    using namespace nv::soc_pwr_smoothing;

    auto     snapshot = PowerSmoothing::get_telemetry_history_snapshot();
    auto&    response = *std::bit_cast<NsmTFFGetRackPwrSmoothTelemetryRes*>(&ntx.data[0]);
    uint8_t* data_ptr = &response.telemetryData[0];

    // Format: timestamp (8 bytes) + sample_count (4 bytes) + samples (100 bytes)
    std::memcpy(data_ptr, &snapshot.timestamp_ms, sizeof(uint64_t));
    std::memcpy(data_ptr + sizeof(uint64_t), &snapshot.sample_count, sizeof(uint32_t));
    std::memcpy(data_ptr + sizeof(uint64_t) + sizeof(uint32_t),
                snapshot.isink_offset.data(),
                snapshot.isink_offset.size());

    populate_response_header(ntx, TelemetryHistoryDataSize);
    return Ccode::Success;
}

// ============================================================================
// Strong Type-FF ADC Calibration handlers - actual implementations
// ============================================================================

Ccode handle_trigger_adc_calibration()
{
    // Run full 26-step calibration blocking (~130ms) in MCTP task context.
    nv::soc_pwr_smoothing::execute_adc_calibration();
    return Ccode::Success;
}

Ccode handle_get_adc_calibration_results(NsmPktResp& ntx)
{
    auto& response = *std::bit_cast<NsmTFFGetAdcCalibResultsRes*>(&ntx.data[0]);

    // Populate the response from current state or flash
    nv::soc_pwr_smoothing::get_adc_calibration_results(response);

    ntx.data_size = sizeof(NsmTFFGetAdcCalibResultsRes);
    return Ccode::Success;
}

}  // namespace nv::mctp::nsm_pwr_smoothing_handlers
