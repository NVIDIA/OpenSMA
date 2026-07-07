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

#include "nv/i2c/powersensor/hscc_raa22800X.h"
#include "nv/nv.h"

using namespace nv::i2c;

namespace {
// PMBus Direct Format coefficients from RAA22800X datasheet
// Formula: Real_Value = (Raw_Value × 10^(-R) - b) / m

// OT_WARN_LIMIT coefficients (unit: °C)
constexpr float    OtWarnLimitSlopeM  = 1.0f;    // m coefficient (Resolution: 1°C/LSB)
constexpr float    OtWarnLimitOffsetB = 0.0f;    // b coefficient
constexpr float    OtWarnLimitExpMult = 1.0f;    // 10^(-R), where R=0
constexpr uint16_t OtWarnLimitMask    = 0xFFFF;  // 16-bit data mask

// READ_VIN (0x88) - Input Voltage coefficients
constexpr float    VinSlopeM  = 10.0f;   // m coefficient (Resolution: 0.1V/LSB)
constexpr float    VinOffsetB = 0.0f;    // b coefficient
constexpr float    VinExpMult = 1.0f;    // 10^(-R), where R=0
constexpr uint16_t VinMask    = 0xFFFF;  // 16-bit data mask

// READ_VOUT (0x8B) - Output Voltage coefficients
constexpr float    VoutSlopeM  = 200.0f;  // m coefficient (Resolution: 5mV/LSB)
constexpr float    VoutOffsetB = 0.0f;    // b coefficient
constexpr float    VoutExpMult = 1.0f;    // 10^(-R), where R=0
constexpr uint16_t VoutMask    = 0xFFFF;  // 16-bit data mask

// READ_TEMPERATURE_1 (0x8D) - Temperature coefficients
constexpr float    TempSlopeM  = 1.0f;    // m coefficient (Resolution: 1°C/LSB)
constexpr float    TempOffsetB = 0.0f;    // b coefficient
constexpr float    TempExpMult = 1.0f;    // 10^(-R), where R=0
constexpr uint16_t TempMask    = 0xFFFF;  // 16-bit data mask

// READ_PIN (0x97) - Input Power coefficients
constexpr float    PowerSlopeM  = 5.0f;    // m coefficient (Resolution: 0.2W/LSB)
constexpr float    PowerOffsetB = 0.0f;    // b coefficient
constexpr float    PowerExpMult = 1.0f;    // 10^(-R), where R=0
constexpr uint16_t PowerMask    = 0xFFFF;  // 16-bit data mask
}  // namespace

Raa22800X::Raa22800X(Port port, uint8_t address) : PowerSensor(port, address)
{
    _ot_warn_limit_coeff = {
        OtWarnLimitSlopeM, OtWarnLimitOffsetB, OtWarnLimitExpMult, OtWarnLimitMask};
    _vin_coeff         = {VinSlopeM, VinOffsetB, VinExpMult, VinMask};
    _vout_coeff        = {VoutSlopeM, VoutOffsetB, VoutExpMult, VoutMask};
    _temp_coeff        = {TempSlopeM, TempOffsetB, TempExpMult, TempMask};
    _power_input_coeff = {PowerSlopeM, PowerOffsetB, PowerExpMult, PowerMask};
}

I2cStatus Raa22800X::init()
{
    // RAA22800X SMBALERT_MASK (1Bh) POR default = 0x00 on every STATUS page, i.e. nothing is
    // masked, so OT_WARN (STATUS_TEMPERATURE bit6) and OT_FAULT (bit7) already assert
    // SMBALERT#/nPMALERT out of reset -- no mask write is needed here. (The RAA-side NSM gap
    // was purely the HSCC_SMBUS_ALT_N polarity in GpioNsmEventSetup, which must be Low.)
    //
    // SMBALERT_MASK on the RAA is a PMBus *block* write ([1Bh][2][STATUS_cmd][mask]); if
    // explicit (non-default) masking is ever required, add a write_block() HAL primitive and
    // write {Register::StatusTemperature, 0x00} here.
    return clear_faults();
}