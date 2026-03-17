/**
 * Copyright (c) 2024-2025, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nv/i2c/error_injection.h"
#include "nv/logger/log.h"
#include "nv/iox/task.h"
#include <array>
#include <cstring>

namespace nv::i2c {

std::array<ErrorInjectionConfig, MaxErrorInjectionPorts>& get_error_injection_configs()
{
    NV_SHARED_BSS static std::array<ErrorInjectionConfig, MaxErrorInjectionPorts> configs{};
    return configs;
}

void enable_error_injection(Port               port,
                            ErrorInjectionType error_type,
                            uint8_t            address,
                            ProtocolType       protocol_type)
{
    // If error_type is Clear, directly call clear_error_injection
    if (error_type == ErrorInjectionType::Clear) {
        clear_error_injection(port, static_cast<uint8_t>(protocol_type));
        nv::info("I2C Error Injection: Cleared error injection on port=%d, proto=0x%x\n",
                 static_cast<uint8_t>(port),
                 static_cast<uint8_t>(protocol_type));
        return;
    }

    auto index = port_to_error_injection_index(port, static_cast<uint8_t>(protocol_type));
    if (index < MaxErrorInjectionPorts) {
        auto& configs                    = get_error_injection_configs();
        configs.at(index).enabled        = true;
        configs.at(index).error_type     = static_cast<uint8_t>(error_type);
        configs.at(index).target_address = address;
        configs.at(index).protocol_type  = static_cast<uint8_t>(protocol_type);

        nv::info(
            "I2C Error Injection: Enabled error injection on port=%d, type=%d, addr=0x%x, "
            "proto=0x%x\n",
            static_cast<uint8_t>(port),
            static_cast<uint8_t>(error_type),
            address,
            static_cast<uint8_t>(protocol_type));
    }
    else {
        nv::info("I2C Error Injection: Invalid port=%d, max supported ports=%d\n",
                 static_cast<uint8_t>(port),
                 MaxErrorInjectionPorts);
    }
}

bool should_inject_error(Port    port,
                         uint8_t address,
                         uint8_t operation_type,
                         uint8_t protocol_type)
{
    (void)operation_type;  // Reserved for future use
    auto        index   = port_to_error_injection_index(port, protocol_type);
    const auto& configs = get_error_injection_configs();

    if (index >= MaxErrorInjectionPorts || !configs.at(index).enabled) {
        return false;
    }

    // Check if the protocol type matches the configured protocol type
    if (configs.at(index).protocol_type != protocol_type) {
        return false;
    }

    // Check if address filtering is applied
    if (configs.at(index).target_address != 0x0) {
        return (address == configs.at(index).target_address);
    }

    return true;
}

bool should_inject_error(ipchandler::Id ipchandler_id,
                         uint8_t        address,
                         uint8_t        operation_type,
                         uint8_t        protocol_type)
{
    if (ipchandler_id == ipchandler::Id::Iox) {
        if constexpr (nv::iox::Task::IoxNum > 0) {
            // Check if address is in valid IOX range
            constexpr auto base_addr = nv::iox::Task::IoxBaseAddr;
            constexpr auto max_addr  = base_addr + nv::iox::Task::IoxNum;

            // Split the range check to avoid linter false positive
            if (address < base_addr) {
                return false;
            }
            if (address < max_addr) {
                const Port port = static_cast<Port>(address - base_addr);
                return should_inject_error(port, address, operation_type, protocol_type);
            }
        }
        return false;
    }

    // Convert ipchandler::Id to Port for I2C handlers
    if (ipchandler_id < ipchandler::Id::I2c0 || ipchandler_id >= ipchandler::Id::I3cStart) {
        return false;
    }

    const Port port = ipchandler_to_error_injection_port(ipchandler_id);
    return should_inject_error(port, address, operation_type, protocol_type);
}

I2cStatus get_injected_error_status(Port port, uint8_t protocol_type)
{
    auto        index   = port_to_error_injection_index(port, protocol_type);
    const auto& configs = get_error_injection_configs();
    if (index >= MaxErrorInjectionPorts || !configs.at(index).enabled) {
        return I2cStatus::Ok;
    }

    switch (static_cast<ErrorInjectionType>(configs.at(index).error_type)) {
        case ErrorInjectionType::Clear:
            clear_error_injection(port, protocol_type);
            return I2cStatus::Ok;

        case ErrorInjectionType::Nack: return I2cStatus::Nak;

        case ErrorInjectionType::Timeout: return I2cStatus::Timeout;

        case ErrorInjectionType::UsbQueueFull: return I2cStatus::Busy;

        case ErrorInjectionType::QueueFull:
        default                           : return I2cStatus::Error;
    }
}

void clear_error_injection(Port port, uint8_t protocol_type)
{
    auto index = port_to_error_injection_index(port, protocol_type);

    if (index < MaxErrorInjectionPorts) {
        auto& configs                    = get_error_injection_configs();
        configs.at(index).enabled        = false;
        configs.at(index).error_type     = static_cast<uint8_t>(ErrorInjectionType::Clear);
        configs.at(index).target_address = 0x0;
    }
    else {
        nv::info("I2C Error Injection: Invalid port=%d for clear, max supported ports=%zu\n",
                 static_cast<uint8_t>(port),
                 MaxErrorInjectionPorts);
    }
}

}  // namespace nv::i2c
