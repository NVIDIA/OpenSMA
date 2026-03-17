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

#include "nv/i2c/powersensor/hsc_mp5926.h"
#include "nv/nv.h"
#include <climits>

using namespace nv::i2c;

Mp5926::Mp5926(Port port, uint8_t address) : PowerSensor(port, address)
{
    _ot_warn_limit_coeff = {
        OtWarnLimitSlopeM, OtWarnLimitOffsetB, OtWarnLimitExpMult, OtWarnLimitMask};
    _vin_coeff         = {VinSlopeM, VinOffsetB, VinExpMult, VinMask};
    _vout_coeff        = {VoutSlopeM, VoutOffsetB, VoutExpMult, VoutMask};
    _temp_coeff        = {TempSlopeM, TempOffsetB, TempExpMult, TempMask};
    _power_input_coeff = {PowerSlopeM, PowerOffsetB, PowerExpMult, PowerMask};
}

I2cStatus Mp5926::read_vin(uint32_t& microvolts)
{
    // Read input voltage from READ_VIN register (16-bit)
    uint16_t raw_value     = 0;
    uint16_t mfr_sys_ctrl  = 0;
    uint8_t  report_format = 0;
    auto     status        = read_reg_16bits(PowerSensor::Register::ReadVin, raw_value);
    if (status != I2cStatus::Ok) {
        return status;
    }

    status = read_reg_16bits(Register::MFR_SYS_CTRL, mfr_sys_ctrl);
    if (status != I2cStatus::Ok) {
        return status;
    }

    report_format = static_cast<uint8_t>((mfr_sys_ctrl >> MFR_SYS_CTRL_REPORT_FORMAT_SHIFT)
                                         & MFR_SYS_CTRL_REPORT_FORMAT_MASK);
    if (report_format == REPORT_FORMAT_DIRECT_MODE) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _vin_coeff.mask;

        // Apply Direct Format: V = (Y × 10^(-R) - b) / m
        const float volts = (static_cast<float>(y) * _vin_coeff.exp_mult - _vin_coeff.b)
                          / _vin_coeff.m;

        // Convert to microvolts
        microvolts = static_cast<uint32_t>(volts * VoltsToMicrovolts);
    }
    else {
        // MP5926 READ_VIN uses Linear11 format
        // Bits[15:11] = N (5-bit signed exponent)
        // Bits[10:0] = Y (11-bit signed mantissa)
        // Formula: VIN = Y × 2^N

        // Extract 5-bit exponent (bits 15:11)
        auto exponent = static_cast<int8_t>((raw_value >> LINEAR11_EXPONENT_SHIFT)
                                            & LINEAR11_EXPONENT_MASK);

        // Sign extend exponent if bit 4 is set (negative exponent)
        // coverity[cert_str34_c_violation]
        if ((exponent & LINEAR11_EXPONENT_SIGN_BIT) != 0) {
            // coverity[cert_int31_c_violation]
            exponent |= static_cast<int8_t>(LINEAR11_EXPONENT_SIGN_EXT);  // Sign extend to
                                                                          // 8-bit
        }

        // Extract 11-bit mantissa (bits 10:0)
        auto mantissa = static_cast<int16_t>(raw_value & LINEAR11_MANTISSA_MASK);

        // Sign extend mantissa if bit 10 is set (negative mantissa)
        if ((mantissa & LINEAR11_MANTISSA_SIGN_BIT) != 0) {
            // coverity[cert_int31_c_violation]
            mantissa |= static_cast<int16_t>(LINEAR11_MANTISSA_SIGN_EXT);  // Sign extend to
                                                                           // 16-bit
        }

        // Apply Linear11 formula: VIN = mantissa × 2^exponent
        float volts = 0.0f;
        // coverity[cert_str34_c_violation]
        if (exponent >= 0) {
            // Positive exponent: multiply by 2^exponent
            // coverity[cert_str34_c_violation]
            volts = static_cast<float>(mantissa * (1 << static_cast<uint32_t>(exponent)));
        }
        else {
            // Negative exponent: divide by 2^(-exponent)
            // coverity[cert_str34_c_violation]
            const int abs_exponent = -exponent;
            if (abs_exponent > 0 && abs_exponent < LINEAR11_MAX_SHIFT) {
                volts = static_cast<float>(mantissa) / static_cast<float>(1 << abs_exponent);
            }
            else {
                volts = 0.0f;  // Invalid exponent
            }
        }

        microvolts = static_cast<uint32_t>(volts * VoltsToMicrovolts);
    }

    return I2cStatus::Ok;
}

