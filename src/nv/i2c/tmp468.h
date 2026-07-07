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
 * TMP468 Driver - TI 1 local + 8 remote channel temperature sensor.
 *
 * Scope (for now): basic register read/write and temperature conversion only.
 * This driver does NOT register into telemetry / NSM / E368 forwarding - that
 * is handled by upper layers and is intentionally out of scope here.
 *
 * All TMP468 registers are 16-bit and transmitted MSB first; use the inherited
 * read_reg_16bits() / write_reg_16bits() rather than the 8-bit accessors.
 *
 * Temperature result registers are left-justified: bits [15:3] hold a 13-bit
 * signed value (two's complement), bits [2:0] read as 0, LSB = 0.0625 C:
 *   temperature_C = (int16_t(raw) >> 3) * 0.0625
 *
 * Reference: TI TMP468 datasheet, SBOS762B.
 */
class Tmp468 : public TempSensor
{
public:
    /// Number of remote temperature channels (remote 1..8).
    static constexpr uint8_t RemoteChannelCount = 8;

    /// TI manufacturer id, read from register 0xFE (shared with TMP451/461).
    static constexpr uint8_t TiManufacturerId = 0x55;

    /// Temperature LSB resolution in Celsius.
    static constexpr float TempLsbCelsius = 0.0625f;

    /**
     * Lock register keys (datasheet 7.5.3, "Lock Register").
     *
     * The TMP468 powers up write-LOCKED: writes to the configuration and limit
     * registers are NAK'd and ignored until the device is unlocked, while reads
     * are unaffected. Write UnlockKey to Register::Lock to enable writes, or
     * LockKey to re-protect them. The lock state is volatile - it is cleared
     * only by a power cycle (not by an MCU reset), since the part has no NVM.
     */
    static constexpr uint16_t UnlockKey = 0xEB19;
    static constexpr uint16_t LockKey   = 0x5CA6;

    /**
     * TMP468 register pointer addresses (16-bit registers).
     *
     * NOTE: THERM / ALERT high-limit register addresses (needed later for
     * "Safe Temp Setup") are intentionally NOT defined yet - confirm the exact
     * addresses in the TMP468 datasheet (SBOS762B) section 7.6 before adding
     * limit get/set helpers.
     */
    enum Register : uint8_t
    {
        LocalTemp      = 0x00,  // Local temperature result
        RemoteTempBase = 0x01,  // Remote 1..8 results at 0x01..0x08
        ThermStatus    = 0x21,  // THERM limit comparator status (read-only)
        Configuration  = 0x30,  // Conversion rate / channel enable / shutdown
        Lock           = 0xC4,  // Write-lock for config/limit regs (see UnlockKey)
    };

    /**
     * Constructor
     * @param port    I2C port the TMP468 is wired to
     * @param address I2C slave address of the TMP468 (E4188: 0x48)
     * @param item    Telemetry item id (unused for the basic read/write scope)
     */
    Tmp468(Port                   port,
           uint8_t                address,
           nv::telemetry::TelemId item = nv::telemetry::TelemId::MaxItem);

    /**
     * Converted temperatures (full 0.0625 C resolution)
     */

    /// Local (board) temperature in Celsius.
    I2cStatus get_local_temp(float& celsius);

    /// Remote (diode) temperature in Celsius. channel: 0..7.
    I2cStatus get_remote_temp(uint8_t channel, float& celsius);

    /**
     * Register write-protection (datasheet 7.5.3). The device powers up locked;
     * config/limit writes are NAK'd until unlock() is called. set_configuration()
     * unlocks automatically, so callers normally do not need to call these.
     */
    I2cStatus unlock();
    I2cStatus lock();

    /**
     * Configuration (16-bit; bit fields per datasheet section 7.6, Table 8)
     */
    I2cStatus set_configuration(uint16_t configuration);
    I2cStatus get_configuration(uint16_t& configuration);

    /// Read the THERM status register (which channels exceeded their limit).
    I2cStatus get_therm_status(uint16_t& status);

    /// Convert a raw 16-bit result register value to Celsius.
    static float raw_to_celsius(uint16_t raw);

    /// True if the remote channel index is in range (0..7).
    static constexpr bool valid_channel(uint8_t channel)
    {
        return channel < RemoteChannelCount;
    }
};

}  // namespace nv::i2c
