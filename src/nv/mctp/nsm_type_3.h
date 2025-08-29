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

#include <array>
#include <climits>
#include <cstdint>

#include "nv/mctp/enums.h"
#include "nv/telemetry/cache.h"

namespace nv::mctp {

constexpr uint8_t NsmPlatEnvCmdNum        = 16;
constexpr uint8_t NsmPlatEnvTempAggregate = 0xFF;

constexpr std::array<uint8_t, NsmPlatEnvCmdNum> gen_type3_code_bitmask()
{
    std::array<uint8_t, NsmPlatEnvCmdNum> bitmask = {0};

    constexpr uint8_t bit_positions[] = {
        static_cast<uint8_t>(NsmPlatEnvCmdCode::GetTemperatureReading),
        static_cast<uint8_t>(NsmPlatEnvCmdCode::GetInventoryInformation)};

    for (uint8_t pos : bit_positions) {
        const size_t byte_index = pos / 8;
        const size_t bit_offset = pos % 8;
        const size_t value      = (1u << bit_offset);
        if (value <= std::numeric_limits<uint8_t>::max()) {
            bitmask.at(byte_index) |= static_cast<uint8_t>(value);
        }
    }

    /**
     * So far Power Draw sensors exist only for MCU projects pg540e00 and pg540
     */
    if constexpr (mcuPowerSensorsSize > 0) {
        uint8_t      pos        = static_cast<uint8_t>(NsmPlatEnvCmdCode::GetCurrentPowerDraw);
        const size_t byte_index = pos / 8;
        const size_t bit_offset = pos % 8;
        const size_t value      = (1u << bit_offset);
        if (value <= std::numeric_limits<uint8_t>::max()) {
            bitmask.at(byte_index) |= static_cast<uint8_t>(value);
        }
    }

    return bitmask;
}

enum class NsmPlatEnvPropertyId : uint8_t
{
    SkuId = 3,
    Uuid  = 10,
};

struct [[gnu::packed]] GetInventoryInformationReq
{
    NsmPlatEnvPropertyId property_id;
};

/** single data structure for type persistent data */
struct [[gnu::packed]] NsmPlatformEnviromentalsPersistentData
{
    static constexpr std::array<uint8_t, NsmPlatEnvCmdNum>
        suppCmdCode = gen_type3_code_bitmask();
};

namespace nsm_type3 {

/**
 * @brief Gets Temperature telemetry
 *
 * Ccode::ErrorInvalidData
 * @param tagId  - The tag/sensor id to get Temperature
 * @param [out] telemetry
 * @return Ccode::Success or any error
 */
Ccode getTemperatureTelemetry(const uint8_t tagId, int32_t& telemetry);

/**
 * @brief Gets Power Draw telemetry
 * @param tagId
 * @param [out] telemetry
 * @return Ccode::Success or any error
 */
Ccode getPowerDrawTelemetry(const uint8_t tagId, uint32_t& telemetry);

}  // namespace nsm_type3

}  // namespace nv::mctp
