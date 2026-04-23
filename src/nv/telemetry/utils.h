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
#include <utility>

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
    QM4_1_Temp,
    QM4_2_Temp,
    MaxItem
};

// This table is only for GB products telemetries: temperature and power telemetries
static constexpr std::array<std::pair<nv::mctp::Type3TemperatureSensors, TelemId>, 10>
    TempSensorIdToTelemIdMapping = {
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
            {nv::mctp::Type3TemperatureSensors::QM4_1_Temp, TelemId::QM4_1_Temp},
         {nv::mctp::Type3TemperatureSensors::QM4_2_Temp, TelemId::QM4_2_Temp},
         }
};

static constexpr std::array<std::pair<nv::mctp::Type3PowerSensors, TelemId>, 3>
    PowerSensorIdToTelemIdPowerMapping = {
        {
         // Legacy Power Sensor enum for GB products
            {nv::mctp::Type3PowerSensors::PowerGpu1, TelemId::Gpu1Power},      // PowerGpu1
            {nv::mctp::Type3PowerSensors::PowerGpu2, TelemId::Gpu2Power},      // PowerGpu2
            {nv::mctp::Type3PowerSensors::PowerModule, TelemId::ModulePower},  // PowerModule
        }
};

/**
 * @brief Get TelemId from Type3TemperatureSensors
 * @param type3Sensor The Type3TemperatureSensors value
 * @return Corresponding TelemId or TelemId::MaxItem if not found
 */
constexpr TelemId getTelemIdFromTempSensorId(uint8_t type3Sensor)
{
    for (const auto& mapping : TempSensorIdToTelemIdMapping) {
        if (mapping.first == type3Sensor) {
            return mapping.second;
        }
    }
    return TelemId::MaxItem;
}

/**
 * @brief Get TelemId from Type3PowerSensors
 * @param type3PowerSensor The Type3PowerSensors value
 * @return Corresponding TelemId or TelemId::MaxItem if not found
 */
constexpr TelemId getTelemIdFromPowerSensorId(uint8_t type3PowerSensor)
{
    for (const auto& mapping : PowerSensorIdToTelemIdPowerMapping) {
        if (mapping.first == type3PowerSensor) {
            return mapping.second;
        }
    }
    return TelemId::MaxItem;
}

uint32_t buffer_to_uint32(std::span<uint8_t> buffer);

}  // namespace nv::telemetry
