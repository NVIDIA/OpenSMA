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
 * MPS MP29540 HSCC (Hot Swap Controller with Current sharing) Driver
 * Provides functionality for voltage, current, temperature and power monitoring
 *
 * Data Format: PMBus formats (Direct Format, Unsigned Binary)
 * Conversion depends on specific command - see MPS MP29540 datasheet
 */
class Mp29540 : public PowerSensor
{
public:
    enum Register
    {
        Page        = 0x00,
        VidStep     = 0x29,
        MfrVoutRtop = 0x51,
        MfrVoutRbot = 0x5E,
        ReadPinEst  = 0x94
    };
    /**
     * Constructor
     * @param port I2C port to use for communication
     * @param address I2C slave address of the MPS MP29540
     */
    Mp29540(Port port, uint8_t address);

    /**
     * Read OT warning limit
     * @param limit Reference to store the limit in degrees Celsius (°C)
     * @return I2cStatus indicating success or failure
     */
    nv::i2c::I2cStatus read_ot_warn_limit(uint8_t& limit);

    /**
     * Write OT warning limit
     * @param limit Limit in degrees Celsius (°C)
     * @return I2cStatus indicating success or failure
     */
    nv::i2c::I2cStatus write_ot_warn_limit(uint8_t limit);

    /**
     * Read output voltage (VOUT) and convert to microvolts
     * @param microvolts Reference to store the voltage in microvolts (µV)
     * @return I2cStatus indicating success or failure
     */
    nv::i2c::I2cStatus read_vout(uint32_t& microvolts);

    /**
     * Read temperature and convert to degrees Celsius
     * @param temperature Reference to store the temperature in degrees Celsius (°C)
     * @return I2cStatus indicating success or failure
     */
    nv::i2c::I2cStatus read_temperature(uint16_t& temperature);

    /**
     * Read system input power and convert to milliwatts
     * Uses READ_PIN_SYS (0x97) for system power measurement
     * @param milliwatts Reference to store the power in milliwatts (mW)
     * @return I2cStatus indicating success or failure
     */
    nv::i2c::I2cStatus read_input_power(uint32_t& milliwatts);

private:
    /**
     * Switch to specified PMBus page
     * @param page_number Page number to switch to (0 or 1)
     * @return I2cStatus indicating success or failure
     */
    nv::i2c::I2cStatus switch_to_page(uint8_t page_number);

    /**
     * Get current PMBus page
     * @param current_page Reference to store current page number
     * @return I2cStatus indicating success or failure
     */
    nv::i2c::I2cStatus get_current_page(uint8_t& current_page);

    /**
     * Calculate VOUT_DIVIDER from resistor values
     * Handles all page switching internally: saves current page, switches to Page 1 to read
     * MFR_VOUT_RBOT and MFR_VOUT_RTOP, calculates divider ratio, and restores original page
     * @param vout_divider Reference to store the calculated divider value
     * @return I2cStatus indicating success or failure
     */
    nv::i2c::I2cStatus calculate_vout_divider(float& vout_divider);

    // Common constants
    static constexpr size_t  ClearFaultsCommandSize = 1;
    static constexpr uint8_t Page0                  = 0;
    static constexpr uint8_t Page1                  = 1;

    // Unit conversion constants
    static constexpr float VoltsToMicrovolts = 1000000.0f;
    static constexpr float WattsToMilliwatts = 1000.0f;

    // VIN reading constants
    static constexpr uint16_t VinValueMask         = 0x7FF;   // 11-bit mask
    static constexpr float    VinResolutionVPerLsb = 0.125f;  // (1/8)V/LSB

    // VOUT reading constants
    static constexpr uint16_t VoutValueMask = 0xFFF;  // 12-bit mask
    static constexpr uint8_t  VidStepShift  = 10;     // Shift to bits 12:10
    static constexpr uint16_t VidStepMask   = 0x07;   // 3-bit mask for bits 12:10

    // MFR_VOUT_RBOT/RTOP register bit masks (Page 1)
    static constexpr uint16_t RbotValueMask = 0x0FFF;  // 12-bit mask for RBOT bits 11:0
    static constexpr uint16_t RtopValueMask = 0x7FFF;  // 15-bit mask for RTOP bits 14:0
    static constexpr uint16_t ZeroDivisor   = 0;
    static constexpr float    ZeroDivider   = 0.0f;

    // Linear11 format constants
    static constexpr uint8_t  ExponentShift      = 11;      // Shift to bits 15:11
    static constexpr uint8_t  ExponentMask       = 0x1F;    // 5-bit mask
    static constexpr uint8_t  ExponentSignBit    = 0x10;    // Bit 4 for sign
    static constexpr uint8_t  ExponentSignExtend = 0xE0;    // Sign extension mask
    static constexpr uint16_t MantissaMask       = 0x7FF;   // 11-bit mask
    static constexpr uint16_t MantissaSignBit    = 0x400;   // Bit 10 for sign
    static constexpr uint16_t MantissaSignExtend = 0xF800;  // Sign extension mask
    static constexpr int8_t   ZeroExponent       = 0;

    // Power reading constants
    static constexpr uint8_t  PowerExponentShift = 11;     // Shift to bits 15:11
    static constexpr uint8_t  PowerExponentMask  = 0x1F;   // 5-bit mask
    static constexpr uint16_t PowerValueMask     = 0x7FF;  // 11-bit mask
    static constexpr float    PowerMultiplier    = 2.0f;   // Base multiplier from datasheet
};

}  // namespace nv::i2c
