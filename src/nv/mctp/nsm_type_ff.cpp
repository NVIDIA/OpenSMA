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
#include <utility>

#include "config.h"

#include "nv/logger/log.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/nsm_pwr_smoothing_handlers.h"

namespace nv::mctp {

using cmd = NsmTypeFFCmdCode;

template<typename Req>
bool Nsm::load_nv_internal_request(const Packet&      rx,
                                   Packet&            tx,
                                   Req&               request,
                                   NvInternalReqCheck check)
{
    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return false;
    }

    static_assert(sizeof(Req) <= UINT8_MAX,
                  "Request size exceeds uint8_t range for is_input_length_valid");

    switch (check) {
        case NvInternalReqCheck::LengthOnly:
            if (!is_input_length_valid(rx, sizeof(Req))) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return false;
            }
            break;
        case NvInternalReqCheck::ExactSize:
            if (!is_input_length_valid(rx, sizeof(Req))) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return false;
            }
            if (nrx.data_size != sizeof(Req)) {
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return false;
            }
            break;
        case NvInternalReqCheck::ExactSizeLengthOnMismatch:
            if (!is_input_length_valid(rx, sizeof(Req)) || nrx.data_size != sizeof(Req)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return false;
            }
            break;
    }

    std::memcpy(&request, &nrx.data[0], sizeof(Req));
    return true;
}

