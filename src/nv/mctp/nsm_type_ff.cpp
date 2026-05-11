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

#include "nv/mctp/nsm_type_ff.h"

#include <cstdint>
#include <cstring>

#include "config.h"

#include "nv/logger/log.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/nsm_pwr_smoothing_handlers.h"

namespace nv::mctp {

using cmd = NsmTypeFFCmdCode;

void Nsm::process_nv_internal(const Packet& rx, Packet& tx)
{
    auto& nrx = NsmPktReq::from(rx);

    // Helper to handle unsupported commands
    [[maybe_unused]] auto unsupported_command = [&]() {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return;
    };

    switch (nrx.get_typeff_code()) {
        case cmd::GetRackPowerSmoothingParam:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::GetRackPowerSmoothingParam)) {
                on_nv_internal_getRackPowerSmoothingParam(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetRackPowerSmoothingParam:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::SetRackPowerSmoothingParam)) {
                on_nv_internal_setRackPowerSmoothingParam(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetRackPowerSmoothingTestHook:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal,
                                     cmd::GetRackPowerSmoothingTestHook)) {
                on_nv_internal_getRackPowerSmoothingTestHook(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetRackPowerSmoothingTestHook:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal,
                                     cmd::SetRackPowerSmoothingTestHook)) {
                on_nv_internal_setRackPowerSmoothingTestHook(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetDebugTelemetry:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::GetDebugTelemetry)) {
                on_nv_internal_getDebugTelemetry(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::TriggerAdcCalibration:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::TriggerAdcCalibration)) {
                on_nv_internal_triggerAdcCalibration(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetAdcCalibrationResults:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::GetAdcCalibrationResults)) {
                on_nv_internal_getAdcCalibrationResults(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::AdcCalibSetLoopbackDacCode:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::AdcCalibSetLoopbackDacCode)) {
                on_nv_internal_adcCalibSetLoopbackDacCode(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetPowerSmoothRawReadback:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::GetPowerSmoothRawReadback)) {
                on_nv_internal_getPowerSmoothRawReadback(rx, tx);
                break;
            }
            return unsupported_command();

        default: return unsupported_command();
    }
}

