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
#include "nv/i2c/powersensor/sensor.h"
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
class Mp5926 : public PowerSensor
{
public:
    enum Register
    {
        MFR_SYS_CTRL = 0xCF
    };
    /**
     * Constructor
     * @param port I2C port to use for communication
     * @param address I2C slave address of the MP5926
     */
    Mp5926(Port port, uint8_t address);
    /**
     * Read input voltage (VIN) and convert to microvolts
     * Format: Linear11 (bits[15:11]=exponent N, bits[10:0]=mantissa Y)
     * Formula: VIN = Y × 2^N (default format per datasheet)
     * Resolution: 62.5mV/LSB, range up to 127.9V
     * @param microvolts Reference to store the voltage in microvolts (µV)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_vin(uint32_t& microvolts);

    /**
     * Read output voltage (VOUT) and convert to microvolts
     * Format: Linear11 (bits[15:11]=exponent N, bits[10:0]=mantissa Y)
     * Formula: VOUT = Y × 2^N (default format per datasheet)
     * Resolution: 62.5mV/LSB, range up to 127.9V
     * @param microvolts Reference to store the voltage in microvolts (µV)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_vout(uint32_t& microvolts);

    /**
     * Read temperature and convert to degrees Celsius
     * Format: Linear11 (bits[15:11]=exponent N, bits[10:0]=mantissa Y)
     * Formula: TEMPERATURE = Y × 2^N
     * Resolution: 0.25°C/LSB
     * @param temperature Reference to store the temperature in degrees Celsius (°C)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_temperature(uint16_t& temperature);

    /**
     * Read input power and convert to milliwatts
     * Format: Linear11 (bits[15:11]=exponent N, bits[10:0]=mantissa Y)
     * Formula: input power = Y × 2^N
     * @param milliwatts Reference to store the power in milliwatts (mW)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_input_power(uint32_t& milliwatts);

    /**
     * Read output power - calculated from VOUT × IOUT (MP5926 doesn't have direct POUT
     * register)
     * @param milliwatts Reference to store the power in milliwatts (mW)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_output_power(uint32_t& milliwatts);

private:
    // PMBus DIRECT format conversion coefficients from MP5926 datasheet
    // Formula: x = (1/m) × (Y × 10^(-R) - b)

    // OT_WARN_LIMIT coefficients (unit: °C)
    // Resolution: 0.25°C/LSB
    static constexpr float    OtWarnLimitSlopeM  = 4.0f;    // m
    static constexpr float    OtWarnLimitOffsetB = 0.0f;    // b
    static constexpr float    OtWarnLimitExpMult = 1.0f;    // 10^(-R), where R=0
    static constexpr uint16_t OtWarnLimitMask    = 0x07FF;  // 11-bit data mask

    // READ_VIN coefficients (unit: V)
    // Resolution: 62.5mV/LSB, range up to 127.9V
    static constexpr float    VinSlopeM  = 16.0f;   // m
    static constexpr float    VinOffsetB = 0.0f;    // b
    static constexpr float    VinExpMult = 1.0f;    // 10^(-R), where R=0
    static constexpr uint16_t VinMask    = 0x07FF;  // 11-bit data mask

    // READ_VOUT coefficients (unit: V)
    // Resolution: 62.5mV/LSB, range up to 127.9V
    static constexpr float    VoutSlopeM  = 16.0f;   // m
    static constexpr float    VoutOffsetB = 0.0f;    // b
    static constexpr float    VoutExpMult = 1.0f;    // 10^(-R), where R=0
    static constexpr uint16_t VoutMask    = 0x07FF;  // 11-bit data mask

    // READ_TEMPERATURE coefficients (unit: °C)
    // Resolution: 0.25°C/LSB
    static constexpr float    TempSlopeM  = 4.0f;    // m
    static constexpr float    TempOffsetB = 0.0f;    // b
    static constexpr float    TempExpMult = 1.0f;    // 10^(-R), where R=0
    static constexpr uint16_t TempMask    = 0x07FF;  // 11-bit data mask

    // READ_INPUT_POWER coefficients (unit: W)
    // Resolution: 4W/LSB, range up to 8188W
    static constexpr float    PowerSlopeM  = 0.25f;   // m
    static constexpr float    PowerOffsetB = 0.0f;    // b
    static constexpr float    PowerExpMult = 1.0f;    // 10^(-R), where R=0
    static constexpr uint16_t PowerMask    = 0x07FF;  // 11-bit data mask

    // Unit conversion factors
    static constexpr float VoltsToMicrovolts = 1000000.0f;  // 1V = 1,000,000µV
    static constexpr float WattsToMilliwatts = 1000.0f;     // 1W = 1,000mW

    // MFR_SYS_CTRL bit infomation
    static constexpr uint8_t MFR_SYS_CTRL_REPORT_FORMAT_SHIFT = 12;
    static constexpr uint8_t MFR_SYS_CTRL_REPORT_FORMAT_MASK  = 0x1;
    static constexpr uint8_t REPORT_FORMAT_DIRECT_MODE        = 0x0;
    static constexpr uint8_t REPORT_FORMAT_LINEAR11_MODE      = 0x1;

    // Linear11 format constants (for VIN, VOUT, TEMPERATURE, INPUT_POWER)
    static constexpr uint8_t LINEAR11_EXPONENT_SHIFT    = 11;    // Shift to extract bits[15:11]
    static constexpr uint8_t LINEAR11_EXPONENT_MASK     = 0x1F;  // 5-bit mask for exponent
    static constexpr uint8_t LINEAR11_EXPONENT_SIGN_BIT = 0x10;  // Bit 4 (sign bit for 5-bit)
    static constexpr uint8_t LINEAR11_EXPONENT_SIGN_EXT = 0xE0;  // Sign extension for negative
                                                                 // exponent
    static constexpr uint16_t LINEAR11_MANTISSA_MASK     = 0x7FF;   // 11-bit mask for mantissa
    static constexpr uint16_t LINEAR11_MANTISSA_SIGN_BIT = 0x400;   // Bit 10 (sign bit for
                                                                    // 11-bit)
    static constexpr uint16_t LINEAR11_MANTISSA_SIGN_EXT = 0xF800;  // Sign extension for
                                                                    // negative mantissa
    static constexpr int LINEAR11_MAX_SHIFT = 32;                   // Maximum safe shift amount
};

}  // namespace nv::i2c
