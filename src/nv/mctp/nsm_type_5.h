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
#pragma once
#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>

namespace nv::mctp {

constexpr uint8_t NsmMaxSupportedErrorTypeBitmapBytesNum = 8;
constexpr uint8_t NsmDevCfgCommonSupportedNum            = 8;

// NVIDIA TYPE 5 Device Configuration Command Code
enum class NsmDevCfgCmdCode : uint8_t
{
    SetErrorInjectionMode           = 0x03,
    GetErrorInjectionMode           = 0x04,
    GetSupportedErrorInjectionTypes = 0x05,
    SetCurrentErrorInjectionTypes   = 0x06,
    GetCurrentErrorInjectionTypes   = 0x07,
    GetErrorInjectionPayload        = 0x0A,
    SetErrorInjectionPayload        = 0x0B,
    ActivateErrorInjection          = 0x0C,
};

/** Common Enable Disable enums */
enum NsmDevCfgEnablingMode : uint8_t
{
    Disable = 0x00,
    Enable  = 0x01,
};

// Error Injection ID values
enum ErrorInjectionID : uint8_t
{
    DeviceError         = 0x04,
    GpioSpoofing        = 0x05,
    MaxErrorInjectionID = GpioSpoofing,

    ErrorInjectionIDBitMap = (1u << DeviceError) | (1u << GpioSpoofing)
};

constexpr uint8_t MaxEinjIDBytesNum = (static_cast<uint8_t>(
                                           ErrorInjectionID::MaxErrorInjectionID)
                                       / 8)
                                    + 1;

// Error Type values
enum ErrorType : uint8_t
{
    FatalErrors              = 0x0,
    PortRecoveryErrors       = 0x1,
    USBBridgeEmulationErrors = 0x2,
    LeakDetectionErrors      = 0x3,
};

enum FatalFaultEIPayloadValues : uint32_t
{
    FatalFaultEIMCUException    = 0,  // bit 0
    FatalFaultEIWatchdogTimeout = 1,  // bit 1
    FatalFaultEIMask = (1u << FatalFaultEIMCUException) | (1u << FatalFaultEIWatchdogTimeout)
};

// Port Recovery Errors payload values
enum PortRecoveryEIPayloadValues : uint8_t
{
    // L1 transport/protocol reset recovery bitmap (Offset 8)
    PortRecoveryL1MCTPEPStall = 0,  // bit 0: MCTP EP stall (MCU support)

    // L2 interface/port reset recovery bitmap (Offset 9)
    PortRecoveryL2MCTPBridgeHang = 1,  // bit 1: MCTP bridge hang (MCU support)
    PortRecoveryL2PLDMT5Hang     = 2,  // bit 2: PLDM T5 hang (MCU support)

    // L3 device reset recovery bitmap (Offset 10) - reserved 0
};

// USB Bridge Emulation Errors payload values
enum USBBridgeEmulationEIPayloadValues : uint8_t
{
    // USB bridge Emulation Payload Type (Offset 8)
    USBBridgeDeviceRoleNotApplied = 0x0,  // Bit 6-7: 0x0 - Not applied
    USBBridgeDeviceRoleMaster     = 0x1,  // Bit 6-7: 0x1 - Device as Master
    USBBridgeDeviceRoleSlave      = 0x2,  // Bit 6-7: 0x2 - Device as Slave
    USBBridgeI2COverUSB           = 0x2,  // Bit 0-5: 0x2 - I2C over USB (cp2112)

