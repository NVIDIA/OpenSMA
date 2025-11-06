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
#include "nv/logger/common.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/nsm.h"
#include "nv/nv.h"

namespace nv::mctp {

namespace nsm_type5 {

constexpr uint32_t
                   valueFatalFaultEIMCUException    = (1u << static_cast<uint32_t>(
                                         FatalFaultEIPayloadValues::FatalFaultEIMCUException));
constexpr uint32_t valueFatalFaultEIWatchdogTimeout = (1u << static_cast<uint32_t>(
                                                           FatalFaultEIPayloadValues::
                                                               FatalFaultEIWatchdogTimeout));

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
    nv::logger::info_wait(logEvent,
                          {static_cast<uint8_t>(ErrorInjectionID::DeviceError), bitmap});

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
bool validateFatalErrorInjectionPayload(uint32_t fault_bitmap)
{
    const auto bitset_counter = __builtin_popcount(fault_bitmap);

    if (bitset_counter > 0) {
        // request bitmap should have only one bit set
        if (bitset_counter > 1) {
            return false;
        }
        // the bit set should be in FatalFaultEIMask
        if ((fault_bitmap & static_cast<uint32_t>(FatalFaultEIPayloadValues::FatalFaultEIMask))
            == 0) {
            return false;
        }
    }

    return true;
}

bool validatePortRecoveryErrorInjectionPayload(
    [[maybe_unused]] PortRecoveryPayload& portRecoveryPayload)
{
    return true;
}

bool validateUSBBridgeEmulationErrorInjectionPayload(
    [[maybe_unused]] USBBridgeEmulationPayload& usbBridgeEmulationPayload)
{
    return true;
}

bool validateGpioSpoofingErrorInjectionPayload(
    [[maybe_unused]] GPIOSpoofingPayload& gpioSpoofingPayload)
{
    return true;
}

}  // namespace nsm_type5

bool Nsm::process_device_configuration(const Packet& rx, Packet& tx)
{
    using cmd       = nv::mctp::NsmDevCfgCmdCode;
    const auto& nrx = NsmPktReq::from(rx);
    auto&       ntx = NsmPktResp::from(tx);

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
    const auto& nrx       = NsmPktReq::from(rx);
    auto&       ntx       = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;

    ntx.completion_code = Ccode::Success;
    ntx.data_size       = 0;

    // when it is disabled also clear the current error types
    if (nrx.data[0] == NsmDevCfgEnablingMode::Disable) {
        if (!type5_data.isCurrentErrorInjectionBitmaskCleared()) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
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
    memcpy(&ntx.data, &type5_data.errorInjectionModeResponse, responseSize);
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
                          + NsmMaxSupportedErrorTypeBitmapBytesNum;
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = NsmMaxSupportedErrorTypeBitmapBytesNum;

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
    const auto& nrx = NsmPktReq::from(rx);
    // check RX parameter size
    if (!is_input_length_valid(rx, RequestSize)
        || nrx.data_size != NsmMaxSupportedErrorTypeBitmapBytesNum) {
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
    std::array<uint8_t, NsmMaxSupportedErrorTypeBitmapBytesNum> msg_with_bitmask{0};
    memcpy(&msg_with_bitmask, &nrx.data, NsmMaxSupportedErrorTypeBitmapBytesNum);

    // Check if Einj Type/ID is supported
    if (!type5_data.isErrorInjectionIdBitmapSupported(msg_with_bitmask)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // To simplify the code, only loop through the supported error types
    for (size_t index = 0; index < NsmMaxSupportedErrorTypeBitmapBytesNum; ++index) {
        const uint8_t error_bitmap    = msg_with_bitmask.at(index);
        const uint8_t current_bitmask = type5_data.current_errors_injection_bitmask.at(index);
        const uint8_t cleared_bits    = current_bitmask & ~error_bitmap;

        // Check if DeviceError bit is being cleared in this byte
        const int32_t bit_offset = static_cast<int32_t>(
                                       static_cast<uint8_t>(ErrorInjectionID::DeviceError))
                                 - static_cast<int32_t>(index * CHAR_BIT);
        if (bit_offset >= 0 && bit_offset < CHAR_BIT && (cleared_bits & (1 << bit_offset))) {
            if (!type5_data.isDeviceErrorStatusBitmapCleared()) {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            type5_data.clearErrorInjectionPayload(
                static_cast<uint8_t>(ErrorInjectionID::DeviceError));
        }

        // Check if GpioSpoofing bit is being cleared in this byte
        const int32_t gpio_bit_offset = static_cast<int32_t>(static_cast<uint8_t>(
                                            ErrorInjectionID::GpioSpoofing))
                                      - static_cast<int32_t>(index * CHAR_BIT);
        if (gpio_bit_offset >= 0 && gpio_bit_offset < CHAR_BIT
            && (cleared_bits & (1 << gpio_bit_offset))) {
            if (!type5_data.isGPIOErrorStatusBitmapCleared()) {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            type5_data.clearErrorInjectionPayload(
                static_cast<uint8_t>(ErrorInjectionID::GpioSpoofing));
        }

        type5_data.current_errors_injection_bitmask.at(index) = error_bitmap;
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
                          + NsmMaxSupportedErrorTypeBitmapBytesNum;
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = NsmMaxSupportedErrorTypeBitmapBytesNum;
    memcpy(&ntx.data, type5_data.current_errors_injection_bitmask.data(), ntx.data_size);
}

void Nsm::on_dev_cfg_get_ErrorInjectionPayload(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    const auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Check if request has the required size
    if (!is_input_length_valid(rx, sizeof(ErrorInjectionId))
        || nrx.data_size < sizeof(ErrorInjectionId)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& ntx           = NsmPktResp::from(tx);
    ntx.data_size       = 0;
    ntx.completion_code = Ccode::Success;

    // Parse request data according to spec
    ErrorInjectionId request{};
    memcpy(&request, &nrx.data[0], sizeof(ErrorInjectionId));

    // Validate Error Injection ID
    if (request.error_injection_id != static_cast<uint16_t>(DeviceError)
        && request.error_injection_id != static_cast<uint16_t>(GpioSpoofing)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    if (request.error_injection_id == static_cast<uint16_t>(DeviceError)) {
        // Handle different error types
        if (request.error_type == static_cast<uint16_t>(FatalErrors)) {
            // Return Fatal Error Payload
            ntx.data_size = sizeof(FatalErrorPayload);
            memcpy(&ntx.data[0], &type5_data.fatalerrorResp, sizeof(FatalErrorPayload));
        }
        else if (request.error_type == static_cast<uint16_t>(PortRecoveryErrors)) {
            // Return Port Recovery Error Payload
            ntx.data_size = sizeof(PortRecoveryPayload);
            memcpy(&ntx.data[0], &type5_data.portRecoveryResp, sizeof(PortRecoveryPayload));
        }
        else if (request.error_type == static_cast<uint16_t>(USBBridgeEmulationErrors)) {
            // Return USB Bridge Emulation Error Payload
            ntx.data_size = sizeof(USBBridgeEmulationPayload);
            memcpy(&ntx.data[0],
                   &type5_data.usbBridgeEmulationResp,
                   sizeof(USBBridgeEmulationPayload));
        }
        else {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }
    else if (request.error_injection_id == static_cast<uint16_t>(GpioSpoofing)) {
        // Update GPIO Spoofing Response Payload from internal data
        const auto gpio_number = type5_data.gpioSpoofingResp.gpio_spoofing_header
                                     .ei_gpio_number;
        const auto response_size = sizeof(GpioSpoofingPayloadHeader)
                                 + gpio_number
                                       * sizeof(type5_data.gpioSpoofingResp.gpio_ei_entries[0]);

        // Check if response_size exceeds the actual struct size to prevent buffer overflow
        if (response_size > sizeof(GPIOSpoofingPayload)) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }

        // Return GPIO Spoofing Error Payload
        ntx.data_size = static_cast<uint16_t>(response_size);
        memcpy(&ntx.data[0], &type5_data.gpioSpoofingResp, response_size);
    }

    const auto total_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
    if (total_length > UINT16_MAX) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    tx.priv.packet_length = static_cast<uint16_t>(total_length);
}

void Nsm::on_dev_cfg_submit_ErrorInjectionPayload(const Packet& rx, Packet& tx)
{
    // check if error injection is enabled
    if (false == type5_data.isErrorInjectionModeEnabled()) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    const auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;

    ErrorInjectionPayloadHeader request{};

    memcpy(&request, &nrx.data[0], sizeof(ErrorInjectionPayloadHeader));

    // Validate request fields according to spec
    if (request.offset != 0) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    if (request.error_injection_id != static_cast<uint16_t>(DeviceError)
        && request.error_injection_id != static_cast<uint16_t>(GpioSpoofing)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Handle DeviceError (0x04)
    if (request.error_injection_id == static_cast<uint16_t>(DeviceError)) {
        // check if current error code is enabled
        if (false == type5_data.isErrorTypeEnabled(static_cast<uint8_t>(DeviceError))) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }

        // Handle different error types
        if (request.error_type == static_cast<uint16_t>(FatalErrors)) {
            // Handle Fatal Errors (Error Type 0x0)
            if (!is_input_length_valid(rx, sizeof(FatalErrorPayload))
                || nrx.data_size < sizeof(FatalErrorPayload)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }

            // Direct memcpy is now safe since FatalErrorPayload is trivial
            memcpy(&type5_data.fatalerrorResp, &nrx.data[0], sizeof(FatalErrorPayload));
            const auto status = nsm_type5::validateFatalErrorInjectionPayload(
                type5_data.fatalerrorResp.fault_payload_bitmap);
            if (status != true) {
                type5_data.fatalerrorResp = FatalErrorPayload{};
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return;
            }
            return;
        }
        else if (request.error_type == static_cast<uint16_t>(PortRecoveryErrors)) {
            if (!is_input_length_valid(rx, sizeof(PortRecoveryPayload))
                || nrx.data_size < sizeof(PortRecoveryPayload)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }

            // Direct memcpy is now safe since PortRecoveryPayload is trivial
            memcpy(&type5_data.portRecoveryResp, &nrx.data[0], sizeof(PortRecoveryPayload));

            // TODO: Fullfill the validatePortRecoveryErrorInjectionPayload
            const auto status = nsm_type5::validatePortRecoveryErrorInjectionPayload(
                type5_data.portRecoveryResp);
            if (status != true) {
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return;
            }

            return;
        }
        else if (request.error_type == static_cast<uint16_t>(USBBridgeEmulationErrors)) {
            // Handle USB Bridge Emulation Errors (Error Type 0x2) - not implemented yet
            if (!is_input_length_valid(rx, sizeof(USBBridgeEmulationPayload))
                || nrx.data_size < sizeof(USBBridgeEmulationPayload)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }

            // Direct memcpy is now safe since USBBridgeEmulationPayload is trivial
            memcpy(&type5_data.usbBridgeEmulationResp,
                   &nrx.data[0],
                   sizeof(USBBridgeEmulationPayload));
            const auto status = nsm_type5::validateUSBBridgeEmulationErrorInjectionPayload(
                type5_data.usbBridgeEmulationResp);
            if (status != true) {
                type5_data.usbBridgeEmulationResp = USBBridgeEmulationPayload{};
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return;
            }

            return;
        }
        else {
            // Reserved error types
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }
    // Handle GpioSpoofing (0x05)
    else if (request.error_injection_id == static_cast<uint16_t>(GpioSpoofing)) {
        // check if current error code is enabled
        if (false == type5_data.isErrorTypeEnabled(static_cast<uint8_t>(GpioSpoofing))) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
        GpioSpoofingPayloadHeader gpioSpoofingHeader{};
        memcpy(&gpioSpoofingHeader, &nrx.data[0], sizeof(GpioSpoofingPayloadHeader));

        if (gpioSpoofingHeader.ei_gpio_number > MaxGPIOSpoofingEntries) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }

        const auto request_data_size = sizeof(GpioSpoofingPayloadHeader)
                                     + gpioSpoofingHeader.ei_gpio_number * sizeof(uint16_t);

        // Handle GPIO Error Injection
        if (!is_input_length_valid(rx, static_cast<uint8_t>(request_data_size))
            || nrx.data_size < static_cast<uint8_t>(request_data_size)) {
            fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
            return;
        }
        // Validate GPIO payload
        if (gpioSpoofingHeader.ei_gpio_number > MaxGPIOSpoofingEntries) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }

        // Direct memcpy is now safe since GPIOSpoofingPayload is trivial
        memcpy(
            &type5_data.gpioSpoofingResp, &nrx.data[0], static_cast<size_t>(request_data_size));
        const auto status = nsm_type5::validateGpioSpoofingErrorInjectionPayload(
            type5_data.gpioSpoofingResp);
        if (status != true) {
            type5_data.gpioSpoofingResp = GPIOSpoofingPayload{};
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }
    else {
        // Reserved error injection IDs
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
}

void Nsm::on_dev_cfg_activate_ErrorInjectionPayload(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    const auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // check if error injection is enabled and if a fault is set
    if (false == type5_data.isErrorInjectionModeEnabled()) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Debug-Token only for Release build, on dev build there is no Debug-Token
    uint8_t    build_type = 0;
    const auto build_ok   = this->fill_build_type(build_type);
    if (true == build_ok && build_type == NsmBuildTypeRel
        && false == debugtoken::is_dbg_token_tlv_in_flash()) {
        fill_error_packet(Ccode::ErrorDebugTokenInstalled, rx, tx);
        return;
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;

    ErrorInjectionId request{};
    memcpy(&request, &nrx.data[0], sizeof(ErrorInjectionId));

    if (request.error_injection_id == static_cast<uint16_t>(DeviceError)) {
        // check if current error code is enabled
        if (false == type5_data.isErrorTypeEnabled(static_cast<uint8_t>(DeviceError))) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }

        switch (request.error_type) {
            case static_cast<uint16_t>(FatalErrors): {
                const auto& fatalErrorPayload = type5_data.getFatalErrorPayload();
                const auto  fatalBitMap       = fatalErrorPayload.fault_payload_bitmap;

                // Only one bit set and it is supported Fatal error bit
                if (__builtin_popcount(fatalBitMap) != 1
                    || (fatalBitMap & FatalFaultEIMask) == 0) {
                    fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                    return;
                }
                // Do not need to clear the status bitmap for fatal error, because it will
                // result in MCU reset to clear the memory.
                type5_data.setDeviceErrorStatusBitmap(DeviceErrorStatusBitmap::FatalError);
                // coverity[cert_int31_c_violation] it will be always less than 0xFF
                Driver::mctp_send_cmd(Driver::CmdCode::NsmT5FatalFaultEI,
                                      static_cast<uint8_t>(fatalBitMap));
                break;
            }

            case static_cast<uint16_t>(PortRecoveryErrors): {
                // TODO: Add API here to activate the port recovery error injection and clear
                // the error injection based on the payload. API: getPortRecoveryPayload()
                // TODO: Need to maintain device_error_status_bitmap for set and clear the
                // error. API available. API:setDeviceErrorStatusBitmap()
                // API:clearDeviceErrorStatusBitmap()

                break;
            }

            case static_cast<uint16_t>(USBBridgeEmulationErrors): {
                // TODO: Add API here to activate the USB bridge emulation error injection and
                // clear the error injection based on the payload. API:
                // getUSBBridgeEmulationPayload()
                // TODO: Need to maintain device_error_status_bitmap for set and clear the
                // error. API available. API:setDeviceErrorStatusBitmap()
                // API:clearDeviceErrorStatusBitmap()
                break;
            }
        }
    }
    else if (request.error_injection_id == static_cast<uint16_t>(GpioSpoofing)) {
        // TODO: Add API here to activate the GPIO spoofing error injection and clear the error
        // injection based on the payload. API: getGPIOErrorStatusBitmap()
        // TODO: Need to maintain gpio_spoofing_error_status_bitmap for set and clear the error.
        // API available. API:setGPIOErrorStatusBitmap() API:clearGPIOErrorStatusBitmap()
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
}

}  // namespace nv::mctp
