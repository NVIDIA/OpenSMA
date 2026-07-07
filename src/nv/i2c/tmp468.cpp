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
#include "nv/i2c/tmp468.h"
#include "nv/nv.h"

using namespace nv::i2c;

Tmp468::Tmp468(Port port, uint8_t address, nv::telemetry::TelemId item)
: TempSensor(port, address, item)
{}

float Tmp468::raw_to_celsius(uint16_t raw)
{
    // Result is left-justified: bits [15:3] = 13-bit signed value (two's
    // complement), bits [2:0] = 0, LSB = 0.0625 C. Interpret as int16_t so the
    // arithmetic right shift sign-extends, then scale by the LSB resolution.
    // coverity[cert_int31_c_violation]
    const auto signed_raw = static_cast<int16_t>(raw);
    return static_cast<float>(signed_raw >> 3) * TempLsbCelsius;
}

I2cStatus Tmp468::get_local_temp(float& celsius)
{
    uint16_t   raw    = 0;
    const auto status = read_reg_16bits(Register::LocalTemp, raw);
    if (status == I2cStatus::Ok) {
        celsius = raw_to_celsius(raw);
    }
    return status;
}

I2cStatus Tmp468::get_remote_temp(uint8_t channel, float& celsius)
{
    if (!valid_channel(channel)) {
        return I2cStatus::Error;
    }
    // Remote results are contiguous: 0x01 (remote 1) .. 0x08 (remote 8).
    // coverity[cert_int31_c_violation]
    const auto reg    = static_cast<uint8_t>(Register::RemoteTempBase + channel);
    uint16_t   raw    = 0;
    const auto status = read_reg_16bits(reg, raw);
    if (status == I2cStatus::Ok) {
        celsius = raw_to_celsius(raw);
    }
    return status;
}

I2cStatus Tmp468::unlock()
{
    // datasheet 7.5.3: write UnlockKey to the Lock register so the config/limit
    // registers accept writes. The device powers up locked.
    return write_reg_16bits(Register::Lock, UnlockKey);
}

I2cStatus Tmp468::lock()
{
    // Re-protect the config/limit registers against accidental writes.
    return write_reg_16bits(Register::Lock, LockKey);
}

I2cStatus Tmp468::set_configuration(uint16_t configuration)
{
    // The TMP468 powers up write-locked; the configuration register NAKs and
    // ignores writes until the device is unlocked (datasheet 7.5.3). Unlock
    // first so the write actually takes effect.
    const auto unlock_status = unlock();
    if (unlock_status != I2cStatus::Ok) {
        return unlock_status;
    }
    return write_reg_16bits(Register::Configuration, configuration);
}

I2cStatus Tmp468::get_configuration(uint16_t& configuration)
{
    return read_reg_16bits(Register::Configuration, configuration);
}

I2cStatus Tmp468::get_therm_status(uint16_t& status)
{
    return read_reg_16bits(Register::ThermStatus, status);
}