    // Error codes (Offset 10)
    USBBridgeErrorClear             = 0x0,  // 0x0: clear error
    USBBridgeErrorI2CQueueFull      = 0x1,  // 0x1: i2c queue full
    USBBridgeErrorI2CNack           = 0x2,  // 0x2: i2c nack
    USBBridgeErrorI2CReadWriteError = 0x3,  // 0x3: i2c read/write error
    USBBridgeErrorUSBQueueFull      = 0x4,  // 0x4: usb queue full
};

struct [[gnu::packed]] NsmDevCfgErrorInjectionModeResponse
{
    uint8_t  mode;                          //<! initial response
    uint32_t persistent_data_modified : 1;  //<! data modified by error injection
    uint32_t other_flags              : 31;
    // Creator
    NsmDevCfgErrorInjectionModeResponse()
    : mode{NsmDevCfgEnablingMode::Disable}
    , persistent_data_modified{0}
    , other_flags{0}
    {
        // Empty
    }
};

constexpr std::array<uint8_t, NsmDevCfgCommonSupportedNum> gen_type5_code_bitmask()
{
    std::array<uint8_t, NsmDevCfgCommonSupportedNum> bitmask = {0};

    constexpr uint8_t bit_positions[] = {
        static_cast<uint8_t>(NsmDevCfgCmdCode::SetErrorInjectionMode),
        static_cast<uint8_t>(NsmDevCfgCmdCode::GetErrorInjectionMode),
        static_cast<uint8_t>(NsmDevCfgCmdCode::GetSupportedErrorInjectionTypes),
        static_cast<uint8_t>(NsmDevCfgCmdCode::SetCurrentErrorInjectionTypes),
        static_cast<uint8_t>(NsmDevCfgCmdCode::GetCurrentErrorInjectionTypes),
        static_cast<uint8_t>(NsmDevCfgCmdCode::GetErrorInjectionPayload),
        static_cast<uint8_t>(NsmDevCfgCmdCode::SetErrorInjectionPayload),
        static_cast<uint8_t>(NsmDevCfgCmdCode::ActivateErrorInjection),
    };

    for (uint8_t pos : bit_positions) {
        const size_t byte_index = pos / 8;
        const size_t bit_offset = pos % 8;
        const size_t value      = (1u << bit_offset);
        if (value <= UCHAR_MAX) {
            bitmask.at(byte_index) |= static_cast<uint8_t>(value);
        }
    }

    return bitmask;
}

constexpr std::array<uint8_t, NsmMaxSupportedErrorTypeBitmapBytesNum>
gen_type5_supported_errors_injection_bitmask()
{
    std::array<uint8_t, NsmMaxSupportedErrorTypeBitmapBytesNum> bitmask = {0};

    constexpr uint8_t bit_positions[] = {static_cast<uint8_t>(DeviceError),
                                         static_cast<uint8_t>(GpioSpoofing)};

    for (uint8_t pos : bit_positions) {
        const size_t byte_index = pos / 8;
        const size_t bit_offset = pos % 8;
        const size_t value      = (1u << bit_offset);
        if (value <= UCHAR_MAX) {
            bitmask.at(byte_index) |= static_cast<uint8_t>(value);
        }
    }
    return bitmask;
}

// Get Error Injection Payload Request data structure according to spec
struct [[gnu::packed]] ErrorInjectionId
{
    uint16_t error_injection_id;  // Offset 0: 0x4: Device error
    uint16_t error_type;  // Offset 2: Optional: 0x0: Fatal errors, 0x1: Port recovery errors,
                          // 0x2: USB bridge emulation errors
};

// Request data structure according to spec
struct [[gnu::packed]] ErrorInjectionPayloadHeader
{
    uint32_t offset;  // Offset 0: General Offset per T5 message definition. Always 0 for MCU
    uint16_t error_injection_id;  // Offset 4: 0x4: Device error
    uint16_t error_type;          // Offset 6: 0x0: Fatal errors
};

struct [[gnu::packed]] FatalErrorPayload
{
    ErrorInjectionPayloadHeader ei_header{0, 0, 0};  // Use default member initializer
    uint32_t fault_payload_bitmap = 0;  // Offset 8: Bit map for fault control and indication

    // No user-defined constructor - this makes the type trivial
};

// Port Recovery Errors payload structure
struct [[gnu::packed]] PortRecoveryPayload
{
    ErrorInjectionPayloadHeader ei_header{0, 0, 0};  // Use default member initializer
    uint8_t l1_recovery_bitmap = 0;  // Offset 8: L1 transport/protocol reset recovery bitmap
    uint8_t l2_recovery_bitmap = 0;  // Offset 9: L2 interface/port reset recovery bitmap
    uint8_t l3_recovery_bitmap = 0;  // Offset 10: L3 device reset recovery bitmap
    uint8_t reserved_1         = 0;  // Offset 11: Reserved 0 for future use