template<typename Handler>
void Nsm::dispatch_nv_internal_get(const Packet& rx, Packet& tx, Handler&& handler)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto&      ntx    = NsmPktResp::from(tx);
    const auto result = std::forward<Handler>(handler)(ntx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

template<typename Req, typename Handler>
void Nsm::dispatch_nv_internal_get_req(const Packet&      rx,
                                       Packet&            tx,
                                       NvInternalReqCheck check,
                                       Handler&&          handler)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    Req request{};
    if (!load_nv_internal_request(rx, tx, request, check)) {
        return;
    }

    auto&      ntx    = NsmPktResp::from(tx);
    const auto result = std::forward<Handler>(handler)(request, ntx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

template<typename Req, typename Handler>
void Nsm::dispatch_nv_internal_set_req(const Packet&      rx,
                                       Packet&            tx,
                                       NvInternalReqCheck check,
                                       Handler&&          handler)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    Req request{};
    if (!load_nv_internal_request(rx, tx, request, check)) {
        return;
    }

    const auto result = std::forward<Handler>(handler)(request);
    auto&      ntx    = NsmPktResp::from(tx);
    if (result == Ccode::Success) {
        ntx.data_size       = 0;
        ntx.completion_code = Ccode::Success;
    }
    else {
        ntx.completion_code = result;
    }
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

template<typename Handler>
void Nsm::dispatch_nv_internal_action(const Packet& rx, Packet& tx, Handler&& handler)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    const auto result = std::forward<Handler>(handler)();
    auto&      ntx    = NsmPktResp::from(tx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

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

        case cmd::SetSocCalibCoefficient:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::SetSocCalibCoefficient)) {
                on_nv_internal_setSocCalibCoefficient(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetSocCalibCoefficient:
            if constexpr (is_cmd_set(NsmMsgType::NvInternal, cmd::GetSocCalibCoefficient)) {
                on_nv_internal_getSocCalibCoefficient(rx, tx);
                break;
            }
            return unsupported_command();

        default: return unsupported_command();
    }
}

void Nsm::on_nv_internal_getRackPowerSmoothingParam(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_get(rx, tx, [](NsmPktResp& ntx) {
        return nsm_pwr_smoothing_handlers::handle_get_rack_power_smoothing_param(ntx);
    });
}

void Nsm::on_nv_internal_setRackPowerSmoothingParam(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_set_req<NsmTFFSetRackPwrSmoothParamReq>(
        rx,
        tx,
        NvInternalReqCheck::ExactSize,
        [](const NsmTFFSetRackPwrSmoothParamReq& request) {
            return nsm_pwr_smoothing_handlers::handle_set_rack_power_smoothing_param(
                request.presetId, request.paramId, request.paramValue);
        });
}

void Nsm::on_nv_internal_getRackPowerSmoothingTestHook(const Packet& rx, Packet& tx)
{
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

    dispatch_nv_internal_get(rx, tx, [](NsmPktResp& ntx) {
        return nsm_pwr_smoothing_handlers::handle_get_rack_power_smoothing_testhook(ntx);
    });
}

void Nsm::on_nv_internal_setRackPowerSmoothingTestHook(const Packet& rx, Packet& tx)
{
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

    dispatch_nv_internal_set_req<NsmTFFSetRackPwrSmoothParamReq>(
        rx,
        tx,
        NvInternalReqCheck::ExactSize,
        [](const NsmTFFSetRackPwrSmoothParamReq& request) {
            return nsm_pwr_smoothing_handlers::handle_set_rack_power_smoothing_testhook(
                request.presetId, request.paramId, request.paramValue);
        });
}

void Nsm::on_nv_internal_getDebugTelemetry(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_get_req<NsmTFFGetRackPwrSmoothTelemetryReq>(
        rx,
        tx,
        NvInternalReqCheck::LengthOnly,
        [](const NsmTFFGetRackPwrSmoothTelemetryReq& request, NsmPktResp& ntx) {
            return nsm_pwr_smoothing_handlers::handle_get_debug_telemetry(request.telemetryType,
                                                                          ntx);
        });
}

void Nsm::on_nv_internal_triggerAdcCalibration(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_action(
        rx, tx, []() { return nsm_pwr_smoothing_handlers::handle_trigger_adc_calibration(); });
}

void Nsm::on_nv_internal_getAdcCalibrationResults(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_get(rx, tx, [](NsmPktResp& ntx) {
        return nsm_pwr_smoothing_handlers::handle_get_adc_calibration_results(ntx);
    });
}

void Nsm::on_nv_internal_adcCalibSetLoopbackDacCode(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_set_req<NsmTFFAdcCalibSetLoopbackDacCodeReq>(
        rx,
        tx,
        NvInternalReqCheck::ExactSize,
        [](const NsmTFFAdcCalibSetLoopbackDacCodeReq& request) {
            return nsm_pwr_smoothing_handlers::handle_adc_calib_set_loopback_dac_code(
                request.dac_code);
        });
}

void Nsm::on_nv_internal_getPowerSmoothRawReadback(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_get_req<NsmTFFGetPowerSmoothRawReadbackReq>(
        rx,
        tx,
        NvInternalReqCheck::ExactSizeLengthOnMismatch,
        [](const NsmTFFGetPowerSmoothRawReadbackReq& request, NsmPktResp& ntx) {
            return nsm_pwr_smoothing_handlers::handle_get_power_smooth_raw_readback(
                request.readback_id, ntx);
        });
}

void Nsm::on_nv_internal_setSocCalibCoefficient(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_set_req<NsmTFFSetSocCalibCoefficientReq>(
        rx,
        tx,
        NvInternalReqCheck::ExactSizeLengthOnMismatch,
        [](const NsmTFFSetSocCalibCoefficientReq& request) {
            return nsm_pwr_smoothing_handlers::handle_set_soc_calib_coefficient(
                request.coeff_id, request.coefficient_value);
        });
}

void Nsm::on_nv_internal_getSocCalibCoefficient(const Packet& rx, Packet& tx)
{
    dispatch_nv_internal_get_req<NsmTFFGetSocCalibCoefficientReq>(
        rx,
        tx,
        NvInternalReqCheck::ExactSizeLengthOnMismatch,
        [](const NsmTFFGetSocCalibCoefficientReq& request, NsmPktResp& ntx) {
            return nsm_pwr_smoothing_handlers::handle_get_soc_calib_coefficient(
                request.coeff_id, ntx);
        });
}

}  // namespace nv::mctp
