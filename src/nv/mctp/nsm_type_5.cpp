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

#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>

#include "nv/bootloader.h"
#include "nv/flash/flash.h"
#include "nv/debugtoken/debugtoken.h"
#include "nv/i2c/error_injection.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/task.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/nsm_pwr_smoothing_handlers.h"
#include "nv/nv.h"

namespace nv::mctp {

namespace nsm_type5 {

// Protocol type mask and I2C address validation constants
constexpr uint8_t ProtocolTypeMask = 0x3F;  // Bits 0-5 of payload_type contain protocol
constexpr uint8_t MaxI2cAddress    = 0x7F;  // Maximum valid 7-bit I2C address

constexpr uint32_t
                   valueFatalFaultEIMCUException    = (1u << static_cast<uint32_t>(
                                         FatalFaultEIPayloadValues::FatalFaultEIMCUException));
constexpr uint32_t valueFatalFaultEIWatchdogTimeout = (1u << static_cast<uint32_t>(
                                                           FatalFaultEIPayloadValues::
                                                               FatalFaultEIWatchdogTimeout));
constexpr uint32_t
    maskPortRecoveryL1EI = (1u << static_cast<uint32_t>(
                                PortRecoveryEIPayloadValues::PortRecoveryL1MCTPEPStall));
constexpr uint32_t
    maskPortRecoveryL2EI = (1u << static_cast<uint32_t>(
                                PortRecoveryEIPayloadValues::PortRecoveryL2MCTPBridgeHang))
                         | static_cast<uint32_t>(
                               1u << static_cast<uint32_t>(
                                   PortRecoveryEIPayloadValues::PortRecoveryL2PLDMT5Hang));
constexpr uint32_t maskPortRecoveryL3EI = 0;

constexpr uint8_t lowDeviceIndexGpuDegradeMode  = 0x80;
constexpr uint8_t highDeviceIndexGpuDegradeMode = 0x87;

constexpr uint8_t highDeviceIndexPowerSupply = 0x07;

// NCSI MAC: 6-byte address; PDS stores as Low (bytes 0-3) and High (bytes 4-5).
constexpr size_t   kNcsiMacLength     = 6U;
constexpr unsigned kNcsiMacByte3Shift = 24;  // shift for byte 3 in uint32_t
constexpr std::array<uint8_t, kNcsiMacLength> kNcsiZeroMac{};

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

void on_nsm_t5_protocol_ei(uint8_t protocol_type,
                           uint8_t bus_number,
                           uint8_t bus_error,
                           uint8_t address)
{
    // Check if this is for I2C protocol (0x02) or I2C PCA9555 protocol (0x05)
    if (protocol_type == static_cast<uint8_t>(nv::i2c::ProtocolType::I2c)
        || protocol_type == static_cast<uint8_t>(nv::i2c::ProtocolType::I2cPca9555)) {
        bool valid_port = false;

        if (protocol_type == static_cast<uint8_t>(nv::i2c::ProtocolType::I2cPca9555)) {
            // For IOX (PCA9555), validate by checking if the port exists in the mapping table
            if constexpr (nv::i2c::NV_IOX_ERROR_INJECTION_PORTS > 0) {
                const auto port  = static_cast<nv::i2c::Port>(bus_number);
                const auto index = nv::i2c::port_to_error_injection_index(port, protocol_type);

                if (index < nv::i2c::NV_I2C_MAX_ERROR_INJECTION_PORTS) {
                    valid_port = true;
                }
                else {
                    nv::warn(
                        "NSM T5: Invalid IOX port %d (Port enum=%d) not found in error "
                        "injection "
                        "mapping table\n",
                        bus_number,
                        static_cast<uint8_t>(port));
                }
            }
            else {
                valid_port = false;
                nv::warn(
                    "NSM T5: IOX error injection is not configured "
                    "(NV_IOX_ERROR_INJECTION_PORTS = "
                    "0)\n");
            }
        }
        else {
            // For I2C, validate by checking if the port exists in the mapping table
            if constexpr (nv::i2c::NV_I2C_ERROR_INJECTION_PORTS > 0) {
                const auto port  = static_cast<nv::i2c::Port>(bus_number);
                const auto index = nv::i2c::port_to_error_injection_index(port, protocol_type);

                if (index < nv::i2c::NV_I2C_MAX_ERROR_INJECTION_PORTS) {
                    valid_port = true;
                }
                else {
                    nv::warn(
                        "NSM T5: Invalid I2C port %d (Port enum=%d) not found in error "
                        "injection "
                        "mapping table\n",
                        bus_number,
                        static_cast<uint8_t>(port));
                }
            }
            else {
                valid_port = false;
                nv::warn(
                    "NSM T5: I2C error injection is not configured "
                    "(NV_I2C_ERROR_INJECTION_PORTS = "
                    "0)\n");
            }
        }

        if (valid_port) {
            const auto port = static_cast<nv::i2c::Port>(bus_number);
            nv::i2c::enable_error_injection(port,
                                            static_cast<nv::i2c::ErrorInjectionType>(bus_error),
                                            address,
                                            static_cast<nv::i2c::ProtocolType>(protocol_type));
        }
    }
    // Check if this is for SPI protocol
    else if (protocol_type == static_cast<uint8_t>(USBBridgeProtocolType::Spi)) {}
    // Other protocols can be handled here in the future
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
    USBBridgeEmulationPayload& usbBridgeEmulationPayload)
{
    // Validate bus number (should be 0-7 for I2C buses)
    if (usbBridgeEmulationPayload.bus_number >= nv::i2c::MaxErrorInjectionPorts) {
        return false;
    }

    // Extract protocol type from payload_type (bits 0-5)
    const uint8_t protocol_type = usbBridgeEmulationPayload.payload_type & ProtocolTypeMask;

    // Validate protocol type (I2C, SPI, I2C PCA9555)
    if (protocol_type != static_cast<uint8_t>(USBBridgeProtocolType::I2c)
        && protocol_type != static_cast<uint8_t>(USBBridgeProtocolType::Spi)
        && protocol_type != static_cast<uint8_t>(USBBridgeProtocolType::I2cPca9555)) {
        return false;
    }

    // Validate error code (should be one of the defined error codes)
    if (usbBridgeEmulationPayload.error > USBBridgeErrorUSBQueueFull) {
        return false;
    }

    // Address validation: 0x0 means not applied, otherwise should be valid 7-bit I2C address
    if (usbBridgeEmulationPayload.address > MaxI2cAddress) {
        return false;
    }

    return true;
}

void triggerGpioSpoofingEvents(const GPIOSpoofingPayload& gpioSpoofingPayload)
{
    std::array<uint32_t, sys::gpio::PortsNumber> port_flags{};
    const auto gpio_count = gpioSpoofingPayload.gpio_spoofing_header.ei_gpio_number;

    for (uint16_t i = 0; i < gpio_count && i < MaxGPIOSpoofingEntries; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto& entry = gpioSpoofingPayload.gpio_ei_entries[i];
        if (entry.activated != 1) {
            continue;
        }

        const auto gpio_index = entry.gpioIndex;
        if (gpio_index >= ipc::GpioSetup.size()) {
            continue;
        }

        const auto& gpio_config = ipc::GpioSetup.at(gpio_index);
        const auto  port        = std::get<0>(gpio_config);
        const auto  pin         = std::get<1>(gpio_config);

        if (port == gpio::InvalidGpioPort || pin == gpio::InvalidGpioPin
            || port >= port_flags.size() || pin >= sys::gpio::PinsPerPort) {
            continue;
        }

        port_flags.at(port) |= (1u << pin);
    }

    for (size_t port = 0; port < port_flags.size(); ++port) {
        if (port_flags.at(port) != 0) {
            Nsm::GpioEventTrigger(
                static_cast<gpio::GpioPort>(port), port_flags.at(port), SpoofingGpio);
        }
    }
}

bool validateGpioSpoofingErrorInjectionPayload(GPIOSpoofingPayload& gpioSpoofingPayload)
{
    // Validate that all GPIO indices are input GPIOs
    // Reject if any GPIO is an output GPIO (MCU controls the pin)
    for (uint16_t i = 0; i < gpioSpoofingPayload.gpio_spoofing_header.ei_gpio_number; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto gpio_index = gpioSpoofingPayload.gpio_ei_entries[i].gpioIndex;

        // Check if GPIO index is within bounds
        if (gpio_index >= ipc::GpioNum) {
            return false;
        }

        // Get GPIO port and pin from configuration
        const auto& gpio_config = ipc::GpioSetup.at(gpio_index);
        const auto  port        = std::get<0>(gpio_config);
        const auto  pin         = std::get<1>(gpio_config);

        // Check if this is a valid hardware GPIO (not virtual)
        if (port == gpio::InvalidGpioPort || pin == gpio::InvalidGpioPin) {
            return false;
        }

        // Get GPIO direction using GPIO driver API
        gpio::Direction dir = gpio::Direction::Input;
        if (gpio::Driver::getDirection(port, pin, dir) != gpio::Status::Ok) {
            return false;
        }

        // Reject if GPIO is configured as output (MCU controls the pin)
        if (dir == gpio::Direction::Output) {
            return false;
        }
    }
    return true;
}

bool validateDeviceIndexGpuDegradeMode(uint8_t device_index)
{
    if (device_index < lowDeviceIndexGpuDegradeMode
        || device_index > highDeviceIndexGpuDegradeMode) {
        return false;
    }
    return true;
}

bool validateActionGpuDegradeMode(uint8_t action)
{
    if (action != NsmDevCfgEnablingMode::Disable && action != NsmDevCfgEnablingMode::Enable) {
        return false;
    }
    return true;
}

bool validateSmaBaseboardRequest(uint8_t data_index)
{
    for (const auto valid_index : smaBaseboardSettingsRequests) {
        if (data_index == valid_index) {
            return true;
        }
    }
    return false;
}

Ccode set_sma_baseboard_sets_resp_wp(nv::mctp::T5SmaBaseboardSetsResponse& response)
{
    if constexpr (WriteProtectionSize > 0) {
        uint8_t enabled = 0;
        for (const auto& config : nv::mctp::WriteProtectionList) {
            auto status = nv::gpio::Driver::read(config.port, config.pin, enabled);
            if (status != nv::gpio::Status::Ok) {
                return Ccode::ErrorGeneral;
            }
            // set response gpio when set/enabled
            if (enabled) {
                bool found = false;
                for (const auto& funcRespBitPos : nv::mctp::smaBaseboardSetsBitResp) {
                    if (std::get<0>(funcRespBitPos) == config.function) {
                        found = true;
                        nsm_msg::set_bit(response.bitarray, std::get<1>(funcRespBitPos));
                    }
                }
                if (false == found) {
                    // missing or wrong data in nv::mctp::smaBaseboardSetsBitResp
                    return Ccode::ErrorGeneral;
                }
            }
        }
    }
    return Ccode::Success;
}

}  // namespace nsm_type5

bool Nsm::process_device_configuration(const Packet& rx, Packet& tx)
{
    using cmd       = nv::mctp::NsmDevCfgCmdCode;
    const auto& nrx = NsmPktReq::from(rx);
    auto&       ntx = NsmPktResp::from(tx);

    ntx.nv_msg_type = nrx.nv_msg_type;
    ntx.set_dev_cfg_code(nrx.get_dev_cfg_code());

    // Helper to handle unsupported commands
    [[maybe_unused]] auto unsupported_command = [&]() {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return false;
    };

    switch (nrx.get_dev_cfg_code()) {
        case cmd::SetErrorInjectionMode:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::SetErrorInjectionMode)) {
                on_dev_cfg_set_errorInjectionMode(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetErrorInjectionMode:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::GetErrorInjectionMode)) {
                on_dev_cfg_get_errorInjectionMode(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetSupportedErrorInjectionTypes:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::GetSupportedErrorInjectionTypes)) {
                on_dev_cfg_get_supportedErrorInjectionTypes(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetCurrentErrorInjectionTypes:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::SetCurrentErrorInjectionTypes)) {
                on_dev_cfg_set_currentErrorInjectionTypes(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetCurrentErrorInjectionTypes:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::GetCurrentErrorInjectionTypes)) {
                on_dev_cfg_get_currentErrorInjectionTypes(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetErrorInjectionPayload:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::GetErrorInjectionPayload)) {
                on_dev_cfg_get_ErrorInjectionPayload(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetErrorInjectionPayload:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::SetErrorInjectionPayload)) {
                on_dev_cfg_submit_ErrorInjectionPayload(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::ActivateErrorInjection:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::ActivateErrorInjection)) {
                on_dev_cfg_activate_ErrorInjectionPayload(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetGpuDegradeMode:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration, cmd::SetGpuDegradeMode)) {
                on_dev_cfg_set_gpu_degrade_mode(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::EnableDisablePowerSupply:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::EnableDisablePowerSupply)) {
                on_dev_cfg_enable_disable_power_supply(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetSmaBaseboardSettings:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::GetSmaBaseboardSettings)) {
                on_dev_cfg_get_sma_baseboard_settings(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetDeviceModeSettings:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::SetDeviceModeSettings)) {
                on_dev_cfg_set_device_mode_settings(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetDeviceModeSettings:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::GetDeviceModeSettings)) {
                on_dev_cfg_get_device_mode_settings(rx, tx);
                break;
            }
            return unsupported_command();
        case cmd::GetSupportedDeviceModes:
            if constexpr (is_cmd_set(NsmMsgType::DeviceConfiguration,
                                     cmd::GetSupportedDeviceModes)) {
                on_dev_cfg_get_supported_device_modes(rx, tx);
                break;
            }
            return unsupported_command();
        default: return unsupported_command();
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
                          + nsm_msg::NsmT5SuppErrorTyesNum;
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = nsm_msg::NsmT5SuppErrorTyesNum;

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
        || nrx.data_size != nsm_msg::NsmT5SuppErrorTyesNum) {
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
    std::array<uint8_t, nsm_msg::NsmT5SuppErrorTyesNum> msg_with_bitmask{0};
    memcpy(&msg_with_bitmask, &nrx.data, nsm_msg::NsmT5SuppErrorTyesNum);

    // Check if Einj Type/ID is supported
    if (!type5_data.isErrorInjectionIdBitmapSupported(msg_with_bitmask)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // To simplify the code, only loop through the supported error types
    for (size_t index = 0; index < nsm_msg::NsmT5SuppErrorTyesNum; ++index) {
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
            type5_data.clearErrorInjectionPayload(
                static_cast<uint8_t>(ErrorInjectionID::GpioSpoofing));
            notifyIoxSpoofingState(false);
        }
        // Check if GpioSpoofing bit is set in this byte
        if (gpio_bit_offset >= 0 && gpio_bit_offset < CHAR_BIT
            && (error_bitmap & (1 << gpio_bit_offset))) {
            notifyIoxSpoofingState(true);
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
                          + nsm_msg::NsmT5SuppErrorTyesNum;
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = nsm_msg::NsmT5SuppErrorTyesNum;
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
        GPIOSpoofingPayload gpioSpoofingPayload{};
        memcpy(&gpioSpoofingPayload, &nrx.data[0], static_cast<size_t>(request_data_size));
        const auto status = nsm_type5::validateGpioSpoofingErrorInjectionPayload(
            gpioSpoofingPayload);
        if (status != true) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }

        // Direct memcpy is now safe since GPIOSpoofingPayload is trivial
        memcpy(
            &type5_data.gpioSpoofingResp, &nrx.data[0], static_cast<size_t>(request_data_size));

        // Set polarity and clear activated bit for each GPIO error injection entry
        auto gpio_count = type5_data.gpioSpoofingResp.gpio_spoofing_header.ei_gpio_number;
        for (uint16_t i = 0; i < gpio_count && i < MaxGPIOSpoofingEntries; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            auto&   gpio_entry = type5_data.gpioSpoofingResp.gpio_ei_entries[i];
            uint8_t default_value{};
            get_gpio_default_value(gpio_entry.gpioIndex, default_value);

            // Set default value to polarit bit for each GPIO error injection entry
            gpio_entry.polarity = default_value;
            // Clear activated bit for each GPIO error injection entry
            gpio_entry.activated = 0;
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
                // Get the Port Recovery payload
                const auto& portRecoveryPayload = type5_data.getPortRecoveryPayload();
                // Extract the recovery bitmaps from the payload
                const uint32_t l1 = static_cast<uint32_t>(
                                        portRecoveryPayload.l1_recovery_bitmap)
                                  & nv::mctp::nsm_type5::maskPortRecoveryL1EI;
                const uint32_t l2 = static_cast<uint32_t>(
                                        portRecoveryPayload.l2_recovery_bitmap)
                                  & nv::mctp::nsm_type5::maskPortRecoveryL2EI;
                const uint32_t l3 = static_cast<uint32_t>(
                                        portRecoveryPayload.l3_recovery_bitmap)
                                  & nv::mctp::nsm_type5::maskPortRecoveryL3EI;
                const uint32_t ei_bitmap = l1 | (l2 << 8) | (l3 << 16);

                if (ei_bitmap != 0) {
                    type5_data.setDeviceErrorStatusBitmap(
                        DeviceErrorStatusBitmap::PortRecoveryError);
                }
                else {
                    type5_data.clearDeviceErrorStatusBitmap(
                        DeviceErrorStatusBitmap::PortRecoveryError);
                }

                nv::mctp::get_task().set_port_recovery_ei_bitmap(ei_bitmap);
                break;
            }

            case static_cast<uint16_t>(USBBridgeEmulationErrors): {
                // Get the USB Bridge Emulation payload
                const auto& usbBridgeEmulationPayload = type5_data
                                                            .getUSBBridgeEmulationPayload();

                // Extract protocol type from payload_type (bits 0-5)
                const uint8_t protocol_type = usbBridgeEmulationPayload.payload_type
                                            & nsm_type5::ProtocolTypeMask;

                // Check if this is a clear error command
                if (usbBridgeEmulationPayload.error == USBBridgeErrorClear) {
                    // Clear the error injection
                    type5_data.clearDeviceErrorStatusBitmap(
                        DeviceErrorStatusBitmap::USBBridgeEmulationError);
                }
                else {
                    // Set the device error status bitmap
                    type5_data.setDeviceErrorStatusBitmap(
                        DeviceErrorStatusBitmap::USBBridgeEmulationError);
                }

                // Activate the protocol error injection
                nsm_type5::on_nsm_t5_protocol_ei(protocol_type,
                                                 usbBridgeEmulationPayload.bus_number,
                                                 usbBridgeEmulationPayload.error,
                                                 usbBridgeEmulationPayload.address);
                break;
            }
        }
    }
    else if (request.error_injection_id == static_cast<uint16_t>(GpioSpoofing)) {
        if (false == type5_data.isErrorTypeEnabled(static_cast<uint8_t>(GpioSpoofing))) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }

        // For GPIO spoofing, error_type is reserved per design doc

        // Activate GPIO spoofing error
        auto gpio_count = type5_data.gpioSpoofingResp.gpio_spoofing_header.ei_gpio_number;
        type5_data.setGPIOErrorStatusBitmap(GPIOErrorStatusBitmap::GPIOSpoofingError);

        // Update polarity and activated for each GPIO error injection
        for (uint16_t i = 0; i < gpio_count && i < MaxGPIOSpoofingEntries; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            auto&      gpio_entry = type5_data.gpioSpoofingResp.gpio_ei_entries[i];
            const auto gpio_index = gpio_entry.gpioIndex;

            // Validate GPIO index is within bounds
            if (gpio_index < ipc::GpioNum) {
                // Get GPIO spoofing configuration
                uint8_t default_value{};
                get_gpio_default_value(gpio_entry.gpioIndex, default_value);

                // Set polarity to inverted default_value
                gpio_entry.polarity = (default_value == 1) ? 0 : 1;

                // Set activated bit
                gpio_entry.activated = 1;
            }
        }
        // trigger event
        nsm_type5::triggerGpioSpoofingEvents(type5_data.gpioSpoofingResp);
        notifyIoxSpoofingState(true);
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
}

