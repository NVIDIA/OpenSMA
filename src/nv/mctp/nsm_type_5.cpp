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

#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>

#include "nv/bootloader.h"
#include "nv/debugtoken/debugtoken.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/nsm.h"
#include "nv/nv.h"

namespace nv::mctp {

namespace nsm_type5 {

constexpr uint32_t valueFatalFaultEIMCUException    = (1u << FatalFaultEIMCUException);
constexpr uint32_t valueFatalFaultEIWatchdogTimeout = (1u << FatalFaultEIWatchdogTimeout);

const nv::logger::EventStructItem& getActivateNsmLoggerEvent(uint32_t bitmap)
{
    if (bitmap == valueFatalFaultEIMCUException) {
        return nv::logger::Event::T5ActivateMcuException;
    }
    else if (bitmap == valueFatalFaultEIWatchdogTimeout) {
        return nv::logger::Event::T5ActivateWatchdogTimeout;
    }
    // it should not happen, but return a valid log anyway
    return nv::logger::Event::NsmLogMessages;
}

void on_nsm_t5_fatal_fault_ei(uint8_t bitmap)
{
    auto logEvent = getActivateNsmLoggerEvent(static_cast<uint32_t>(bitmap));
    // coverity[cert_int31_c_violation] it will be always less than 0xFF
    nv::logger::info_wait(logEvent, {static_cast<uint8_t>(FatalFaultEI), bitmap});

    // if Fatal Fault EI MCUException is active
    if (bitmap == valueFatalFaultEIMCUException) {
        sys::common::ErrorInject::trigger_MemManageFault();
    }
    else if (bitmap == valueFatalFaultEIWatchdogTimeout) {
        sys::common::ErrorInject::trigger_WatchdogReset();
    }
    // else run for other erros
}

/**
 *
 * @param [in] fault_bitmap      - the bitmap from request message
 * @param [in/out] cur_fault_bitmap - the current Fatal fault bitmap
 * @return
 */
Ccode handleFatalFaultErrorInjectionPayload(uint32_t fault_bitmap, uint32_t& cur_fault_bitmap)
{
    const auto bitset_counter = std::popcount(fault_bitmap);

    if (bitset_counter > 0) {
        // request bitmap should have only one bit set
        if (bitset_counter > 1) {
            return Ccode::ErrorInvalidData;
        }
        // the bit set should be in FatalFaultEIMask
        if ((fault_bitmap & FatalFaultEIMask) == 0) {
            return Ccode::ErrorInvalidData;
        }
    }
    // else fault_bitmap is zero, then it is to clear cur_fault_bitmap

    // set the bitmap from request to the current fault bitmap
    cur_fault_bitmap = fault_bitmap;
    return Ccode::Success;
}

}  // namespace nsm_type5

bool Nsm::process_device_configuration(const Packet& rx, Packet& tx)
{
    using cmd = nv::mctp::NsmDevCfgCmdCode;
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    ntx.nv_msg_type = nrx.nv_msg_type;
    ntx.set_dev_cfg_code(nrx.get_dev_cfg_code());

    switch (nrx.get_dev_cfg_code()) {
        case cmd::SetErrorInjectionMode: on_dev_cfg_set_errorInjectionMode(rx, tx); break;
        case cmd::GetErrorInjectionMode: on_dev_cfg_get_errorInjectionMode(rx, tx); break;
        case cmd::GetSupportedErrorInjectionTypes:
            on_dev_cfg_get_supportedErrorInjectionTypes(rx, tx);
            break;
        case cmd::SetCurrentErrorInjectionTypes:
            on_dev_cfg_set_currentErrorInjectionTypes(rx, tx);
            break;
        case cmd::GetCurrentErrorInjectionTypes:
            on_dev_cfg_get_currentErrorInjectionTypes(rx, tx);
            break;
        case cmd::GetErrorInjectionPayload: on_dev_cfg_get_ErrorInjectionPayload(rx, tx); break;
        case cmd::SetErrorInjectionPayload:
            on_dev_cfg_submit_ErrorInjectionPayload(rx, tx);
            break;
        case cmd::ActivateErrorInjection:
            on_dev_cfg_activate_ErrorInjectionPayload(rx, tx);
            break;
        default: fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx); return false;
    }
    return true;
}

/**
 * @brief Sets new Error Injection Model according to the parameter in @rx
 *        This value is stored in type5_data.errorInjectionModeResponse
 * @param rx - Input message
 * @param tx - Output message
 */