I2cStatus Mp5926::read_vout(uint32_t& microvolts)
{
    // Read output voltage from READ_VOUT register (16-bit)
    uint16_t raw_value     = 0;
    uint16_t mfr_sys_ctrl  = 0;
    uint8_t  report_format = 0;
    auto     status        = read_reg_16bits(PowerSensor::Register::ReadVout, raw_value);
    if (status != I2cStatus::Ok) {
        return status;
    }

    status = read_reg_16bits(Register::MFR_SYS_CTRL, mfr_sys_ctrl);
    if (status != I2cStatus::Ok) {
        return status;
    }

    report_format = static_cast<uint8_t>((mfr_sys_ctrl >> MFR_SYS_CTRL_REPORT_FORMAT_SHIFT)
                                         & MFR_SYS_CTRL_REPORT_FORMAT_MASK);
    if (report_format == REPORT_FORMAT_DIRECT_MODE) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _vout_coeff.mask;

        // Apply Direct Format: V = (Y × 10^(-R) - b) / m
        const float volts = (static_cast<float>(y) * _vout_coeff.exp_mult - _vout_coeff.b)
                          / _vout_coeff.m;

        // Convert to microvolts
        microvolts = static_cast<uint32_t>(volts * VoltsToMicrovolts);
    }
    else {
        // MP5926 READ_VOUT uses Linear11 format (default per datasheet)
        // Bits[15:11] = N (5-bit signed exponent)
        // Bits[10:0] = Y (11-bit signed mantissa)
        // Formula: VOUT = Y × 2^N

        // Extract 5-bit exponent (bits 15:11)
        auto exponent = static_cast<int8_t>((raw_value >> LINEAR11_EXPONENT_SHIFT)
                                            & LINEAR11_EXPONENT_MASK);

        // Sign extend exponent if bit 4 is set (negative exponent)
        // coverity[cert_str34_c_violation]
        if ((exponent & LINEAR11_EXPONENT_SIGN_BIT) != 0) {
            // coverity[cert_int31_c_violation]
            exponent |= static_cast<int8_t>(LINEAR11_EXPONENT_SIGN_EXT);  // Sign extend to
                                                                          // 8-bit
        }

        // Extract 11-bit mantissa (bits 10:0)
        auto mantissa = static_cast<int16_t>(raw_value & LINEAR11_MANTISSA_MASK);

        // Sign extend mantissa if bit 10 is set (negative mantissa)
        if ((mantissa & LINEAR11_MANTISSA_SIGN_BIT) != 0) {
            // coverity[cert_int31_c_violation]
            mantissa |= static_cast<int16_t>(LINEAR11_MANTISSA_SIGN_EXT);  // Sign extend to
                                                                           // 16-bit
        }

        // Apply Linear11 formula: VOUT = mantissa × 2^exponent
        float volts = 0.0f;
        // coverity[cert_str34_c_violation]
        if (exponent >= 0) {
            // Positive exponent: multiply by 2^exponent
            // coverity[cert_str34_c_violation]
            volts = static_cast<float>(mantissa * (1 << static_cast<uint32_t>(exponent)));
        }
        else {
            // Negative exponent: divide by 2^(-exponent)
            // coverity[cert_str34_c_violation]
            const int abs_exponent = -exponent;
            if (abs_exponent > 0 && abs_exponent < LINEAR11_MAX_SHIFT) {
                volts = static_cast<float>(mantissa) / static_cast<float>(1 << abs_exponent);
            }
            else {
                volts = 0.0f;  // Invalid exponent
            }
        }

        microvolts = static_cast<uint32_t>(volts * VoltsToMicrovolts);
    }

    return I2cStatus::Ok;
}