void Nsm::on_dev_cfg_set_gpu_degrade_mode(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    if (!is_input_length_valid(rx, sizeof(NsmDevCfgGpuDegradeModeRequest))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    NsmDevCfgGpuDegradeModeRequest request{};
    std::memcpy(&request.device_index, &nrx.data[0], sizeof(NsmDevCfgGpuDegradeModeRequest));

    if (!nsm_type5::validateDeviceIndexGpuDegradeMode(request.device_index)
        || !nsm_type5::validateActionGpuDegradeMode(request.action)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;

    // TODO: Implement GPU Degrade Mode here
    nv::info("%s() OK\n", __func__);
}

void Nsm::on_dev_cfg_enable_disable_power_supply(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    if (!is_input_length_valid(rx, sizeof(T5PowerSupplyRequest))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    T5PowerSupplyRequest request{};

    // check request data
    std::memcpy(&request.device_index, &nrx.data[0], sizeof(T5PowerSupplyRequest));
    if (request.device_index > nsm_type5::highDeviceIndexPowerSupply) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    if (request.mode != nv::mctp::Enable && request.mode != nv::mctp::Disable) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;

    // TODO: Implement Enable/Disable Power Supply here
    nv::info("%s() OK\n", __func__);
}

void Nsm::on_dev_cfg_get_sma_baseboard_settings(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    if (!is_input_length_valid(rx, sizeof(T5SmaBaseboardSetsRequest))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    T5SmaBaseboardSetsRequest request{};

    // check request data
    std::memcpy(&request.data_index, &nrx.data[0], sizeof(T5SmaBaseboardSetsRequest));
    if (false == nsm_type5::validateSmaBaseboardRequest(request.data_index)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    T5SmaBaseboardSetsResponse response{};
    auto&                      ntx = NsmPktResp::from(tx);
    ntx.completion_code            = Ccode::Success;

    switch (request.data_index) {
        case SmaBaseboardSets::WPSettings:
            ntx.completion_code = nsm_type5::set_sma_baseboard_sets_resp_wp(response);
            break;
        default: break;
    }
    if (ntx.completion_code != Ccode::Success) {
        fill_error_packet(ntx.completion_code, rx, tx);
        return;
    }

    ntx.data_size         = sizeof(response);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + sizeof(response);

    memcpy(&ntx.data[0], response.bitarray.data(), sizeof(response));
}

void Nsm::on_dev_cfg_set_device_mode_settings(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    const auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Require at least the mode index (32‑bit) to be present
    if (nrx.data_size < sizeof(uint32_t)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    uint32_t raw_mode_index = 0;
    std::memcpy(&raw_mode_index, &nrx.data[0], sizeof(raw_mode_index));
    const auto mode_index = static_cast<DeviceModeIndex>(raw_mode_index);

    const auto* payload_bytes = &nrx.data[sizeof(raw_mode_index)];
    float       ramp_rate     = 0.0f;

    Ccode result = Ccode::ErrorInvalidData;
    switch (mode_index) {
        case DeviceModeIndex::MaxACPowerRampRate:
            if (nrx.data_size != sizeof(DeviceModeIndex) + sizeof(float)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }
            // Copy float value instead of reinterpreting pointer
            std::memcpy(&ramp_rate, &payload_bytes[0], sizeof(ramp_rate));
            result = nsm_pwr_smoothing_handlers::handle_set_max_ac_ramp_rate(ramp_rate);
            break;
        case DeviceModeIndex::SoCPowerSmoothEnabled:
            if (nrx.data_size != sizeof(DeviceModeIndex) + sizeof(bool)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }
            result = nsm_pwr_smoothing_handlers::handle_set_soc_power_smooth_enabled(
                payload_bytes[0] != 0);
            break;
        case DeviceModeIndex::SoCPowerSmoothCurrentPresetIndex:
            if (nrx.data_size != sizeof(DeviceModeIndex) + sizeof(uint8_t)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }
            result = nsm_pwr_smoothing_handlers::handle_set_soc_power_smooth_current_preset(
                payload_bytes[0]);
            break;
        case DeviceModeIndex::SoCPowerBrakeEnabled:
            if (nrx.data_size != sizeof(DeviceModeIndex) + sizeof(bool)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }
            result = nsm_pwr_smoothing_handlers::handle_set_soc_power_brake_enabled(
                payload_bytes[0] != 0);
            break;
        case DeviceModeIndex::SoCThermBrakeEnabled:
            if (nrx.data_size != sizeof(DeviceModeIndex) + sizeof(bool)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }
            result = nsm_pwr_smoothing_handlers::handle_set_soc_therm_brake_enabled(
                payload_bytes[0] != 0);
            break;
        case DeviceModeIndex::NcsiMac: result = handle_set_ncsi_mac(payload_bytes); break;
        default                      : fill_error_packet(Ccode::ErrorInvalidData, rx, tx); return;
    }
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

void Nsm::on_dev_cfg_get_device_mode_settings(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    if (!is_input_length_valid(rx, sizeof(uint32_t))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& device_mode_index = *std::bit_cast<uint32_t*>(&nrx.data[0]);
    auto& ntx               = NsmPktResp::from(tx);

    Ccode result = Ccode::ErrorInvalidData;
    switch (static_cast<DeviceModeIndex>(device_mode_index)) {
        case DeviceModeIndex::MaxACPowerRampRate:
            result = nsm_pwr_smoothing_handlers::handle_get_max_ac_ramp_rate(ntx);
            break;
        case DeviceModeIndex::SoCPowerSmoothEnabled:
            result = nsm_pwr_smoothing_handlers::handle_get_soc_power_smooth_enabled(ntx);
            break;
        case DeviceModeIndex::SoCPowerSmoothCurrentPresetIndex:
            result = nsm_pwr_smoothing_handlers::handle_get_soc_power_smooth_current_preset(
                ntx);
            break;
        case DeviceModeIndex::SoCPowerBrakeEnabled:
            result = nsm_pwr_smoothing_handlers::handle_get_soc_power_brake_enabled(ntx);
            break;
        case DeviceModeIndex::SoCThermBrakeEnabled:
            result = nsm_pwr_smoothing_handlers::handle_get_soc_therm_brake_enabled(ntx);
            break;
        case DeviceModeIndex::NcsiMac: result = handle_get_ncsi_mac(ntx); break;
        default                      : break;
    }

    // Check if feature is available
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_dev_cfg_get_supported_device_modes(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Validate request size - must contain Handle (uint32_t)
    if (nrx.data_size < sizeof(uint32_t)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // Parse handle from request
    uint32_t request_handle = 0;
    std::memcpy(&request_handle, &nrx.data[0], sizeof(request_handle));

    // For first request, handle must be 0
    // Since all supported modes fit in one response, we only accept handle = 0
    if (request_handle != 0) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx = NsmPktResp::from(tx);

    // Delegate to handler (weak returns ErrorUnsupportedCmd, strong populates response)
    const Ccode result = nsm_pwr_smoothing_handlers::handle_get_supported_device_modes(ntx);
    if (result != Ccode::Success) {
        fill_error_packet(result, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}
namespace {
/** Load current NCSI MAC from PDS when in-memory current is all zeros. */
static void ensure_current_ncsi_mac_loaded(NsmDevCfgPersistentData& data)
{
    if (data.currentNcsiMacAddrLoaded) {
        return;
    }
    nv::flash::Data low  = 0;
    nv::flash::Data high = 0;
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsNcsiMacAddrLow, low)
        != nv::flash::Status::Ok) {
        return;
    }
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsNcsiMacAddrHigh, high)
        != nv::flash::Status::Ok) {
        return;
    }
    // PdsNcsiMacAddrLow: bytes 0,1,2,3; PdsNcsiMacAddrHigh: bytes 4,5 (low 16 bits)
    data.currentNcsiMacAddr[0]    = static_cast<uint8_t>(low >> 0);
    data.currentNcsiMacAddr[1]    = static_cast<uint8_t>(low >> 8);
    data.currentNcsiMacAddr[2]    = static_cast<uint8_t>(low >> 16);
    data.currentNcsiMacAddr[3]    = static_cast<uint8_t>(low >> nsm_type5::kNcsiMacByte3Shift);
    data.currentNcsiMacAddr[4]    = static_cast<uint8_t>(high >> 0);
    data.currentNcsiMacAddr[5]    = static_cast<uint8_t>(high >> 8);
    data.currentNcsiMacAddrLoaded = true;
}
}  // namespace
Ccode Nsm::handle_set_ncsi_mac(const uint8_t* data)
{
    if (data == nullptr) {
        return Ccode::ErrorInvalidData;
    }

    ensure_current_ncsi_mac_loaded(type5_data);

    std::memcpy(
        type5_data.pendingNcsiMacAddr.data(), data, nv::mctp::nsm_type5::kNcsiMacLength);

    // PDS: Low = bytes 0..3, High = bytes 4..5 (low 16 bits)
    const nv::flash::Data low = static_cast<nv::flash::Data>(data[0])
                              | (static_cast<nv::flash::Data>(data[1]) << 8)
                              | (static_cast<nv::flash::Data>(data[2]) << 16)
                              | (static_cast<nv::flash::Data>(data[3]) << 24);
    const nv::flash::Data high = static_cast<nv::flash::Data>(data[4])
                               | (static_cast<nv::flash::Data>(data[5]) << 8);

    if (nv::flash::Flash::set_data(nv::flash::Key::PdsNcsiMacAddrLow, low)
        != nv::flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }
    if (nv::flash::Flash::set_data(nv::flash::Key::PdsNcsiMacAddrHigh, high)
        != nv::flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode Nsm::handle_get_ncsi_mac(NsmPktResp& ntx)
{
    ensure_current_ncsi_mac_loaded(type5_data);

    const auto current_length = static_cast<uint16_t>(type5_data.currentNcsiMacAddr.size());
    uint16_t   pending_length = 0;

    // Response: [current_length, pending_length, current_value[, pending_value]]
    memcpy(&ntx.data[0], &current_length, sizeof(uint16_t));
    memcpy(&ntx.data[2], &pending_length, sizeof(uint16_t));
    memcpy(&ntx.data[4],
           type5_data.currentNcsiMacAddr.data(),
           type5_data.currentNcsiMacAddr.size());

    // Include pending only when it differs from current and is not all zeros
    const bool pending_differs  = (type5_data.currentNcsiMacAddr
                                  != type5_data.pendingNcsiMacAddr);
    const bool pending_non_zero = (type5_data.pendingNcsiMacAddr
                                   != nv::mctp::nsm_type5::kNcsiZeroMac);
    if (pending_differs && pending_non_zero) {
        pending_length = static_cast<uint16_t>(type5_data.pendingNcsiMacAddr.size());
        memcpy(&ntx.data[2], &pending_length, sizeof(uint16_t));
        memcpy(&ntx.data[4] + current_length,
               type5_data.pendingNcsiMacAddr.data(),
               type5_data.pendingNcsiMacAddr.size());
    }

    ntx.data_size = sizeof(uint16_t) + sizeof(uint16_t) + current_length + pending_length;
    return Ccode::Success;
}

};  // namespace nv::mctp