void Nsm::on_nv_internal_getRackPowerSmoothingParam(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx = NsmPktResp::from(tx);

    // Use handler - returns Ccode
    auto result = nsm_pwr_smoothing_handlers::handle_get_rack_power_smoothing_param(ntx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_nv_internal_setRackPowerSmoothingParam(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    if (!is_input_length_valid(rx, sizeof(NsmTFFSetRackPwrSmoothParamReq))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // Verify data size matches single-parameter request
    const auto expected_size = sizeof(NsmTFFSetRackPwrSmoothParamReq);
    if (nrx.data_size != expected_size) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& request = *std::bit_cast<NsmTFFSetRackPwrSmoothParamReq*>(&nrx.data[0]);

    // Copy values from packed struct to avoid reference binding issues
    const uint8_t  preset_id   = request.presetId;
    const uint8_t  param_id    = request.paramId;
    const uint32_t param_value = request.paramValue;

    // Use handler - returns Ccode
    auto result = nsm_pwr_smoothing_handlers::handle_set_rack_power_smoothing_param(
        preset_id, param_id, param_value);

    auto& ntx = NsmPktResp::from(tx);
    if (result == Ccode::Success) {
        ntx.data_size       = 0;
        ntx.completion_code = Ccode::Success;
    }
    else {
        ntx.completion_code = result;
    }
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

void Nsm::on_nv_internal_getRackPowerSmoothingTestHook(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // WAR: Temporary out-of-spec enablement for TS3/QS manufacturing flow.
    // Track revert to spec-gated Rel behavior: https://nvbugspro.nvidia.com/bug/5910647
    // Spec target after revert: Dev allowed without token; Rel gated by debug-token policy.
    // WAR active: debug-token gate is intentionally bypassed for all builds.
    // Keep original gate logic below for explicit revert context.
    /*
    // Debug token check required unless confirmed dev build
    uint8_t    build_type   = 0;
    const auto build_ok     = fill_build_type(get_active_slot(), build_type);
    const bool is_dev_build = (build_ok && build_type == NsmBuildTypeDev);
    if (!is_dev_build) {
        // Check debug token is installed and valid
        const auto token_check = nv::debugtoken::check_debug_token_type_enabled(
            nv::debugtoken::Type::FlashDebugFw);

        if (token_check != nv::debugtoken::TokenErrorCode::NoErrorCode) {
            fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
            return;
        }
    }
    */

    auto& ntx = NsmPktResp::from(tx);

    // Use handler - returns Ccode
    auto result = nsm_pwr_smoothing_handlers::handle_get_rack_power_smoothing_testhook(ntx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_nv_internal_setRackPowerSmoothingTestHook(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // WAR: Temporary out-of-spec enablement for TS3/QS manufacturing flow.
    // Track revert to spec-gated Rel behavior: https://nvbugspro.nvidia.com/bug/5910647
    // Spec target after revert: Dev allowed without token; Rel gated by debug-token policy.
    // WAR active: debug-token gate is intentionally bypassed for all builds.
    // Keep original gate logic below for explicit revert context.
    /*
    // Debug token check required unless confirmed dev build
    uint8_t    build_type   = 0;
    const auto build_ok     = fill_build_type(get_active_slot(), build_type);
    const bool is_dev_build = (build_ok && build_type == NsmBuildTypeDev);
    if (!is_dev_build) {
        // Check debug token is installed and valid
        const auto token_check = nv::debugtoken::check_debug_token_type_enabled(
            nv::debugtoken::Type::FlashDebugFw);

        if (token_check != nv::debugtoken::TokenErrorCode::NoErrorCode) {
            fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
            return;
        }
    }
    */

    if (!is_input_length_valid(rx, sizeof(NsmTFFSetRackPwrSmoothParamReq))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // Verify data size matches single-parameter request
    const auto expected_size = sizeof(NsmTFFSetRackPwrSmoothParamReq);
    if (nrx.data_size != expected_size) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& request = *std::bit_cast<NsmTFFSetRackPwrSmoothParamReq*>(&nrx.data[0]);

    // Copy values from packed struct to avoid reference binding issues
    const uint8_t  preset_id   = request.presetId;
    const uint8_t  param_id    = request.paramId;
    const uint32_t param_value = request.paramValue;

    // Use handler - returns Ccode
    auto result = nsm_pwr_smoothing_handlers::handle_set_rack_power_smoothing_testhook(
        preset_id, param_id, param_value);

    auto& ntx = NsmPktResp::from(tx);
    if (result == Ccode::Success) {
        ntx.data_size       = 0;
        ntx.completion_code = Ccode::Success;
    }
    else {
        ntx.completion_code = result;
    }
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

void Nsm::on_nv_internal_getDebugTelemetry(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Guard against silent narrowing of sizeof(...) to uint8_t
    static_assert(sizeof(NsmTFFGetRackPwrSmoothTelemetryReq) <= UINT8_MAX,
                  "Request size exceeds uint8_t range for is_input_length_valid");

    if (!is_input_length_valid(rx, sizeof(NsmTFFGetRackPwrSmoothTelemetryReq))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // Copy request bytes into local POD instead of reinterpreting the byte buffer
    NsmTFFGetRackPwrSmoothTelemetryReq request{};
    std::memcpy(&request, &nrx.data[0], sizeof(request));

    // Use handler - returns Ccode
    auto result = nsm_pwr_smoothing_handlers::handle_get_debug_telemetry(request.telemetryType,
                                                                         ntx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_nv_internal_triggerAdcCalibration(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx = NsmPktResp::from(tx);

    // Use handler - starts ADC calibration test
    auto result = nsm_pwr_smoothing_handlers::handle_trigger_adc_calibration();
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

void Nsm::on_nv_internal_getAdcCalibrationResults(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx = NsmPktResp::from(tx);

    // Use handler - gets calibration results
    auto result = nsm_pwr_smoothing_handlers::handle_get_adc_calibration_results(ntx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_nv_internal_adcCalibSetLoopbackDacCode(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    static_assert(sizeof(NsmTFFAdcCalibSetLoopbackDacCodeReq) <= UINT8_MAX,
                  "Request size exceeds uint8_t range for is_input_length_valid");

    if (!is_input_length_valid(rx, sizeof(NsmTFFAdcCalibSetLoopbackDacCodeReq))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    if (nrx.data_size != sizeof(NsmTFFAdcCalibSetLoopbackDacCodeReq)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    NsmTFFAdcCalibSetLoopbackDacCodeReq request{};
    std::memcpy(&request, &nrx.data[0], sizeof(request));

    auto result = nsm_pwr_smoothing_handlers::handle_adc_calib_set_loopback_dac_code(
        request.dac_code);

    auto& ntx = NsmPktResp::from(tx);
    if (result == Ccode::Success) {
        ntx.data_size       = 0;
        ntx.completion_code = Ccode::Success;
    }
    else {
        ntx.completion_code = result;
    }
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

void Nsm::on_nv_internal_getPowerSmoothRawReadback(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    static_assert(sizeof(NsmTFFGetPowerSmoothRawReadbackReq) <= UINT8_MAX,
                  "Request size exceeds uint8_t range for is_input_length_valid");

    if (!is_input_length_valid(rx, sizeof(NsmTFFGetPowerSmoothRawReadbackReq))
        || nrx.data_size != sizeof(NsmTFFGetPowerSmoothRawReadbackReq)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    NsmTFFGetPowerSmoothRawReadbackReq request{};
    std::memcpy(&request, &nrx.data[0], sizeof(request));
    const uint8_t readback_id = request.readback_id;

    auto& ntx = NsmPktResp::from(tx);

    auto result = nsm_pwr_smoothing_handlers::handle_get_power_smooth_raw_readback(readback_id,
                                                                                   ntx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

}  // namespace nv::mctp
