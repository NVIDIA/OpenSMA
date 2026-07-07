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
#include "nv/i2c/common.h"
#include <span>

namespace nv::i2c {

class PowerSensor
{
protected:
    Port    _port;
    uint8_t _address;

    // Direct Format coefficient structure drivers
    struct DirectFormatCoeff
    {
        float    m;         // slope (divisor)
        float    b;         // offset
        float    exp_mult;  // pre-calculated 10^(-r)
        uint16_t mask;      // bit mask for extracting valid data
    };

    DirectFormatCoeff _ot_warn_limit_coeff;
    DirectFormatCoeff _vin_coeff;
    DirectFormatCoeff _vout_coeff;
    DirectFormatCoeff _temp_coeff;
    DirectFormatCoeff _power_input_coeff;

    // Unit conversion constant
    static constexpr float VoltsToMicrovolts = 1000000.0f;

public:
    enum Register
    {
        ClearFaults     = 0x03,
        OtFaultLimit    = 0x4F,
        OtWarnLimit     = 0x51,
        StatusWord      = 0x79,
        ReadVin         = 0x88,
        ReadVout        = 0x8B,
        ReadTemperature = 0x8D,
        ReadInputPower  = 0x97
    };

    PowerSensor(Port port, uint8_t address);
    I2cStatus init();
    I2cStatus clear_faults();
    I2cStatus read_faults(uint16_t& faults);
    I2cStatus read_ot_warn_limit(uint8_t& limit);
    I2cStatus write_ot_warn_limit(uint8_t limit);
    I2cStatus read_vin(uint32_t& microvolts);
    I2cStatus read_vout(uint32_t& microvolts);
    I2cStatus read_temperature(uint16_t& temperature);
    I2cStatus read_input_power(uint32_t& milliwatts);
    void      set_power_input_coeff(float m, float b, float exp_mult, uint16_t mask);
    I2cStatus send_command(uint8_t offset);
    I2cStatus write_reg(uint8_t offset, uint8_t value);
    I2cStatus write_reg_16bits(uint8_t offset, uint16_t value);
    I2cStatus read_reg(uint8_t offset, uint8_t& value);
    I2cStatus read_reg_16bits(uint8_t offset, uint16_t& value);
    I2cStatus read_block(uint8_t offset, std::span<uint8_t> value, uint8_t& length);
};

}  // namespace nv::i2c