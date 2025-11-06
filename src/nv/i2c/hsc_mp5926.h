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
 * MP5926 Hot-Swap Controller Driver
 * Provides functionality for voltage, current, temperature and power monitoring
 *
 * Data Format: PMBus DIRECT format (16-bit)
 * Conversion formula: x = (1/m) × (Y × 10^(-R) - b)
 * Where: x = real-world value, Y = raw value, m/b/R = coefficients from MP5926 datasheet
 */
class Mp5926 : public TempSensor
{
public:
    /**
     * MP5926 Register Addresses
     */
    enum Register : uint8_t
    {
        ClearFaults     = 0x03,  // Clear all fault conditions
        ReadVin         = 0x88,  // Returns the measured value of the input voltage
        ReadIin         = 0x89,  // Returns the measured value of the input current
        ReadVout        = 0x8B,  // Returns the measured value of the output voltage
        ReadTemperature = 0x8D,  // Returns the temperature measured by the sensor
        ReadInputPower  = 0x97,  // Retrieves averaged input power measurement
    };

    /**
     * Constructor
     * @param port I2C port to use for communication
     * @param address I2C slave address of the MP5926
     * @param item Telemetry item ID for this sensor (default: MaxItem)
     */
    Mp5926(Port                   port,
           uint8_t                address,
           nv::telemetry::TelemId item = nv::telemetry::TelemId::MaxItem);

    /**
     * Clear all fault conditions
     * @return I2cStatus indicating success or failure
     */
    I2cStatus clear_faults();

    /**
     * Read input voltage (VIN) and convert to microvolts
     * Direct mode: m=16, b=0, r=0
     * Formula: V(µV) = [(1/16) × Y] × 1,000,000
     * @param microvolts Reference to store the voltage in microvolts (µV)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_vin(uint32_t& microvolts);

    /**
     * Read output voltage (VOUT) and convert to microvolts
     * Direct mode: m=16, b=0, r=0
     * Formula: V(µV) = [(1/16) × Y] × 1,000,000
     * @param microvolts Reference to store the voltage in microvolts (µV)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_vout(uint32_t& microvolts);

    /**
     * Read temperature and convert to degrees Celsius
     * Direct mode: m=4, b=0, r=0
     * Formula: T(°C) = (1/4) × Y
     * @param temperature Reference to store the temperature in degrees Celsius (°C)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_temperature(uint16_t& temperature);

    /**
     * Read input power and convert to milliwatts
     * Direct mode: m=0.25, b=0, r=0
     * Formula: P(mW) = [(1/0.25) × Y] × 1,000
     * @param milliwatts Reference to store the power in milliwatts (mW)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_input_power(uint32_t& milliwatts);

private:
    // PMBus DIRECT format conversion coefficients from MP5926 datasheet
    // Formula: x = (1/m) × (Y × 10^(-R) - b)

    // READ_VIN coefficients (unit: V)
    // Resolution: 62.5mV/LSB, range up to 127.9V
    static constexpr float VinSlopeCoeff  = 16.0f;  // m
    static constexpr float VinOffsetCoeff = 0.0f;   // b
    static constexpr int   VinExpCoeff    = 0;      // R

    // READ_VOUT coefficients (unit: V)
    // Resolution: 62.5mV/LSB, range up to 127.9V
    static constexpr float VoutSlopeCoeff  = 16.0f;  // m
    static constexpr float VoutOffsetCoeff = 0.0f;   // b
    static constexpr int   VoutExpCoeff    = 0;      // R

    // READ_TEMPERATURE coefficients (unit: °C)
    // Resolution: 0.25°C/LSB
    static constexpr float TempSlopeCoeff  = 4.0f;  // m
    static constexpr float TempOffsetCoeff = 0.0f;  // b
    static constexpr int   TempExpCoeff    = 0;     // R

    // READ_INPUT_POWER coefficients (unit: W)
    // Resolution: 4W/LSB, range up to 8188W
    static constexpr float PowerSlopeCoeff  = 0.25f;  // m
    static constexpr float PowerOffsetCoeff = 0.0f;   // b
    static constexpr int   PowerExpCoeff    = 0;      // R

    // Unit conversion factors
    static constexpr float VoltsToMicrovolts = 1000000.0f;  // 1V = 1,000,000µV
    static constexpr float WattsToMilliwatts = 1000.0f;     // 1W = 1,000mW
};

}  // namespace nv::i2c