void Nsm::on_dev_cfg_set_errorInjectionMode(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx             = NsmPktReq::from(rx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;

    ntx.completion_code = Ccode::Success;
    ntx.data_size       = 0;

    // when it is disabled also clear the current error types
    if (nrx.data[0] == NsmDevCfgEnablingMode::Disable) {
        type5_data.clearErrorInjection();
    }
    type5_data.errorInjectionModeResponse.mode = nrx.data[0];
}

/**
 * @brief Copies the current Error Injection Mode as reponse
 *        The response is stored in type5_data.errorInjectionModeResponse
 * @param rx - Input message
 * @param tx - Output message
 */
void Nsm::on_dev_cfg_get_errorInjectionMode(const Packet& rx, Packet& tx)
{
    auto responseSize = sizeof(type5_data.errorInjectionModeResponse);
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + responseSize;

    ntx.completion_code = Ccode::Success;
    ntx.data_size       = responseSize;
    std::memcpy(&ntx.data, &type5_data.errorInjectionModeResponse, responseSize);
}

/**
 * @brief Gets the supported Error Injection Types
 * @param rx - Input message
 * @param tx - Output message
 */
void Nsm::on_dev_cfg_get_supportedErrorInjectionTypes(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize
                          + NsmDevCfgSupportedErrorInjectionTypesNum;
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = NsmDevCfgSupportedErrorInjectionTypesNum;

    std::copy(type5_data.suppErrorTypes.begin(),
              type5_data.suppErrorTypes.end(),
              std::begin(ntx.data));
}

/**
 * @brief Sets the supported Error Injection Types
 * @param rx - Input message
 * @param tx - Output message
 */
void Nsm::on_dev_cfg_set_currentErrorInjectionTypes(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 8;
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);
    // check RX parameter size
    if (!is_input_length_valid(rx, RequestSize)
        || nrx.data_size != NsmDevCfgSupportedErrorInjectionTypesNum) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = 0;

    // check if Error Injection Mode is set
    // @sa on_dev_cfg_get_errorInjectionMode()
    if (false == type5_data.isErrorInjectionModeEnabled()) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // set internal bitmask buffer
    std::array<uint8_t, NsmDevCfgSupportedErrorInjectionTypesNum> msg_with_bitmask{0};
    const size_t rx_param_size = NsmDevCfgSupportedErrorInjectionTypesNum;
    std::memcpy(&msg_with_bitmask, &nrx.data, rx_param_size);

    // first loop just checks invalid data, does not set current ErrorInjection Types
    for (size_t index = 0; index < rx_param_size; ++index) {
        const uint8_t error_bitmask = msg_with_bitmask.at(index);
        const uint8_t supp_error    = type5_data.suppErrorTypes.at(index);
        // bit a bit check if an intended error type is supported
        uint8_t mask = 1;
        for (uint8_t bit = 0; bit < 8; ++bit, mask <<= 1) {
            if ((error_bitmask & mask) && !(supp_error & mask)) {
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return;
            }
        }
    }
    // second time just sets current ErrorInjection Types
    for (size_t index = 0; index < rx_param_size; ++index) {
        const uint8_t error_bitmask = msg_with_bitmask.at(index);
        const uint8_t supp_error    = type5_data.suppErrorTypes.at(index);
        type5_data.current_errors_injection_bitmask.at(index) = error_bitmask & supp_error;
    }
}

/**
 * @brief Get Current Error Injection Types
 *        Just copies the value of
 *        type5_data.current_errors_injection_bitmask as response
 * @param rx - Input message
 * @param tx - Output message
 */
void Nsm::on_dev_cfg_get_currentErrorInjectionTypes(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize
                          + NsmDevCfgSupportedErrorInjectionTypesNum;
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = NsmDevCfgSupportedErrorInjectionTypesNum;
    std::memcpy(&ntx.data, type5_data.current_errors_injection_bitmask.data(), ntx.data_size);
}

void Nsm::on_dev_cfg_get_ErrorInjectionPayload(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    if (nrx.data_size != sizeof(uint32_t)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    auto&    ntx        = NsmPktResp::from(tx);
    uint32_t error_code = 0;
    std::memcpy(&error_code, &nrx.data[0], sizeof(uint32_t));
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    if (error_code == FatalFaultEI) {
        ntx.data_size = sizeof(FatalFaultPayloadResponse);
        std::memcpy(
            &ntx.data[0], &type5_data.fatalFaultResp, sizeof(FatalFaultPayloadResponse));
        tx.priv.packet_length += sizeof(FatalFaultPayloadResponse);
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
}

void Nsm::on_dev_cfg_submit_ErrorInjectionPayload(const Packet& rx, Packet& tx)
{
    /* For T5 Set/Submit EI Payload the Request data is:

         Offset     | Error Injectio ID | Fault Bit Map
         ---------- |-------------------|--------------
            NvU32   |    NvU32          |  NvU32
     */
    using FatalFaultPayloadRequest = FatalFaultPayloadResponse;

    constexpr uint8_t RequestSize = sizeof(FatalFaultPayloadRequest);
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // check if error injection is enabled
    if (false == type5_data.isErrorInjectionModeEnabled()) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    FatalFaultPayloadRequest request{};

    std::memcpy(&request.offset, &nrx.data[0], sizeof(FatalFaultPayloadRequest));

    if (request.error_code == FatalFaultEI) {
        // check if current error code is enabled
        if (false == type5_data.isErrorTypeEnabled(FatalFaultEI)) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
        uint32_t cur_fatal_bitmap = type5_data.fatalFaultResp.bitmap;
        auto     rcCode = nsm_type5::handleFatalFaultErrorInjectionPayload(request.bitmap,
                                                                       cur_fatal_bitmap);
        if (rcCode != Ccode::Success) {
            fill_error_packet(rcCode, rx, tx);
            return;
        }
        type5_data.fatalFaultResp.bitmap = cur_fatal_bitmap;
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
}

void Nsm::on_dev_cfg_activate_ErrorInjectionPayload(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;

    auto all_bits = type5_data.fatalFaultResp.bitmap & FatalFaultEIMask;

    // check if error injection is enabled and if a fault is set
    if (false == type5_data.isErrorInjectionModeEnabled() || all_bits == 0) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // only one bit must be set
    if (all_bits == FatalFaultEIMask) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Debug-Token only for Release build, on dev build there is no Debug-Token
    uint8_t build_type = 0;
    auto    build_ok   = fill_build_type(build_type);
    if (true == build_ok && build_type == NsmBuildTypeRel
        && false == debugtoken::is_dbg_token_in_flash()) {
        fill_error_packet(Ccode::ErrorDebugTokenInstalled, rx, tx);
        return;
    }

    // coverity[cert_int31_c_violation] it will be always less than 0xFF
    Driver::mctp_send_cmd(Driver::CmdCode::NsmT5FatalFaultEI,
                          static_cast<uint8_t>(type5_data.fatalFaultResp.bitmap));
}

}  // namespace nv::mctp