I2cStatus Mp5926::read_temperature(uint16_t& temperature)
{
    // Read temperature from READ_TEMPERATURE register (16-bit)
    uint16_t raw_value     = 0;
    uint16_t mfr_sys_ctrl  = 0;
    uint8_t  report_format = 0;
    auto     status        = read_reg_16bits(PowerSensor::Register::ReadTemperature, raw_value);
    if (status != I2cStatus::Ok) {
        return status;
    }

    status = read_reg_16bits(Register::MFR_SYS_CTRL, mfr_sys_ctrl);
    if (status != I2cStatus::Ok) {
        return status;
    }

    report_format = static_cast<uint8_t>((mfr_sys_ctrl >> MFR_SYS_CTRL_REPORT_FORMAT_SHIFT)
                                         & MFR_SYS_CTRL_REPORT_FORMAT_MASK);
    if (report_format == REPORT_FORMAT_DIRECT_MODE) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _temp_coeff.mask;

        // Apply Direct Format: T = (Y × 10^(-R) - b) / m
        const float temp_celsius = (static_cast<float>(y) * _temp_coeff.exp_mult
                                    - _temp_coeff.b)
                                 / _temp_coeff.m;

        temperature = static_cast<uint16_t>(temp_celsius);
    }
    else {
        // MP5926 READ_TEMPERATURE uses Linear11 format
        // Bits[15:11] = N (5-bit signed exponent)
        // Bits[10:0] = Y (11-bit signed mantissa)
        // Formula: TEMPERATURE = Y × 2^N

        // Extract 5-bit exponent (bits 15:11)
        auto exponent = static_cast<int8_t>((raw_value >> LINEAR11_EXPONENT_SHIFT)
                                            & LINEAR11_EXPONENT_MASK);

        // Sign extend exponent if bit 4 is set (negative exponent)
        // coverity[cert_str34_c_violation]
        if ((exponent & LINEAR11_EXPONENT_SIGN_BIT) != 0) {
            // coverity[cert_int31_c_violation]
            exponent |= static_cast<int8_t>(LINEAR11_EXPONENT_SIGN_EXT);  // Sign extend to
                                                                          // 8-bit
        }

        // Extract 11-bit mantissa (bits 10:0)
        auto mantissa = static_cast<int16_t>(raw_value & LINEAR11_MANTISSA_MASK);

        // Sign extend mantissa if bit 10 is set (negative mantissa)
        if ((mantissa & LINEAR11_MANTISSA_SIGN_BIT) != 0) {
            // coverity[cert_int31_c_violation]
            mantissa |= static_cast<int16_t>(LINEAR11_MANTISSA_SIGN_EXT);  // Sign extend to
                                                                           // 16-bit
        }

        // Apply Linear11 formula: TEMPERATURE = mantissa × 2^exponent
        float temp_celsius = 0.0f;
        // coverity[cert_str34_c_violation]
        if (exponent >= 0) {
            // Positive exponent: multiply by 2^exponent
            temp_celsius = static_cast<float>(mantissa
                                              // coverity[cert_str34_c_violation]
                                              * (1 << static_cast<uint32_t>(exponent)));
        }
        else {
            // Negative exponent: divide by 2^(-exponent)
            // coverity[cert_str34_c_violation]
            const int abs_exponent = -exponent;
            if (abs_exponent > 0 && abs_exponent < LINEAR11_MAX_SHIFT) {
                temp_celsius = static_cast<float>(mantissa)
                             / static_cast<float>(1 << abs_exponent);
            }
            else {
                temp_celsius = 0.0f;  // Invalid exponent
            }
        }

        temperature = static_cast<uint16_t>(temp_celsius);
    }

    return I2cStatus::Ok;
}