    // No user-defined constructor - this makes the type trivial
};

// USB Bridge Emulation Errors payload structure
struct [[gnu::packed]] USBBridgeEmulationPayload
{
    ErrorInjectionPayloadHeader ei_header{0, 0, 0};  // Use default member initializer
    uint8_t payload_type = 0;  // Offset 8: USB bridge Emulation Payload Type
    uint8_t bus_number   = 0;  // Offset 9: Bus Number (0x0 to 0x8 - I2C bus)
    uint8_t error        = 0;  // Offset 10: Error code
    uint8_t address      = 0;  // Offset 11: Address (0x0: Not applied)

    // No user-defined constructor - this makes the type trivial
};

// GPIO Error Injection payload structure according to spec
struct [[gnu::packed]] GpioSpoofingPayloadHeader
{
    uint32_t offset;  // Offset 0: General Offset per T5 message definition. Always 0 for MCU
    uint16_t error_injection_id;  // Offset 4: 0x6: GPIO Errors
    uint16_t ei_gpio_number;      // Offset 6: 0x0: GPIO Errors
};

constexpr uint8_t MaxGPIOSpoofingEntries = 16;

struct [[gnu::packed]] GpioSpoofingEntry
{
    uint16_t gpioIndex : 14;
    union
    {
        uint16_t reserved  : 1;  // for request
        uint16_t gpioValue : 1;  // for response
    };
    uint16_t eiOperation : 1;
};

// GPIO Spoofing Response data structure according to spec
struct [[gnu::packed]] GPIOSpoofingPayload
{
    GpioSpoofingPayloadHeader gpio_spoofing_header{0, 0, 0};  // Use default member initializer
    struct GpioSpoofingEntry  gpio_ei_entries[MaxGPIOSpoofingEntries]{0};  // Use default member
                                                                           // initializer

    // No user-defined constructor - this makes the type trivial
};

enum DeviceErrorStatusBitmap : uint8_t
{
    FatalError              = 0,
    PortRecoveryError       = 1,
    USBBridgeEmulationError = 2,
    LeakDetectionError      = 3,
};

enum GPIOErrorStatusBitmap : uint8_t
{
    GPIOSpoofingError = 0,
};

/** single data structure for type persistent data */
struct [[gnu::packed]] NsmDevCfgPersistentData
{
    NsmDevCfgErrorInjectionModeResponse errorInjectionModeResponse;
    static constexpr std::array<uint8_t, NsmDevCfgCommonSupportedNum>
        suppCmdCode = gen_type5_code_bitmask();
    static constexpr std::array<uint8_t, NsmMaxSupportedErrorTypeBitmapBytesNum>
        suppErrorTypes = gen_type5_supported_errors_injection_bitmask();
    std::array<uint8_t, NsmMaxSupportedErrorTypeBitmapBytesNum>
        current_errors_injection_bitmask{};

    uint32_t device_error_status_bitmap        = 0;
    uint32_t gpio_spoofing_error_status_bitmap = 0;

    // keep General Error Injection Payload response: error and bitmap
    FatalErrorPayload fatalerrorResp;

    // Port Recovery Errors payload
    PortRecoveryPayload portRecoveryResp;

    // USB Bridge Emulation Errors payload
    USBBridgeEmulationPayload usbBridgeEmulationResp;

    // GPIO Spoofing Errors payload
    GPIOSpoofingPayload gpioSpoofingResp;

    bool isErrorInjectionModeEnabled()
    {
        return errorInjectionModeResponse.mode == NsmDevCfgEnablingMode::Enable;
    }

    bool isErrorInjectionIDSupported(uint8_t error_injection_id)
    {
        const uint8_t byte_index = error_injection_id / 8;
        const uint8_t bit_offset = error_injection_id % 8;
        if (byte_index < suppErrorTypes.size()) {
            if (suppErrorTypes.at(byte_index) & (1u << bit_offset)) {
                return true;
            }
        }
        return false;
    }

