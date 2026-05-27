/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/i2c/nct75.h"
#include "nv/nv.h"

// NCT75 register map is a strict subset of TMP1075's (no DeviceId at 0x0F):
//   0x00 Temperature (R, 16-bit two's-complement, 12-bit left-justified)
//   0x01 Configuration (R/W, 8-bit)
//   0x02 THYST       (R/W, 16-bit, default 0x4B00 = +75 degC)
//   0x03 TOS         (R/W, 16-bit, default 0x5000 = +80 degC)
//   0x04 One-shot    (W,   any value triggers a single conversion)

using namespace nv::i2c;

Nct75::Nct75(Port port, uint8_t address, nv::telemetry::TelemId item)
: TempSensor(port, address, item)
{}

int16_t Nct75::convert_12bit_to_signed(int16_t raw_value)
{
    // Arithmetic right shift on int16_t sign-extends the MSB, yielding a
    // signed 12-bit value in units of 1/16 degC.
    return static_cast<int16_t>(raw_value >> 4);
}

int16_t Nct75::convert_signed_to_12bit(int8_t temp_celsius)
{
    // Place the integer Celsius value in the upper byte; lower 4 bits stay
    // zero. Result has the same byte order the device expects when written
    // back via write_reg_16bits().
    // coverity[cert_int31_c_violation]
    return static_cast<int16_t>(temp_celsius << 8);
}

I2cStatus Nct75::read_temperature(int8_t& temp_celsius)
{
    uint16_t temp_raw = 0;

    auto status = read_reg_16bits(Register::Temperature, temp_raw);
    if (status == I2cStatus::Ok) {
        // High byte alone gives 1 degC resolution and naturally fits int8_t
        // across the device's guaranteed range (-55..+125 degC).
        // coverity[cert_int31_c_violation]
        temp_celsius = static_cast<int8_t>(temp_raw >> 8);
    }
    return status;
}

I2cStatus Nct75::read_temperature_q4(int16_t& temp_q4)
{
    uint16_t temp_raw = 0;

    auto status = read_reg_16bits(Register::Temperature, temp_raw);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        temp_q4 = convert_12bit_to_signed(static_cast<int16_t>(temp_raw));
    }
    return status;
}

I2cStatus Nct75::set_tos_limit(int8_t threshold)
{
    const int16_t threshold_raw = convert_signed_to_12bit(threshold);
    // coverity[cert_int31_c_violation]
    return write_reg_16bits(Register::Tos, static_cast<uint16_t>(threshold_raw));
}

I2cStatus Nct75::get_tos_limit(int8_t& threshold)
{
    uint16_t threshold_raw_u16 = 0;

    auto status = read_reg_16bits(Register::Tos, threshold_raw_u16);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_raw_u16 >> 8);
    }
    return status;
}

I2cStatus Nct75::set_thyst_limit(int8_t threshold)
{
    const int16_t threshold_raw = convert_signed_to_12bit(threshold);
    // coverity[cert_int31_c_violation]
    return write_reg_16bits(Register::Thyst, static_cast<uint16_t>(threshold_raw));
}

I2cStatus Nct75::get_thyst_limit(int8_t& threshold)
{
    uint16_t threshold_raw_u16 = 0;

    auto status = read_reg_16bits(Register::Thyst, threshold_raw_u16);
    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_raw_u16 >> 8);
    }
    return status;
}

I2cStatus Nct75::set_configuration(uint8_t configuration)
{
    return write_reg(Register::Configuration, configuration);
}

I2cStatus Nct75::get_configuration(uint8_t& configuration)
{
    return read_reg(Register::Configuration, configuration);
}

I2cStatus Nct75::trigger_one_shot()
{
    // Per datasheet: "the data written to this register is irrelevant and is
    // not stored. It is the write operation that causes the one-shot
    // conversion." We write 0 by convention.
    return write_reg(Register::OneShot, 0u);
}

I2cStatus Nct75::probe()
{
    // No device-ID register exists; ACK on a config-register read is the
    // strongest non-destructive presence signal available.
    uint8_t cfg = 0;
    return read_reg(Register::Configuration, cfg);
}