I2cStatus Mp5926::read_input_power(uint32_t& milliwatts)
{
    // Read input power from READ_INPUT_POWER register (16-bit)
    uint16_t raw_value     = 0;
    uint16_t mfr_sys_ctrl  = 0;
    uint8_t  report_format = 0;
    auto     status        = read_reg_16bits(PowerSensor::Register::ReadInputPower, raw_value);
    if (status != I2cStatus::Ok) {
        return status;
    }

    status = read_reg_16bits(Register::MFR_SYS_CTRL, mfr_sys_ctrl);
    if (status != I2cStatus::Ok) {
        return status;
    }

    report_format = static_cast<uint8_t>((mfr_sys_ctrl >> MFR_SYS_CTRL_REPORT_FORMAT_SHIFT)
                                         & MFR_SYS_CTRL_REPORT_FORMAT_MASK);
    if (report_format == REPORT_FORMAT_DIRECT_MODE) {
        // Extract valid bits using subclass-specific mask
        const uint16_t y = raw_value & _power_input_coeff.mask;

        // Apply Direct Format: P(W) = (Y × 10^(-R) - b) / m
        const float watts = (static_cast<float>(y) * _power_input_coeff.exp_mult
                             - _power_input_coeff.b)
                          / _power_input_coeff.m;

        // Convert to milliwatts
        milliwatts = static_cast<uint32_t>(watts * 1000.0f);
    }
    else {
        // MP5926 READ_PIN uses Linear11 format
        // Bits[15:11] = N (5-bit signed exponent)
        // Bits[10:0] = Y (11-bit signed mantissa)
        // Formula: input power = Y × 2^N

        // Extract 5-bit exponent (bits 15:11)
        // coverity[cert_str34_c_violation]
        auto exponent = static_cast<int8_t>((raw_value >> LINEAR11_EXPONENT_SHIFT)
                                            & LINEAR11_EXPONENT_MASK);

        // Sign extend exponent if bit 4 is set (negative exponent)
        // coverity[cert_str34_c_violation]
        if ((exponent & LINEAR11_EXPONENT_SIGN_BIT) != 0) {
            // coverity[cert_int31_c_violation]
            exponent |= static_cast<int8_t>(LINEAR11_EXPONENT_SIGN_EXT);  // Sign extend to
                                                                          // 8-bit
        }

        // Extract 11-bit mantissa (bits 10:0)
        auto mantissa = static_cast<int16_t>(raw_value & LINEAR11_MANTISSA_MASK);

        // Sign extend mantissa if bit 10 is set (negative mantissa)
        if ((mantissa & LINEAR11_MANTISSA_SIGN_BIT) != 0) {
            // coverity[cert_int31_c_violation]
            mantissa |= static_cast<int16_t>(LINEAR11_MANTISSA_SIGN_EXT);  // Sign extend to
                                                                           // 16-bit
        }

        // Apply Linear11 formula: input power = mantissa × 2^exponent
        float watts = 0.0f;
        // coverity[cert_str34_c_violation]
        if (exponent >= 0) {
            // Positive exponent: multiply by 2^exponent
            // coverity[cert_str34_c_violation]
            watts = static_cast<float>(mantissa * (1 << static_cast<uint32_t>(exponent)));
        }
        else {
            // Negative exponent: divide by 2^(-exponent)
            // coverity[cert_str34_c_violation]
            const int abs_exponent = -exponent;
            if (abs_exponent > 0 && abs_exponent < LINEAR11_MAX_SHIFT) {
                watts = static_cast<float>(mantissa) / static_cast<float>(1 << abs_exponent);
            }
            else {
                watts = 0.0f;  // Invalid exponent
            }
        }

        milliwatts = static_cast<uint32_t>(watts * WattsToMilliwatts);
    }

    return I2cStatus::Ok;
}
