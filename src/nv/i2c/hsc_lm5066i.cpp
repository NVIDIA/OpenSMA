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

#include "nv/i2c/hsc_lm5066i.h"
#include "nv/nv.h"
#include <climits>

using namespace nv::i2c;

Lm5066i::Lm5066i(Port port, uint8_t address, nv::telemetry::TelemId item)
: TempSensor(port, address, item)
{}

I2cStatus Lm5066i::clear_faults()
{
    // Write to CLEAR_FAULTS register to clear all fault conditions
    // For PMBus CLEAR_FAULTS command, we send the command byte only (no data)
    return write_reg(Register::ClearFaults, 0x00);
}

I2cStatus Lm5066i::read_vin(uint32_t& microvolts)
{
    // Read input voltage from READ_VIN register (16-bit)
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadVin, raw_value);

    if (status == I2cStatus::Ok) {
        // LM5066I READ_VIN coefficients from Table 47
        // Formula: V(V) = (1/m) × (Y × 10^(-R) - b)
        // VinExpCoeff = -2 means multiply by 10^(-(-2)) = 10^2 = 100

        // Convert 12-bit two's complement to signed integer
        const auto y = static_cast<int16_t>(raw_value);

        // Apply DIRECT format conversion formula and convert to microvolts
        // V(V) = (Y × 100 - VinOffsetCoeff) / VinSlopeCoeff
        // V(µV) = V(V) × VoltsToMicrovolts
        constexpr float exp_multiplier = 100.0f;  // 10^(-VinExpCoeff)
        const float     volts = (static_cast<float>(y) * exp_multiplier - VinOffsetCoeff)
                          / VinSlopeCoeff;
        microvolts = static_cast<uint32_t>(volts * VoltsToMicrovolts);
    }

    return status;
}

I2cStatus Lm5066i::read_vout(uint32_t& microvolts)
{
    // Read output voltage from READ_VOUT register (16-bit)
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadVout, raw_value);

    if (status == I2cStatus::Ok) {
        // LM5066I READ_VOUT coefficients from Table 47
        // Formula: V(V) = (1/m) × (Y × 10^(-R) - b)
        // VoutExpCoeff = -2 means multiply by 10^(-(-2)) = 10^2 = 100

        // Convert 12-bit two's complement to signed integer
        const auto y = static_cast<int16_t>(raw_value);

        // Apply DIRECT format conversion formula and convert to microvolts
        // V(V) = (Y × 100 - VoutOffsetCoeff) / VoutSlopeCoeff
        // V(µV) = V(V) × VoltsToMicrovolts
        constexpr float exp_multiplier = 100.0f;  // 10^(-VoutExpCoeff)
        const float     volts = (static_cast<float>(y) * exp_multiplier - VoutOffsetCoeff)
                          / VoutSlopeCoeff;
        microvolts = static_cast<uint32_t>(volts * VoltsToMicrovolts);
    }

    return status;
}

I2cStatus Lm5066i::read_temperature(uint16_t& temperature)
{
    // Read temperature from READ_TEMPERATURE register (16-bit)
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadTemperature, raw_value);

    if (status == I2cStatus::Ok) {
        // LM5066I READ_TEMPERATURE coefficients from Table 47
        // Formula: T(°C) = (1/m) × (Y × 10^(-R) - b)
        // TempExpCoeff = -3 means multiply by 10^(-(-3)) = 10^3 = 1000

        // Convert 12-bit two's complement to signed integer
        const auto y = static_cast<int16_t>(raw_value);

        // Apply DIRECT format conversion formula
        // T(°C) = (Y × 1000 - TempOffsetCoeff) / TempSlopeCoeff
        constexpr float exp_multiplier = 1000.0f;  // 10^(-TempExpCoeff)
        const float temp_celsius = (static_cast<float>(y) * exp_multiplier - TempOffsetCoeff)
                                 / TempSlopeCoeff;
        temperature = static_cast<uint16_t>(temp_celsius);
    }

    return status;
}

I2cStatus Lm5066i::read_input_power(uint32_t& milliwatts)
{
    // Read input power from READ_INPUT_POWER register (16-bit)
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadInputPower, raw_value);

    if (status == I2cStatus::Ok) {
        // LM5066I READ_PIN coefficients from Table 47 (CL = VDD)
        // Formula: P(W) = (1/m) × (Y × 10^(-R) - b)
        // PowerExpCoeff = -3 means multiply by 10^(-(-3)) = 10^3 = 1000

        // Convert 12-bit two's complement to signed integer
        const auto y = static_cast<int16_t>(raw_value);

        // Apply DIRECT format conversion formula and convert to milliwatts
        // P(W) = (Y × 1000 - PowerOffsetCoeff) / PowerSlopeCoeff
        // P(mW) = P(W) × WattsToMilliwatts
        constexpr float exp_multiplier = 1000.0f;  // 10^(-PowerExpCoeff)
        const float     watts = (static_cast<float>(y) * exp_multiplier - PowerOffsetCoeff)
                          / PowerSlopeCoeff;
        milliwatts = static_cast<uint32_t>(watts * WattsToMilliwatts);
    }

    return status;
}
