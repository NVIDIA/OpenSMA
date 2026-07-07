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
#include "nv/i2c/sensor.h"
#include <stdint.h>

// Register map per TMP432 datasheet (SBOS441), Table 4.
namespace nv::i2c {

/**
 * TMP432 Simplified Driver
 * Three-channel (local, remote1, remote2) temperature sensor with alert/THERM limits.
 */
class Tmp432 : public TempSensor
{
public:
    static constexpr uint8_t ExpectedDeviceId = 0x32;

    enum Register : uint8_t
    {
        LocalTempHigh             = 0x00,
        Remote1TempHigh           = 0x01,
        Status                    = 0x02,
        ConfigurationRead         = 0x03,
        LocalTempHighLimitRead    = 0x05,
        Remote1TempHighLimitRead  = 0x07,
        ConfigurationWrite        = 0x09,
        LocalTempHighLimitWrite   = 0x0B,
        Remote1TempHighLimitWrite = 0x0D,
        Remote2TempHighLimit      = 0x15,
        Remote1ThermLimit         = 0x19,
        Remote2ThermLimit         = 0x1A,
        LocalThermLimit           = 0x20,
        Remote2TempHigh           = 0x23,
        Configuration2Read        = 0x3F,
        DeviceId                  = 0xFD,
    };

    enum Configuration : uint8_t
    {
        THERM2_Enabled = 1u << 5,  // AL/TH bit: ALERT/THERM2 pin as THERM2
    };

    enum Configuration2 : uint8_t
    {
        Remote2Enabled = 1u << 5,  // REN2 bit: enable remote channel 2
    };

    Tmp432(Port                   port,
           uint8_t                address,
           nv::telemetry::TelemId item = nv::telemetry::TelemId::MaxItem);

    I2cStatus get_device_id(uint8_t& device_id);

    I2cStatus get_local_high_temp(int8_t& temp_integer);
    I2cStatus get_remote_high_temp(int8_t& temp_integer);
    I2cStatus get_remote2_high_temp(int8_t& temp_integer);

    I2cStatus set_local_high_alert_thresholds(int8_t threshold);
    I2cStatus get_local_high_alert_thresholds(int8_t& threshold);

    I2cStatus set_remote_high_alert_thresholds(int8_t threshold);
    I2cStatus get_remote_high_alert_thresholds(int8_t& threshold);

    I2cStatus set_remote2_high_alert_thresholds(int8_t threshold);
    I2cStatus get_remote2_high_alert_thresholds(int8_t& threshold);

    I2cStatus set_remote_therm_limit(int8_t threshold);
    I2cStatus get_remote_therm_limit(int8_t& threshold);

    I2cStatus set_remote2_therm_limit(int8_t threshold);
    I2cStatus get_remote2_therm_limit(int8_t& threshold);

    I2cStatus set_local_therm_limit(int8_t threshold);
    I2cStatus get_local_therm_limit(int8_t& threshold);

    I2cStatus set_configuration(uint8_t configuration);
    I2cStatus get_configuration(uint8_t& configuration);

    I2cStatus set_configuration2(uint8_t configuration);
    I2cStatus get_configuration2(uint8_t& configuration);
};

}  // namespace nv::i2c
