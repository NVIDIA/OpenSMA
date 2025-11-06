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
 * TMP461 Simplified Driver
 * Provides basic functionality for local temperature reading and alert thresholds
 */
class Tmp461 : public TempSensor
{
public:
    /**
     * TMP461 Register Addresses
     */
    enum Register : uint8_t
    {
        LocalTempHigh            = 0x00,  // Local temperature high byte
        RemoteTempHigh           = 0x01,  // Remote temperature high byte
        Status                   = 0x02,  // Status register
        configurationRead        = 0x03,  // Configuration register (read)
        LocalTempHighLimitRead   = 0x05,  // Local temperature high limit (read)
        RemoteTempHighLimitRead  = 0x07,  // Remote temperature high limit (read)
        configurationWrite       = 0x09,  // Configuration register (write)
        LocalTempHighLimitWrite  = 0x0B,  // Local temperature high limit (write)
        RemoteTempHighLimitWrite = 0x0D,  // Remote temperature high limit (write)
        LocalTempLow             = 0x15,  // Local temperature low byte
        RemoteThermLimit         = 0x19,  // Remote THERM limit (write/read)
        LocalThermLimit          = 0x20,  // Local THERM limit (write/read)
        ManufacturerId           = 0xFE,  // Manufacturer ID register
    };

    enum Configuration : uint8_t
    {
        THERM2_Enabled = 1u << 5,  // THERM2 enabled
    };

    /**
     * Constructor
     * @param port I2C port to use for communication
     * @param address I2C slave address of the TMP461
     * @param item Telemetry item ID for this sensor
     */
    Tmp461(Port                   port,
           uint8_t                address,
           nv::telemetry::TelemId item = nv::telemetry::TelemId::MaxItem);

    /**
     * Temperature Reading Functions
     */

    /**
     * Get local temperature in Celsius (integer)
     * @param temp_integer Reference to store the temperature value
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_local_high_temp(int8_t& temp_integer);

    /**
     * Get remote temperature in Celsius (integer)
     * @param temp_integer Reference to store the temperature value
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_remote_high_temp(int8_t& temp_integer);

    /**
     * Alert Threshold Functions
     */

    /**
     * Set local temperature alert threshold (high limit only)
     * @param threshold Temperature threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_local_high_alert_thresholds(int8_t threshold);

    /**
     * Get current local temperature alert threshold (high limit only)
     * @param threshold Reference to store the threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_local_high_alert_thresholds(int8_t& threshold);

    /**
     * Set remote temperature alert threshold (high limit)
     * @param threshold Temperature threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_remote_high_alert_thresholds(int8_t threshold);

    /**
     * Get remote temperature alert threshold (high limit)
     * @param threshold Reference to store the threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_remote_high_alert_thresholds(int8_t& threshold);

    /**
     * THERM Limit Functions
     */

    /**
     * Set remote THERM limit
     * @param threshold Temperature threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_remote_therm_limit(int8_t threshold);

    /**
     * Get remote THERM limit
     * @param threshold Reference to store the threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_remote_therm_limit(int8_t& threshold);

    /**
     * Set local THERM limit
     * @param threshold Temperature threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_local_therm_limit(int8_t threshold);

    /**
     * Get local THERM limit
     * @param threshold Reference to store the threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_local_therm_limit(int8_t& threshold);

    /**
     * Set configuration
     * @param configuration Configuration value (uint8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_configuration(uint8_t configuration);

    /**
     * Get configuration
     * @param configuration Reference to store the configuration value (uint8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_configuration(uint8_t& configuration);
};

}  // namespace nv::i2c