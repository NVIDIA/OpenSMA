/**
 * Copyright (c) 2024-2025, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once

#include <array>
#include <cstdint>
#include "nv/i2c/common.h"
#include "nv/i2c/port.h"
#include "nv/ipchandler/enums.h"
#include NV_IPC_CONFIG_H

namespace nv::i2c {

enum class ErrorInjectionType : uint8_t
{
    Clear        = 0x00,  // Clear error injection
    QueueFull    = 0x01,  // Queue full error
    Nack         = 0x02,  // NACK error
    Timeout      = 0x03,  // Timeout error
    UsbQueueFull = 0x04   // USB Queue Full error
};

enum class ProtocolType : uint8_t
{
    I2c        = 0x02,  // Standard I2C protocol
    I2cPca9555 = 0x05   // I2C PCA9555 expander protocol
};

struct ErrorInjectionConfig
{
    bool    enabled;         // Whether error injection is enabled
    uint8_t error_type;      // Type of error to inject
    uint8_t target_address;  // Target I2C address for error injection (0x0: not applied)
    uint8_t protocol_type;   // Protocol type (0x02 = I2C, 0x05 = I2C_PCA9555)
};

constexpr size_t MaxErrorInjectionPorts = NV_I2C_MAX_ERROR_INJECTION_PORTS;

//  lookup ipchandler_id in the table and return port
inline constexpr Port ipchandler_to_error_injection_port(nv::ipchandler::Id id)
{
    for (const auto& mapping : ErrorInjectionPortMappingTable) {
        if (mapping.ipchandler_id == id) {
            return mapping.port;
        }
    }
    return Port::Begin;  // Invalid/unsupported bus
}

// Map Port enum value to array index in error injection config array
// Both I2C and IOX entries are in ErrorInjectionPortMappingTable
// - protocol_type 0x02 (I2C): Searches for matching port (ipchandler_id = I2c0 to I2c6+)
// - protocol_type 0x05 (IOX): Searches for matching port (ipchandler_id = Iox)
// Returns index if valid, otherwise returns NV_I2C_MAX_ERROR_INJECTION_PORTS (invalid index)
inline constexpr size_t port_to_error_injection_index(Port port, uint8_t protocol_type)
{
    if (protocol_type == static_cast<uint8_t>(ProtocolType::I2c)) {
        // I2C: Search for I2C handler with matching port
        for (size_t i = 0; i < ErrorInjectionPortMappingTable.size(); ++i) {
            if (ErrorInjectionPortMappingTable[i].ipchandler_id != ipchandler::Id::Iox
                && ErrorInjectionPortMappingTable[i].port == port) {
                return i;  // Found I2C handler
            }
        }
    }
    else if (protocol_type == static_cast<uint8_t>(ProtocolType::I2cPca9555)) {
        // IOX: Search for IOX entry with matching port offset
        for (size_t i = 0; i < ErrorInjectionPortMappingTable.size(); ++i) {
            if (ErrorInjectionPortMappingTable[i].ipchandler_id == ipchandler::Id::Iox
                && ErrorInjectionPortMappingTable[i].port == port) {
                return i;  // Found IOX device
            }
        }
    }

    return NV_I2C_MAX_ERROR_INJECTION_PORTS;  // Invalid index
}

std::array<ErrorInjectionConfig, MaxErrorInjectionPorts>& get_error_injection_configs();

void      enable_error_injection(Port               port,
                                 ErrorInjectionType error_type,
                                 uint8_t            address       = 0x0,
                                 ProtocolType       protocol_type = ProtocolType::I2c);
bool      should_inject_error(Port    port,
                              uint8_t address,
                              uint8_t operation_type,
                              uint8_t protocol_type);
bool      should_inject_error(ipchandler::Id ipchandler_id,
                              uint8_t        address,
                              uint8_t        operation_type,
                              uint8_t        protocol_type);
I2cStatus get_injected_error_status(Port port, uint8_t protocol_type);
void      clear_error_injection(Port port, uint8_t protocol_type);

}  // namespace nv::i2c
