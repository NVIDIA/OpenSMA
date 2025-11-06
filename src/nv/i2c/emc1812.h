/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
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
 * EMC1812 Simplified Driver
 * Provides basic functionality for local and remote temperature reading and alert thresholds
 */
class Emc1812 : public TempSensor
{
public:
    /**
     * EMC1812 Register Addresses
     */
    enum Register : uint8_t
    {
        LocalTempHigh       = 0x00,  // Local temperature high byte
        LocalTempLow        = 0x29,  // Local temperature low byte
        RemoteTempHigh      = 0x01,  // Remote temperature high byte
        RemoteTempLow       = 0x10,  // Remote temperature low byte
        LocalTempHighLimit  = 0x05,  // Local temperature high limit
        LocalTempLowLimit   = 0x06,  // Local temperature low limit
        RemoteTempHighLimit = 0x07,  // Remote temperature high limit
        RemoteTempLowLimit  = 0x08,  // Remote temperature low limit
        Status              = 0x02,  // Status register
        ManufacturerId      = 0xFE,  // Manufacturer ID register
    };

    /**
     * Constructor
     * @param port I2C port to use for communication
     * @param address I2C slave address of the EMC1812
     * @param item Telemetry item ID for this sensor
     */
    Emc1812(Port                   port,
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
     * Alert Threshold Functions
     */

    /**
     * Set local temperature alert threshold (high limit only)
     * @param threshold Temperature threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_local_high_alert_threshold(int8_t threshold);

    /**
     * Get current local temperature alert threshold (high limit only)
     * @param threshold Reference to store the threshold in Celsius (int8_t)
     * @return I2cStatus indicating success or failure
     */
    I2cStatus get_local_high_alert_threshold(int8_t& threshold);
};
}  // namespace nv::i2c
