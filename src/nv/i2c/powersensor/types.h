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
#include "nv/i2c/port.h"
#include "nv/mctp/enums.h"

namespace nv::i2c::power {

enum MfrId : uint32_t
{
    TI            = 0x00004954,
    MPS           = 0x004D5053,
    INFINEON      = 0x00004649,
    RNS           = 0x0052384B,
    RNS_RAA228000 = 0x00523847,
    RNS_RAA228006 = 0x00523841,
};

enum MfrModel : uint64_t
{
    LM5066I   = 0x0049363630354D4CULL,
    MP5926    = 0x4D50353932360000ULL,
    XPD712021 = 0x0000323137504458ULL,
    MP29540   = 0x00004D3339353032ULL,
    RAA228004 = 0x0000000000000558ULL,
    RAA228000 = 0x0000000000007612ULL,
    RAA228006 = 0x0000000000000550ULL,
    EMPTY     = 0x0000000000000000ULL,
};

// MP5926 MFR_MODEL mask - last 2 bytes are Reserved, ignore them during comparison
constexpr uint64_t MP5926_MODEL_MASK = 0xFFFFFFFFFFFF0000ULL;

enum class DeviceType : uint8_t
{
    Unknown,
    LM5066I,
    MP5926,
    XPD712021,
    RAA22800X,
    MP29540
};

// Configuration for sensor list in config.h (used for device identification)
struct PowerSensorListConfig
{
    uint8_t                           address;      // I2C Address (single address per device)
    uint32_t                          mfr_id;       // Expected MFR_ID for verification
    uint64_t                          mfr_model;    // Expected MFR_MODEL for verification
    DeviceType                        device_type;  // Device Type
    nv::mctp::Type3TemperatureSensors temp_sensor_id;   // NSM Type 3 Temperature sensor ID
    nv::mctp::Type3PowerSensors       power_sensor_id;  // NSM Type 3 Power sensor ID
    nv::mctp::T3Voltage               vout_sensor_id;   // NSM Type 3 Voltage output sensor ID
    nv::mctp::T3Voltage               vin_sensor_id;    // NSM Type 3 Voltage input sensor ID
    nv::mctp::PowerSensorFaults       alert_sensor_id;  // NSM Type 3 Alert sensor ID
};

}  // namespace nv::i2c::power