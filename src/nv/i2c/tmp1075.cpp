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
#include "nv/i2c/tmp1075.h"
#include "nv/nv.h"

// TMP1075 is similar to NCT75 Register Map.

using namespace nv::i2c;

Tmp1075::Tmp1075(Port port, uint8_t address, nv::telemetry::TelemId item)
: TempSensor(port, address, item)
{}

I2cStatus Tmp1075::read_temperature(int8_t& temp_celsius)
{
    uint16_t temp_raw = 0;

    // Read 16-bit temperature register using dedicated API
    auto status = read_reg_16bits(Register::Temperature, temp_raw);

    if (status == I2cStatus::Ok) {
        // Calculate the integer celsius value from the 12-bit temperature code
        // coverity[cert_int31_c_violation]
        temp_celsius = static_cast<int8_t>(temp_raw >> 8);
    }

    return status;
}

int16_t Tmp1075::convert_12bit_to_signed(int16_t raw_value)
{
    // Limit/temperature data is left-justified in bits [15:4] with 0.0625 degC
    // resolution, so the integer Celsius value lives in the upper byte (bits
    // [15:8]). Arithmetic right shift by 8 sign-extends and yields degrees C.
    return static_cast<int16_t>(raw_value >> 8);
}

int16_t Tmp1075::convert_signed_to_12bit(int8_t temp_celsius)
{
    // Encode integer Celsius into the register's left-justified format: the
    // 12-bit code is temp * 16 (1 LSB = 0.0625 degC) placed in bits [15:4],
    // which is exactly temp << 8. Per datasheet, e.g. +80 degC -> 0x5000.
    // coverity[cert_int31_c_violation]
    return static_cast<int16_t>(temp_celsius << 8);
}

I2cStatus Tmp1075::set_low_limit(int8_t threshold)
{
    const int16_t threshold_raw = convert_signed_to_12bit(threshold);

    // Write 16-bit low limit register using dedicated API
    // coverity[cert_int31_c_violation]
    return write_reg_16bits(Register::LowLimit, static_cast<uint16_t>(threshold_raw));
}

I2cStatus Tmp1075::get_low_limit(int8_t& threshold)
{
    uint16_t threshold_raw_u16 = 0;

    // Read 16-bit low limit register using dedicated API
    auto status = read_reg_16bits(Register::LowLimit, threshold_raw_u16);
    if (status == I2cStatus::Ok) {
        // Convert to int16_t for arithmetic right shift with sign extension
        // coverity[cert_int31_c_violation]
        const int16_t temp_12bit = convert_12bit_to_signed(
            static_cast<int16_t>(threshold_raw_u16));
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(temp_12bit);
    }

    return status;
}

I2cStatus Tmp1075::set_high_limit(int8_t threshold)
{
    const int16_t threshold_raw = convert_signed_to_12bit(threshold);

    // Write 16-bit high limit register using dedicated API
    // coverity[cert_int31_c_violation]
    return write_reg_16bits(Register::HighLimit, static_cast<uint16_t>(threshold_raw));
}

I2cStatus Tmp1075::get_high_limit(int8_t& threshold)
{
    uint16_t threshold_raw_u16 = 0;

    // Read 16-bit high limit register using dedicated API
    auto status = read_reg_16bits(Register::HighLimit, threshold_raw_u16);
    if (status == I2cStatus::Ok) {
        // Convert to int16_t for arithmetic right shift with sign extension
        // coverity[cert_int31_c_violation]
        const int16_t temp_12bit = convert_12bit_to_signed(
            static_cast<int16_t>(threshold_raw_u16));
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(temp_12bit);
    }

    return status;
}

I2cStatus Tmp1075::get_device_id(uint16_t& device_id)
{
    // Read 16-bit device ID register using dedicated API
    // Default value is 0x7500, with MSB 0x75 indicating TMP1075
    return read_reg_16bits(Register::DeviceId, device_id);
}
