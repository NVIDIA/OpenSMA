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
#include "nv/i2c/tmp432.h"

using namespace nv::i2c;

Tmp432::Tmp432(Port port, uint8_t address, nv::telemetry::TelemId item)
: TempSensor(port, address, item)
{}

I2cStatus Tmp432::get_device_id(uint8_t& device_id)
{
    return read_reg(Register::DeviceId, device_id);
}

I2cStatus Tmp432::get_local_high_temp(int8_t& temp_integer)
{
    uint8_t temp_high_byte = 0;
    auto    status         = read_reg(Register::LocalTempHigh, temp_high_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        temp_integer = static_cast<int8_t>(temp_high_byte);
    }
    return status;
}

I2cStatus Tmp432::get_remote_high_temp(int8_t& temp_integer)
{
    uint8_t temp_high_byte = 0;
    auto    status         = read_reg(Register::Remote1TempHigh, temp_high_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        temp_integer = static_cast<int8_t>(temp_high_byte);
    }
    return status;
}

I2cStatus Tmp432::get_remote2_high_temp(int8_t& temp_integer)
{
    uint8_t temp_high_byte = 0;
    auto    status         = read_reg(Register::Remote2TempHigh, temp_high_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        temp_integer = static_cast<int8_t>(temp_high_byte);
    }
    return status;
}

I2cStatus Tmp432::set_local_high_alert_thresholds(int8_t threshold)
{
    // coverity[cert_int31_c_violation]
    return write_reg(Register::LocalTempHighLimitWrite, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp432::get_local_high_alert_thresholds(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::LocalTempHighLimitRead, threshold_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp432::set_remote_high_alert_thresholds(int8_t threshold)
{
    // coverity[cert_int31_c_violation]
    return write_reg(Register::Remote1TempHighLimitWrite, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp432::get_remote_high_alert_thresholds(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::Remote1TempHighLimitRead, threshold_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp432::set_remote2_high_alert_thresholds(int8_t threshold)
{
    // coverity[cert_int31_c_violation]
    return write_reg(Register::Remote2TempHighLimit, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp432::get_remote2_high_alert_thresholds(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::Remote2TempHighLimit, threshold_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp432::set_remote_therm_limit(int8_t threshold)
{
    // coverity[cert_int31_c_violation]
    return write_reg(Register::Remote1ThermLimit, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp432::get_remote_therm_limit(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::Remote1ThermLimit, threshold_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp432::set_remote2_therm_limit(int8_t threshold)
{
    // coverity[cert_int31_c_violation]
    return write_reg(Register::Remote2ThermLimit, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp432::get_remote2_therm_limit(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::Remote2ThermLimit, threshold_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp432::set_local_therm_limit(int8_t threshold)
{
    // coverity[cert_int31_c_violation]
    return write_reg(Register::LocalThermLimit, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp432::get_local_therm_limit(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::LocalThermLimit, threshold_byte);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp432::set_configuration(uint8_t configuration)
{
    return write_reg(Register::ConfigurationWrite, configuration);
}

I2cStatus Tmp432::get_configuration(uint8_t& configuration)
{
    return read_reg(Register::ConfigurationRead, configuration);
}

I2cStatus Tmp432::set_configuration2(uint8_t configuration)
{
    return write_reg(Register::Configuration2Read, configuration);
}

I2cStatus Tmp432::get_configuration2(uint8_t& configuration)
{
    return read_reg(Register::Configuration2Read, configuration);
}
