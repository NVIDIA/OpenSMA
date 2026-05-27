/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
 * NCT75 (onsemi) Temperature Sensor Driver
 *
 * 12-bit sigma-delta temperature sensor with SMBus/I2C interface, pin and
 * register compatible with LM75 / TCN75 and very close to TI TMP1075. The
 * part has a 7-bit base address of 1001b with the three LSBs set by the
 * A2/A1/A0 pins, giving eight possible addresses 0x48-0x4F.
 *
 * Datasheet: NCT75MNR2G (onsemi NCT75/D rev 7).
 *
 * Temperature data is a 12-bit two's-complement value left-justified in a
 * 16-bit register (bits [15:4], lower 4 bits always zero), resolution
 * 0.0625 degC. Guaranteed measurement range -55 degC .. +125 degC.
 *
 * Unlike TMP1075 the NCT75 exposes no device-ID or manufacturer-ID
 * register, so identification is by-address-only.
 */
class Nct75 : public TempSensor
{
public:
    /**
     * NCT75 register set (address-pointer P[2:0] selects the target).
     */
    enum Register : uint8_t
    {
        Temperature   = 0x00,  // R, 16-bit, 12-bit two's complement left-justified
        Configuration = 0x01,  // R/W, 8-bit
        Thyst         = 0x02,  // R/W, 16-bit hysteresis limit  (default 0x4B00 = +75 degC)
        Tos           = 0x03,  // R/W, 16-bit overtemp limit    (default 0x5000 = +80 degC)
        OneShot       = 0x04,  // W,   8-bit; writing any value triggers a one-shot conversion
    };

    /**
     * Configuration register bit definitions.
     * Bits D7-D6 are reserved (write 0).
     */
    enum Configuration : uint8_t
    {
        Shutdown        = 1u << 0,  // 0 = normal, 1 = shutdown (interface remains active)
        InterruptMode   = 1u << 1,  // 0 = comparator mode, 1 = interrupt mode
        OsAlertActiveHi = 1u << 2,  // 0 = active-low (default), 1 = active-high
        // Fault-queue selects how many consecutive over-temp conversions must
        // occur before OS/ALERT asserts. Bits [4:3]:
        //   00 = 1 fault (default), 01 = 2, 10 = 4, 11 = 6.
        FaultQueue1    = 0u << 3,
        FaultQueue2    = 1u << 3,
        FaultQueue4    = 2u << 3,
        FaultQueue6    = 3u << 3,
        FaultQueueMask = 3u << 3,
        OneShotEnable  = 1u << 5,  // 0 = continuous conversion, 1 = arm one-shot
    };

    static constexpr int8_t DefaultTosCelsius   = 80;
    static constexpr int8_t DefaultThystCelsius = 75;

    /**
     * Constructor
     * @param port    I2C port to use for communication
     * @param address 7-bit I2C slave address (0x48-0x4F)
     * @param item    Telemetry item ID for this sensor
     */
    Nct75(Port                   port,
          uint8_t                address,
          nv::telemetry::TelemId item = nv::telemetry::TelemId::MaxItem);

    /**
     * Read temperature as a signed integer in Celsius.
     * Reads the 16-bit temperature register and returns the upper byte
     * (8-bit signed integer Celsius, 1 degC resolution).
     * @param temp_celsius Reference to store the temperature
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_temperature(int8_t& temp_celsius);

    /**
     * Read the full 12-bit temperature value in units of 0.0625 degC.
     * Returned value is sign-extended to int16_t; multiply by 0.0625
     * (or divide by 16) to obtain degrees Celsius.
     * @param temp_q4 Reference to store the signed 12-bit reading
     * @return I2cStatus indicating success or failure
     */
    I2cStatus read_temperature_q4(int16_t& temp_q4);

    /**
     * Overtemp limit (TOS) — register 0x03.
     * Asserts OS/ALERT when measured temperature meets or exceeds this value.
     * @param threshold Temperature threshold in Celsius
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_tos_limit(int8_t threshold);
    I2cStatus get_tos_limit(int8_t& threshold);

    /**
     * Hysteresis limit (THYST) — register 0x02.
     * In comparator mode OS/ALERT deasserts when temperature falls below
     * THYST; in interrupt mode it functions as the lower threshold.
     * @param threshold Temperature threshold in Celsius
     * @return I2cStatus indicating success or failure
     */
    I2cStatus set_thyst_limit(int8_t threshold);
    I2cStatus get_thyst_limit(int8_t& threshold);

    /**
     * Configuration register (0x01).
     * Use the Configuration enum bit flags to compose the value.
     */
    I2cStatus set_configuration(uint8_t configuration);
    I2cStatus get_configuration(uint8_t& configuration);

    /**
     * Trigger a single one-shot conversion.
     * Only effective when the device is in shutdown or one-shot mode (see
     * Configuration::Shutdown / Configuration::OneShotEnable). The write
     * value is ignored by the device.
     * @return I2cStatus indicating success or failure
     */
    I2cStatus trigger_one_shot();

    /**
     * Lightweight presence check.
     * NCT75 has no ID register, so we probe by reading the configuration
     * register and treating any ACK as device-present.
     * @return I2cStatus::Ok if the device ACKed, otherwise the bus status
     */
    I2cStatus probe();

private:
    /**
     * Convert a 12-bit two's-complement value left-justified in 16 bits
     * into a sign-extended int16_t in units of 0.0625 degC.
     */
    static int16_t convert_12bit_to_signed(int16_t raw_value);

    /**
     * Convert an integer Celsius value (-128..+127) into the 16-bit
     * register format used by THYST / TOS (12-bit two's complement,
     * left-justified, lower 4 bits zero).
     */
    static int16_t convert_signed_to_12bit(int8_t temp_celsius);
};

}  // namespace nv::i2c
