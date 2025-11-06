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
 * TMP1075 Temperature Sensor Driver
 * Provides functionality for temperature reading and alert thresholds
 * Temperature data is 12-bit in two's complement format
 */
class Tmp1075 : public TempSensor
{
public:
    static constexpr uint16_t ExpectedDeviceId = 0x7500;
    /**
     * TMP1075 Register Addresses
     */
    enum Register : uint8_t
    {
        Temperature   = 0x00,  // Temperature register (12-bit, read-only)
        Configuration = 0x01,  // Configuration register
        LowLimit      = 0x02,  // Low limit register (12-bit, R/W)
        HighLimit     = 0x03,  // High limit register (12-bit, R/W)
        DeviceId      = 0x0F,  // Device ID register
    };

    /**
     * Constructor
     * @param port I2C port to use for communication
     * @param address I2C slave address of the TMP1075
     * @param item Telemetry item ID for this sensor
     */
    Tmp1075(Port                   port,
            uint8_t                address,
            nv::telemetry::TelemId item = nv::telemetry::TelemId::MaxItem);

    /**
     * Temperature Reading Functions
     */

    /**
     * Read temperature and convert to integer Celsius
     * Reads 12-bit temperature register and extracts integer part (bits [15:8])
     * Temperature range: -128°C to +127°C
     * @param temp_celsius Reference to store the integer temperature in Celsius
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_temperature(int8_t& temp_celsius);

    /**
     * Alert Threshold Functions (12-bit registers)
     */

    /**
     * Set low limit temperature threshold
     * @param threshold Temperature threshold in Celsius (will be converted to 12-bit format)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_low_limit(int8_t threshold);

    /**
     * Get low limit temperature threshold
     * @param threshold Reference to store the threshold in Celsius
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_low_limit(int8_t& threshold);

    /**
     * Set high limit temperature threshold
     * @param threshold Temperature threshold in Celsius (will be converted to 12-bit format)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_high_limit(int8_t threshold);

    /**
     * Get high limit temperature threshold
     * @param threshold Reference to store the threshold in Celsius
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_high_limit(int8_t& threshold);

    /**
     * Get device ID
     * Reads the 16-bit device ID register (address 0x0F)
     * Default value is 0x7500 with MSB reading 0x75 indicating TMP1075
     * @param device_id Reference to store the 16-bit device ID
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_device_id(uint16_t& device_id);

private:
    /**
     * Helper function to convert 12-bit two's complement to signed value
     * Uses arithmetic right shift for automatic sign extension
     * @param raw_value 16-bit value with 12-bit temperature in upper bits
     * @return Signed temperature value
     */
    static int16_t convert_12bit_to_signed(int16_t raw_value);

    /**
     * Helper function to convert signed temperature to 12-bit format
     * @param temp_celsius Temperature in Celsius
     * @return 16-bit value with 12-bit temperature in proper format
     */
    static int16_t convert_signed_to_12bit(int8_t temp_celsius);
};

}  // namespace nv::i2c