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
#include "nv/i2c/tmp461.h"
#include "nv/nv.h"

using namespace nv::i2c;

Tmp461::Tmp461(Port port, uint8_t address, nv::telemetry::TelemId item)
: TempSensor(port, address, item)
{}

I2cStatus Tmp461::get_local_high_temp(int8_t& temp_integer)
{
    uint8_t temp_high_byte = 0;

    // Read only high byte register (integer part)
    auto status = read_reg(Register::LocalTempHigh, temp_high_byte);
    if (status == I2cStatus::Ok) {
        // Convert high byte directly to signed integer temperature
        // coverity[cert_int31_c_violation]
        temp_integer = static_cast<int8_t>(temp_high_byte);
    }
    return status;
}

I2cStatus Tmp461::get_remote_high_temp(int8_t& temp_integer)
{
    uint8_t temp_high_byte = 0;

    // Read only high byte register (integer part)
    auto status = read_reg(Register::RemoteTempHigh, temp_high_byte);
    if (status == I2cStatus::Ok) {
        // Convert high byte directly to signed integer temperature
        // coverity[cert_int31_c_violation]
        temp_integer = static_cast<int8_t>(temp_high_byte);
    }
    return status;
}

// The published NSM Spec of Set Thermal Threshold defines NvU8 for threshold to be set.
// So here just to align with that.
I2cStatus Tmp461::set_local_high_alert_thresholds(int8_t threshold)
{
    // Write signed threshold directly to write register (0x0B)
    // Note: TMP461 has different addresses for read (0x05) and write (0x0B)
    // coverity[cert_int31_c_violation]
    return write_reg(Register::LocalTempHighLimitWrite, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp461::get_local_high_alert_thresholds(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::LocalTempHighLimitRead, threshold_byte);
    if (status == I2cStatus::Ok) {
        // Convert back to signed int8_t
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp461::set_remote_high_alert_thresholds(int8_t threshold)
{
    // Write signed threshold directly to write register (0x0D)
    // Note: TMP461 has different addresses for read (0x07) and write (0x0D)
    // coverity[cert_int31_c_violation]
    return write_reg(Register::RemoteTempHighLimitWrite, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp461::get_remote_high_alert_thresholds(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::RemoteTempHighLimitRead, threshold_byte);
    if (status == I2cStatus::Ok) {
        // Convert back to signed int8_t
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp461::set_remote_therm_limit(int8_t threshold)
{
    // Write signed threshold directly to remote THERM limit register
    // coverity[cert_int31_c_violation]
    return write_reg(Register::RemoteThermLimit, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp461::get_remote_therm_limit(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::RemoteThermLimit, threshold_byte);
    if (status == I2cStatus::Ok) {
        // Convert back to signed int8_t
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp461::set_local_therm_limit(int8_t threshold)
{
    // Write signed threshold directly to local THERM limit register
    // coverity[cert_int31_c_violation]
    return write_reg(Register::LocalThermLimit, static_cast<uint8_t>(threshold));
}

I2cStatus Tmp461::get_local_therm_limit(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::LocalThermLimit, threshold_byte);
    if (status == I2cStatus::Ok) {
        // Convert back to signed int8_t
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

I2cStatus Tmp461::set_configuration(uint8_t configuration)
{
    return write_reg(Register::configurationWrite, configuration);
}

I2cStatus Tmp461::get_configuration(uint8_t& configuration)
{
    return read_reg(Register::configurationRead, configuration);
}