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
#include "nv/i2c/powersensor/sensor.h"
#include "nv/nv.h"

using namespace nv::i2c;

namespace {
// Default coefficient values for PowerSensor
constexpr float    DefaultSlopeM  = 1.0f;
constexpr float    DefaultOffsetB = 0.0f;
constexpr float    DefaultExpMult = 1.0f;
constexpr uint16_t DefaultMask    = 0xFFFF;  // 16-bit full mask
}  // namespace

PowerSensor::PowerSensor(Port port, uint8_t address)
: _port(port)
, _address(address)
, _ot_warn_limit_coeff{DefaultSlopeM, DefaultOffsetB, DefaultExpMult, DefaultMask}
, _vin_coeff{DefaultSlopeM, DefaultOffsetB, DefaultExpMult, DefaultMask}
, _vout_coeff{DefaultSlopeM, DefaultOffsetB, DefaultExpMult, DefaultMask}
, _temp_coeff{DefaultSlopeM, DefaultOffsetB, DefaultExpMult, DefaultMask}
, _power_input_coeff{DefaultSlopeM, DefaultOffsetB, DefaultExpMult, DefaultMask}
{}

void PowerSensor::set_power_input_coeff(float m, float b, float exp_mult, uint16_t mask)
{
    _power_input_coeff = {m, b, exp_mult, mask};
}

I2cStatus PowerSensor::clear_faults()
{
    // PMBus CLEAR_FAULTS (0x03) is a Send Byte command.
    return send_command(Register::ClearFaults);
}

I2cStatus PowerSensor::read_faults(uint16_t& faults)
{
    return read_reg_16bits(Register::StatusWord, faults);
}

I2cStatus PowerSensor::read_ot_warn_limit(uint8_t& limit)
{
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::OtWarnLimit, raw_value);

    if (status == I2cStatus::Ok) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _ot_warn_limit_coeff.mask;

        // Apply Direct Format: T = (Y × 10^(-R) - b) / m
        const float temp_celsius = (static_cast<float>(y) * _ot_warn_limit_coeff.exp_mult
                                    - _ot_warn_limit_coeff.b)
                                 / _ot_warn_limit_coeff.m;

        limit = static_cast<uint8_t>(temp_celsius);
    }

    return status;
}

I2cStatus PowerSensor::write_ot_warn_limit(uint8_t limit)
{
    const float temp_celsius = (static_cast<float>(limit) * _ot_warn_limit_coeff.m
                                + _ot_warn_limit_coeff.b)
                             / _ot_warn_limit_coeff.exp_mult;
    const uint16_t raw_value = static_cast<uint16_t>(temp_celsius) & _ot_warn_limit_coeff.mask;

    return write_reg_16bits(Register::OtWarnLimit, raw_value);
}

I2cStatus PowerSensor::init()
{
    return I2cStatus::Ok;
}

I2cStatus PowerSensor::read_vin(uint32_t& microvolts)
{
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadVin, raw_value);

    if (status == I2cStatus::Ok) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _vin_coeff.mask;

        // Apply Direct Format: V = (Y × 10^(-R) - b) / m
        const float volts = (static_cast<float>(y) * _vin_coeff.exp_mult - _vin_coeff.b)
                          / _vin_coeff.m;

        // Convert to microvolts
        microvolts = static_cast<uint32_t>(volts * VoltsToMicrovolts);
    }

    return status;
}

I2cStatus PowerSensor::read_vout(uint32_t& microvolts)
{
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadVout, raw_value);

    if (status == I2cStatus::Ok) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _vout_coeff.mask;

        // Apply Direct Format: V = (Y × 10^(-R) - b) / m
        const float volts = (static_cast<float>(y) * _vout_coeff.exp_mult - _vout_coeff.b)
                          / _vout_coeff.m;

        // Convert to microvolts
        microvolts = static_cast<uint32_t>(volts * VoltsToMicrovolts);
    }

    return status;
}

I2cStatus PowerSensor::read_temperature(uint16_t& temperature)
{
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadTemperature, raw_value);

    if (status == I2cStatus::Ok) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _temp_coeff.mask;

        // Apply Direct Format: T = (Y × 10^(-R) - b) / m
        const float temp_celsius = (static_cast<float>(y) * _temp_coeff.exp_mult
                                    - _temp_coeff.b)
                                 / _temp_coeff.m;

        temperature = static_cast<uint16_t>(temp_celsius);
    }

    return status;
}

I2cStatus PowerSensor::read_input_power(uint32_t& milliwatts)
{
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadInputPower, raw_value);

    if (status == I2cStatus::Ok) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _power_input_coeff.mask;

        // Apply Direct Format: P(W) = (Y × 10^(-R) - b) / m
        const float watts = (static_cast<float>(y) * _power_input_coeff.exp_mult
                             - _power_input_coeff.b)
                          / _power_input_coeff.m;

        // Convert to milliwatts
        milliwatts = static_cast<uint32_t>(watts * 1000.0f);
    }

    return status;
}