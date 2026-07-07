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

#include "nv/i2c/powersensor/hsc_xpd712021.h"

using namespace nv::i2c;

namespace {
// PMBus Direct Format coefficients from XPD712021 datasheet
// Formula: Real_Value = (Raw_Value × 10^(-R) - b) / m

// OT_WARN_LIMIT coefficients (unit: °C)
constexpr float    OtWarnLimitSlopeM  = 52.0f;     // m coefficient
constexpr float    OtWarnLimitOffsetB = 14321.0f;  // b coefficient (Offset: 275.4°C)
constexpr float    OtWarnLimitExpMult = 10.0f;     // 10^(-R), where R=-1
constexpr uint16_t OtWarnLimitMask    = 0xFFFF;    // 16-bit data mask

// READ_VIN (0x88) - Input Voltage coefficients
constexpr float    VinSlopeM  = 4653.0f;  // m coefficient (Resolution: ~0.215mV/LSB)
constexpr float    VinOffsetB = 0.0f;     // b coefficient
constexpr float    VinExpMult = 100.0f;   // 10^(-R), where R=-2
constexpr uint16_t VinMask    = 0xFFFF;   // 16-bit data mask

// READ_VOUT (0x8B) - Output Voltage coefficients
constexpr float    VoutSlopeM  = 4653.0f;  // m coefficient (Resolution: ~0.215mV/LSB)
constexpr float    VoutOffsetB = 0.0f;     // b coefficient
constexpr float    VoutExpMult = 100.0f;   // 10^(-R), where R=-2
constexpr uint16_t VoutMask    = 0xFFFF;   // 16-bit data mask

// READ_TEMPERATURE_1 (0x8D) - Temperature coefficients
constexpr float    TempSlopeM  = 52.0f;     // m coefficient
constexpr float    TempOffsetB = 14321.0f;  // b coefficient (Offset: 275.4°C)
constexpr float    TempExpMult = 10.0f;     // 10^(-R), where R=-1
constexpr uint16_t TempMask    = 0xFFFF;    // 16-bit data mask

// READ_PIN (0x97) - Input Power coefficients
constexpr float    PowerSlopeM  = 10527.0f;  // m coefficient
constexpr float    PowerOffsetB = 0.0f;      // b coefficient
constexpr float    PowerExpMult = 1000.0f;   // 10^(-R), where R=-3
constexpr uint16_t PowerMask    = 0xFFFF;    // 16-bit data mask
}  // namespace

Xpd712021::Xpd712021(Port port, uint8_t address) : PowerSensor(port, address)
{
    _ot_warn_limit_coeff = {
        OtWarnLimitSlopeM, OtWarnLimitOffsetB, OtWarnLimitExpMult, OtWarnLimitMask};
    _vin_coeff         = {VinSlopeM, VinOffsetB, VinExpMult, VinMask};
    _vout_coeff        = {VoutSlopeM, VoutOffsetB, VoutExpMult, VoutMask};
    _temp_coeff        = {TempSlopeM, TempOffsetB, TempExpMult, TempMask};
    _power_input_coeff = {PowerSlopeM, PowerOffsetB, PowerExpMult, PowerMask};
}

I2cStatus Xpd712021::init()
{
    // The XDP712's OT_WARN/OT_FAULT limits, SMBALERT# routing (GPO_CFG), fault/warning masks
    // and current-sense (RDS(on)) settings all live in the device NVM/MTP, programmed at
    // manufacturing via the Infineon design file (.xdp, e.g. PG558_XDP712-021) -- so unlike
    // Lm5066i/Mp5926 there is no ALERT_MASK for the FW to write here. We still clear any stale
    // boot-time latched faults for a clean status baseline.
    return clear_faults();
}
