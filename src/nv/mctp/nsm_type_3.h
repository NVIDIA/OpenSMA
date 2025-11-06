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
        static_cast<uint8_t>(NsmPlatEnvCmdCode::SetThermalParameter),
        static_cast<uint8_t>(NsmPlatEnvCmdCode::GetThermalParameter),
        static_cast<uint8_t>(NsmPlatEnvCmdCode::GetInventoryInformation)};
#ifdef ENABLE_ADC_LEAK_DETECTION
    static_cast<uint8_t>(NsmPlatEnvCmdCode::GetLeakDetectionInfo);
    static_cast<uint8_t>(NsmPlatEnvCmdCode::SetLeakDetectionThresholds);
#endif

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

enum LeakDetectionSensorThresholds : uint8_t
{
    NsmPlatEnvThresholdMinLeak   = 0,
    NsmPlatEnvThresholdMaxLeak   = 1,
    NsmPlatEnvThresholdMaxNormal = 2,
    NsmPlatEnvThresholdSize
};

struct [[gnu::packed]] SetLeakSensorThresholdsRequest
{
    uint8_t  sensor_id;
    uint8_t  leak;
    uint16_t thresholds[NsmPlatEnvThresholdSize];
};

/**
 * @brief The SetLeakSensorThresholdsResponse inherits SetLeakSensorThresholdsRequest adding
 * adc_reading
 */
struct [[gnu::packed]] SetLeakSensorThresholdsResponse : SetLeakSensorThresholdsRequest
{
    uint16_t adc_reading;
};

struct [[gnu::packed]] LeakDetectionHeader
{
    uint8_t n_sensors;
    uint8_t n_thresholds;
};
struct [[gnu::packed]] NsmPlatEnvLeakDetectionRequest
{
    LeakDetectionHeader            header;
    SetLeakSensorThresholdsRequest sensors[NsmPlatEnvLeakSensorsSize];
};

struct [[gnu::packed]] NsmPlatEnvLeakDetectionResponse
{
    LeakDetectionHeader header;

    SetLeakSensorThresholdsResponse sensors[NsmPlatEnvLeakSensorsSize];
};

struct [[gnu::packed]] NsmT3SetThermalParameterReq
{
    uint8_t sensorId;
    uint8_t threshold;
};

struct [[gnu::packed]] NsmT3GetThermalParameterReq
{
    uint8_t sensorId;
};

struct [[gnu::packed]] NsmT3GetThermalParameterRes
{
    uint32_t threshold;
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

/**
 * @brief Find temperature sensor index in I2cTempSensorList by sensor ID
 * @param sensorId The sensor ID to search for
 * @return Index of the sensor in I2cTempSensorList, or Type3TemperatureSensors::TempSensor_End
 * if not found
 */
uint8_t findTemperatureSensorIndex(uint8_t sensorId);

}  // namespace nsm_type3

}  // namespace nv::mctp