    bool isErrorInjectionIdBitmapSupported(
        const std::array<uint8_t, NsmMaxSupportedErrorTypeBitmapBytesNum>& error_bitmap)
    {
        // Check if error_bitmap contains only supported error types
        // Return false if any unsupported bits are set
        for (size_t index = 0; index < NsmMaxSupportedErrorTypeBitmapBytesNum; ++index) {
            if (error_bitmap.at(index) & ~suppErrorTypes.at(index)) {
                return false;  // Found unsupported bits
            }
        }
        return true;  // All bits are supported (or no bits are set)
    }

    bool isErrorTypeEnabled(uint8_t error_type)
    {
        const uint8_t byte_index = error_type / 8;
        const uint8_t bit_offset = error_type % 8;
        if (byte_index < current_errors_injection_bitmask.size()) {
            if (current_errors_injection_bitmask.at(byte_index) & (1u << bit_offset)) {
                return true;
            }
        }
        return false;
    }

    bool isCurrentErrorInjectionBitmaskCleared()
    {
        return current_errors_injection_bitmask
            == std::array<uint8_t, NsmMaxSupportedErrorTypeBitmapBytesNum>{};
    }

    void clearErrorInjectionPayload(uint8_t error_injection_id)
    {
        if (error_injection_id == static_cast<uint8_t>(ErrorInjectionID::DeviceError)) {
            fatalerrorResp         = FatalErrorPayload{};
            portRecoveryResp       = PortRecoveryPayload{};
            usbBridgeEmulationResp = USBBridgeEmulationPayload{};
            gpioSpoofingResp       = GPIOSpoofingPayload{};
        }
        else if (error_injection_id == static_cast<uint8_t>(ErrorInjectionID::GpioSpoofing)) {
            // TODO: Add API here to clear the GPIO spoofing error
        }
    }

    // Getter APIs for payload variables
    const FatalErrorPayload& getFatalErrorPayload() const { return fatalerrorResp; }

    const PortRecoveryPayload& getPortRecoveryPayload() const { return portRecoveryResp; }

    const USBBridgeEmulationPayload& getUSBBridgeEmulationPayload() const
    {
        return usbBridgeEmulationResp;
    }

    const GPIOSpoofingPayload& getGPIOSpoofingPayload() const { return gpioSpoofingResp; }

    // Set bit to inject the error and clear the bit to clear the error and be able to recover
    // the device to normal operation.
    // TODO: See if need more status options.
    void setDeviceErrorStatusBitmap(DeviceErrorStatusBitmap error)
    {
        device_error_status_bitmap |= (1u << error);
    }

    void setGPIOErrorStatusBitmap(GPIOErrorStatusBitmap error)
    {
        gpio_spoofing_error_status_bitmap |= (1u << error);
    }

    void clearDeviceErrorStatusBitmap(DeviceErrorStatusBitmap error)
    {
        device_error_status_bitmap &= ~(1u << error);
    }

    void clearGPIOErrorStatusBitmap(GPIOErrorStatusBitmap error)
    {
        gpio_spoofing_error_status_bitmap &= ~(1u << error);
    }

    bool isDeviceErrorStatusBitmapCleared(void) { return device_error_status_bitmap == 0; }

    bool isGPIOErrorStatusBitmapCleared(void) { return gpio_spoofing_error_status_bitmap == 0; }

    bool isAllErrorInjectionCleared(void)
    {
        return device_error_status_bitmap == 0 && gpio_spoofing_error_status_bitmap == 0;
    }
};

namespace nsm_type5 {

/**
 * @brief This is the callback for the NSM T5 EI Payload Activate
 * @param bitmap - The NSM T5 EI Payload bitmap
 *                 It identifies which Fatal Fault is going to be launched
 */
void on_nsm_t5_fatal_fault_ei(uint8_t bitmap);

}  // namespace nsm_type5

}  // namespace nv::mctp
