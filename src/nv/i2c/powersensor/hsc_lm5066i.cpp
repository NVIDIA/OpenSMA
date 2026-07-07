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

#include "nv/i2c/powersensor/hsc_lm5066i.h"
#include "nv/nv.h"
#include <climits>

using namespace nv::i2c;

namespace {
// PMBus Direct Format coefficients from LM5066I datasheet Table 47
// Formula: Real_Value = (Raw_Value × 10^(-R) - b) / m

// OT_WARN_LIMIT (0x51) - Over Temperature Warning Limit coefficients
constexpr float    OtWarnLimitSlopeM  = 16000.0f;  // m coefficient
constexpr float    OtWarnLimitOffsetB = 0.0f;      // b coefficient
constexpr float    OtWarnLimitExpMult = 1000.0f;   // 10^(-R), where R=-3
constexpr uint16_t OtWarnLimitMask    = 0x0FFF;    // 12-bit data mask

// READ_VIN (0x88) - Input Voltage coefficients
constexpr float    VinSlopeM  = 4617.0f;  // m coefficient
constexpr float    VinOffsetB = -140.0f;  // b coefficient
constexpr float    VinExpMult = 100.0f;   // 10^(-R), where R=-2
constexpr uint16_t VinMask    = 0x0FFF;   // 12-bit data mask

// READ_VOUT (0x8B) - Output Voltage coefficients
constexpr float    VoutSlopeM  = 4602.0f;  // m coefficient
constexpr float    VoutOffsetB = 500.0f;   // b coefficient
constexpr float    VoutExpMult = 100.0f;   // 10^(-R), where R=-2
constexpr uint16_t VoutMask    = 0x0FFF;   // 12-bit data mask

// READ_TEMPERATURE_1 (0x8D) - Temperature coefficients
constexpr float    TempSlopeM  = 16000.0f;  // m coefficient
constexpr float    TempOffsetB = 0.0f;      // b coefficient
constexpr float    TempExpMult = 1000.0f;   // 10^(-R), where R=-3
constexpr uint16_t TempMask    = 0x0FFF;    // 12-bit data mask

// READ_PIN (0x97) - Input Power coefficients for 1mOhm sense resistor, CL tied to VDD.
constexpr float    PowerSlope1MOhmClVddM  = 1701.0f;   // m coefficient
constexpr float    PowerOffset1MOhmClVddB = -4000.0f;  // b coefficient
constexpr float    PowerExpMult           = 1000.0f;   // 10^(-R), where R=-3
constexpr uint16_t PowerMask              = 0x0FFF;    // 12-bit data mask

}  // namespace

namespace nv::i2c::power {
__attribute__((weak)) uint8_t lm5066i_ot_fault_limit_celsius()
{
    return 0;
}
__attribute__((weak)) uint8_t lm5066i_ot_warn_limit_celsius()
{
    return 0;
}
}  // namespace nv::i2c::power

Lm5066i::Lm5066i(Port port, uint8_t address) : PowerSensor(port, address)
{
    _ot_warn_limit_coeff = {
        OtWarnLimitSlopeM, OtWarnLimitOffsetB, OtWarnLimitExpMult, OtWarnLimitMask};
    _vin_coeff         = {VinSlopeM, VinOffsetB, VinExpMult, VinMask};
    _vout_coeff        = {VoutSlopeM, VoutOffsetB, VoutExpMult, VoutMask};
    _temp_coeff        = {TempSlopeM, TempOffsetB, TempExpMult, TempMask};
    _power_input_coeff = {
        PowerSlope1MOhmClVddM, PowerOffset1MOhmClVddB, PowerExpMult, PowerMask};
}

I2cStatus Lm5066i::write_ot_fault_limit(uint8_t limit)
{
    // OT_FAULT_LIMIT and OT_WARN_LIMIT use the same coefficients.
    const float raw_value = (static_cast<float>(limit) * _ot_warn_limit_coeff.m
                             + _ot_warn_limit_coeff.b)
                          / _ot_warn_limit_coeff.exp_mult;
    return write_reg_16bits(Register::OtFaultLimit,
                            static_cast<uint16_t>(raw_value) & _ot_warn_limit_coeff.mask);
}

I2cStatus Lm5066i::init()
{
    // Unmask OT warning (bit10) and OT fault (bit2) in ALERT_MASK (D8h) so an over-temperature
    // condition asserts SMBA. D8h is 1=mask / 0=assert, so CLEAR those bits (LM5066i default
    // FD04h leaves OT-warn masked). Read-modify-write to preserve the other mask bits.
    {
        constexpr uint8_t  AlertMaskReg = 0xD8;
        constexpr uint16_t OtMaskBits   = (1u << 10) | (1u << 2);  // OT_WARN, OT_FAULT
        uint16_t           mask         = 0;
        I2cStatus          st           = read_reg_16bits(AlertMaskReg, mask);
        if (st != I2cStatus::Ok) {
            return st;
        }
        st = write_reg_16bits(AlertMaskReg, static_cast<uint16_t>(mask & ~OtMaskBits));
        if (st != I2cStatus::Ok) {
            return st;
        }
    }

    // Per-project limits; 0 means "not opted in", leave the LM5066i hardware default.
    const uint8_t ot_fault_c = nv::i2c::power::lm5066i_ot_fault_limit_celsius();
    if (ot_fault_c != 0) {
        const I2cStatus status = write_ot_fault_limit(ot_fault_c);
        if (status != I2cStatus::Ok) {
            return status;
        }
    }

    const uint8_t ot_warn_c = nv::i2c::power::lm5066i_ot_warn_limit_celsius();
    if (ot_warn_c != 0) {
        const I2cStatus status = write_ot_warn_limit(ot_warn_c);
        if (status != I2cStatus::Ok) {
            return status;
        }
    }

    // Clear any stale boot-time latched faults for a clean status baseline.
    return clear_faults();
}
