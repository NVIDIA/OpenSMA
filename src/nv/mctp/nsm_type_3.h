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

#include "nv/mctp/constants.h"
#include "nv/mctp/enums.h"
#include "nv/telemetry/cache.h"

namespace nv::mctp {

constexpr uint8_t NsmPlatEnvTempAggregate = 0xFF;

enum class NsmPlatEnvPropertyId : uint8_t
{
    SkuId = 3,
    Uuid  = 10,
};

struct [[gnu::packed]] GetInventoryInformationReq
{
    NsmPlatEnvPropertyId property_id;
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

struct [[gnu::packed]] NsmT3BusBarInfoTemperature
{
    uint32_t temperature;
    uint32_t temperature_threshold;
    NsmT3BusBarInfoTemperature() : temperature{0}, temperature_threshold{0}
    {
        // Empty
    }
    void set_temperatures_NvS8_4();
};

namespace nsm_type3 {

using TelemetryValue                     = nv::telemetry::Cache::Value;
static constexpr auto TelemetryInvalid   = nv::telemetry::Cache::InvalidItem;
static constexpr auto TemperatureInvalid = static_cast<int32_t>(TelemetryInvalid);

/**
 * @returns the sensor temperature of the @a sensorId or TemperatureInvalid in case any error
 */
int32_t getTemperatureTelemetry(const uint8_t sensorId);

/**
 * @brief Gets Power Draw telemetry
 * @param sensorId - the sensor id
 */
TelemetryValue getPowerTelemetry(const uint8_t sensorId);

/**
 * @brief Gets Voltage telemetry
 * @param sensorId - the sensor id
 */
TelemetryValue getVoltageTelemetry(const uint8_t sensorId);

/**
 * @brief Find temperature sensor index in I2cTempSensorList by sensor ID
 * @param sensorId The sensor ID to search for
 * @return Index of the sensor in I2cTempSensorList, or Type3TemperatureSensors::TempSensor_End
 * if not found
 */
uint8_t findTemperatureSensorIndex(uint8_t sensorId);

}  // namespace nsm_type3

}  // namespace nv::mctp
