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

#include "nv/telemetry/smb_telemetry.h"

#include "nv/gpio/driver.h"
#include "nv/mctp/nsm_type_3.h"
#include "nv/telemetry/cache.h"
#include NV_IPC_CONFIG_H
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

using namespace nv;

namespace nv::smb_telemetry {

// overriden by product/telemetry.cpp
uint8_t __attribute__((weak)) get_end_address()
{
    return SmbDirect::StartAddress;
}

// overriden by product/telemetry.cpp
bool __attribute__((weak))
get_smbtelemetry_table(std::span<const SmbDirectSensorMapping>& smbtelemetry_table)
{
    (void)smbtelemetry_table;
    return false;
}

// overriden by product/telemetry.cpp
void __attribute__((weak)) refresh_product_specific_telemetries(
    const SmbDirectSensorMapping& entry, std::span<uint8_t> cache, uint8_t buffer_pos)
{
    (void)entry;
    (void)cache;
    (void)buffer_pos;
    return;
}

void SmbDirect::refresh_cache(std::span<uint8_t> cache)
{
    using namespace nv::smb_telemetry;
    using namespace nv::i2c;

    const uint8_t end_address = get_end_address();
    if (end_address == SmbDirect::StartAddress) {
        return;
    }

    if (cache.size() < (end_address - SmbDirect::StartAddress)) {
        return;
    }

    const uint8_t StartAddress = SmbDirect::StartAddress;

    std::span<const SmbDirectSensorMapping> smbtelemetry_table{};
    if (!get_smbtelemetry_table(smbtelemetry_table)) {
        return;
    }
    for (const auto& entry : smbtelemetry_table) {
        const auto    entry_addr = static_cast<uint8_t>(entry.address);
        const uint8_t buffer_pos = entry_addr - StartAddress;

        if (buffer_pos >= (end_address - SmbDirect::StartAddress)
            || (buffer_pos + entry.size) > (end_address - SmbDirect::StartAddress)) {
            continue;
        }

        switch (entry.type) {
            case TelemetryType::Temperature: {
                int32_t temperature = nv::mctp::nsm_type3::getTemperatureTelemetry(
                    entry.sensor_id);
                temperature     = temperature >> 8;
                auto temp_bytes = std::bit_cast<std::array<uint8_t, sizeof(int32_t)>>(
                    temperature);
                std::memcpy(&cache[buffer_pos], temp_bytes.data(), entry.size);
                break;
            }
            case TelemetryType::Power: {
                const uint32_t power = nv::mctp::nsm_type3::getPowerTelemetry(entry.sensor_id);
                auto power_bytes = std::bit_cast<std::array<uint8_t, sizeof(uint32_t)>>(power);
                std::memcpy(&cache[buffer_pos], power_bytes.data(), entry.size);
                break;
            }
            case TelemetryType::Voltage: {
                const uint32_t voltage = nv::mctp::nsm_type3::getVoltageTelemetry(
                    entry.sensor_id);
                auto voltage_bytes = std::bit_cast<std::array<uint8_t, sizeof(uint32_t)>>(
                    voltage);
                std::memcpy(&cache[buffer_pos], voltage_bytes.data(), entry.size);
                break;
            }
            default:
                // Product-specific handling for non-common telemetry types.
                refresh_product_specific_telemetries(entry, cache, buffer_pos);
                break;
        }
    }
}

std::span<const uint8_t> SmbDirect::get_cache()
{
    return {cache_.data(), cache_.size()};
}

}  // namespace nv::smb_telemetry
