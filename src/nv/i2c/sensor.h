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
#include <span>
#include <stdint.h>
#include "nv/i2c/common.h"
#include "nv/i2c/port.h"
#include "nv/mctp/enums.h"
#include "nv/telemetry/cache.h"

namespace nv::i2c {

enum ManufacturerId : uint8_t
{
    TI        = 0x55,  // TMP461, 451
    Microchip = 0x54,  // EMC1812
};

enum SensorModel : uint8_t
{
    Sensor_Tmp461  = 1,
    Sensor_Emc1812 = 2,
    Sensor_Tmp1075 = 3,
};

// I2C Temperature Sensor configuration structure
// Using [[gnu::packed]] to ensure no padding bytes are added by the compiler
struct [[gnu::packed]] I2cTempSensorConfig
{
    nv::i2c::Port                     port;
    uint8_t                           identified_addr;
    uint8_t                           sensor_model;
    nv::mctp::Type3TemperatureSensors sensor_id;
    std::array<uint8_t, 2> addresses;  // primary/secondary address, can be extended if with
                                       // more vendors.
};

struct [[gnu::packed]] I2cTempSensorThresholdsConfig
{
    nv::i2c::Port                     port;
    uint8_t                           identified_addr;
    uint8_t                           sensor_model;
    int8_t                            local_warning_threshold;
    int8_t                            local_shutdown_threshold;
    int8_t                            remote_warning_threshold;
    int8_t                            remote_shutdown_threshold;
    nv::mctp::Type3TemperatureSensors sensor_id;
    std::array<uint8_t, 2> addresses;  // primary/secondary address, can be extended if with
                                       // more vendors.
};

class TempSensor
{
public:
    enum Register
    {
        RemoteTempLimit = 0x19,
        LocalTempLimit  = 0x20,
        ManufacturerId  = 0xFE,
    };

    enum class Status : uint8_t
    {
        SetRegister,
        LocalLimit,
        RemoteLimit,
    };

    TempSensor(Port                   port,
               uint8_t                address,
               nv::telemetry::TelemId item = nv::telemetry::TelemId::MaxItem);
    I2cStatus get_manufacturer_id(uint8_t& id);
    I2cStatus get_device_id(uint8_t& id);
    I2cStatus get_temp(uint8_t& temp);
    I2cStatus get_CX8_temp(uint8_t& temp);
    void      update_cache();
    I2cStatus read_reg(uint8_t offset, uint8_t& value);
    I2cStatus write_reg(uint8_t offset, uint8_t value);
    I2cStatus read_reg_16bits(uint8_t offset, uint16_t& value);
    I2cStatus write_reg_16bits(uint8_t offset, uint16_t value);

private:
    Port                   _port;
    uint8_t                _address;
    nv::telemetry::TelemId _item;
};

}  // namespace nv::i2c
