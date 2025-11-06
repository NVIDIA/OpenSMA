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
#include <stdint.h>
#include <array>
#include <span>

// Include the enum definition
#include "nv/mctp/enums.h"

namespace nv::telemetry {

enum TelemId : uint8_t
{
    Gpu1Temp,
    Gpu2Temp,
    Gpu1Power,
    Gpu2Power,
    ModulePower,
    ModuleTemp1,
    ModuleTemp2,
    InternalTemp,
    MaxModuleTemp,
    Gpio,
    CX8_1_Temp,
    CX8_2_Temp,
    MaxItem
};

/**
 * @brief Mapping from Type3TemperatureSensors to TelemId
 *        Provides one-to-one mapping for temperature sensors
 *        Uses compact structure with only valid mappings
 */
static constexpr std::array<std::pair<nv::mctp::Type3TemperatureSensors, TelemId>, 22>
    Type3ToTelemIdMapping = {
        {
         // Legacy Sensor ID for GB products
            {nv::mctp::Type3TemperatureSensors::TempGpu1, TelemId::Gpu1Temp},  // TempGpu1
            {nv::mctp::Type3TemperatureSensors::TempGpu2, TelemId::Gpu2Temp},  // TempGpu2
            {nv::mctp::Type3TemperatureSensors::TempTMP451_1,
             TelemId::ModuleTemp1},  // TempTMP451_1
            {nv::mctp::Type3TemperatureSensors::TempTMP451_2,
             TelemId::ModuleTemp2},  // TempTMP451_2
            {nv::mctp::Type3TemperatureSensors::TempMaxModule,
             TelemId::MaxModuleTemp},  // TempMaxModule
            {nv::mctp::Type3TemperatureSensors::TempSMAInternal,
             TelemId::InternalTemp},  // TempSMAInternal
            {nv::mctp::Type3TemperatureSensors::TempCX8_1, TelemId::CX8_1_Temp},  // TempCX8_1
            {nv::mctp::Type3TemperatureSensors::TempCX8_2, TelemId::CX8_2_Temp},  // TempCX8_2
        }
};

/**
 * @brief Get TelemId from Type3TemperatureSensors
 * @param type3Sensor The Type3TemperatureSensors value
 * @return Corresponding TelemId or TelemId::MaxItem if not found
 */
constexpr TelemId getTelemIdFromType3(uint8_t type3Sensor)
{
    for (const auto& mapping : Type3ToTelemIdMapping) {
        if (mapping.first == type3Sensor) {
            return mapping.second;
        }
    }
    return TelemId::MaxItem;
}

/**
 * @brief Check if Type3TemperatureSensors has a valid mapping to TelemId
 * @param type3Sensor The Type3TemperatureSensors value
 * @return true if mapping exists, false otherwise
 */
constexpr bool hasValidTelemIdMapping(nv::mctp::Type3TemperatureSensors type3Sensor)
{
    return getTelemIdFromType3(type3Sensor) != TelemId::MaxItem;
}

uint32_t buffer_to_uint32(std::span<uint8_t> buffer);

}  // namespace nv::telemetry
