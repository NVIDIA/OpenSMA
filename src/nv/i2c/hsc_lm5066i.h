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

namespace nv::i2c {

/**
 * LM5066I Power Monitor Driver
 * Provides functionality for voltage, current, temperature and power monitoring
 *
 * Data Format: PMBus DIRECT format (12-bit two's complement)
 * Conversion formula: x = (1/m) × (Y × 10^(-R) - b)
 * Where: x = real-world value, Y = raw value, m/b/R = coefficients from Table 47
 */
class Lm5066i : public TempSensor
{
public:
    /**
     * LM5066I Register Addresses
     */
    enum Register : uint8_t
    {
        ClearFaults     = 0x03,  // Clear all fault conditions
        ReadVin         = 0x88,  // Returns the 12-bit measured value of the input voltage
        ReadIin         = 0x89,  // Returns the 12-bit measured value of the input current
        ReadVout        = 0x8B,  // Returns the 12-bit measured value of the output voltage
        ReadTemperature = 0x8D,  // Returns the signed value of the temperature measured by the
                                 // external temperature sense diode
        ReadInputPower = 0x97,   // Retrieves averaged input power measurement
    };

    /**
     * Constructor
     * @param port I2C port to use for communication
     * @param address I2C slave address of the LM5066I
     * @param item Telemetry item ID for this sensor (default: MaxItem)
     */
    Lm5066i(Port                   port,
            uint8_t                address,
            nv::telemetry::TelemId item = nv::telemetry::TelemId::MaxItem);

    /**
     * Clear all fault conditions
     * @return I2cStatus indicating success or failure
     */
    I2cStatus clear_faults();

    /**
     * Read input voltage (VIN) and convert to microvolts
     * Formula: V(µV) = [(1/4617) × (Y × 100 + 140.0)] × 1,000,000
     * @param microvolts Reference to store the voltage in microvolts (µV)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_vin(uint32_t& microvolts);

    /**
     * Read output voltage (VOUT) and convert to microvolts
     * Formula: V(µV) = [(1/4602) × (Y × 100 - 500.0)] × 1,000,000
     * @param microvolts Reference to store the voltage in microvolts (µV)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_vout(uint32_t& microvolts);

    /**
     * Read temperature and convert to degrees Celsius
     * Formula: T(°C) = (1/16000) × (Y × 1000)
     * @param temperature Reference to store the temperature in degrees Celsius (°C)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_temperature(uint16_t& temperature);

    /**
     * Read input power and convert to milliwatts
     * Formula: P(mW) = [(1/1701) × (Y × 1000 + 4000)] × 1,000
     * @param milliwatts Reference to store the power in milliwatts (mW)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_input_power(uint32_t& milliwatts);

private:
    // PMBus DIRECT format conversion coefficients from LM5066I datasheet Table 47
    // Formula: x = (1/m) × (Y × 10^(-R) - b)

    // READ_VIN coefficients (unit: V)
    static constexpr float VinSlopeCoeff  = 4617.0f;  // m
    static constexpr float VinOffsetCoeff = -140.0f;  // b
    static constexpr int   VinExpCoeff    = -2;       // R

    // READ_VOUT coefficients (unit: V)
    static constexpr float VoutSlopeCoeff  = 4602.0f;  // m
    static constexpr float VoutOffsetCoeff = 500.0f;   // b
    static constexpr int   VoutExpCoeff    = -2;       // R

    // READ_TEMPERATURE coefficients (unit: °C)
    static constexpr float TempSlopeCoeff  = 16000.0f;  // m
    static constexpr float TempOffsetCoeff = 0.0f;      // b
    static constexpr int   TempExpCoeff    = -3;        // R

    // READ_INPUT_POWER coefficients (unit: W, CL = VDD)
    static constexpr float PowerSlopeCoeff  = 1701.0f;   // m
    static constexpr float PowerOffsetCoeff = -4000.0f;  // b
    static constexpr int   PowerExpCoeff    = -3;        // R

    // Unit conversion factors
    static constexpr float VoltsToMicrovolts = 1000000.0f;  // 1V = 1,000,000µV
    static constexpr float WattsToMilliwatts = 1000.0f;     // 1W = 1,000mW
};

}  // namespace nv::i2c
