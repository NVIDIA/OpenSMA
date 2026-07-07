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

#include "nv/i2c/powersensor/hscc_mp29540.h"

using namespace nv::i2c;

namespace {
// PMBus Direct Format coefficients from MP29540 datasheet
// Formula: Real_Value = (Raw_Value × 10^(-R) - b) / m

// OT_WARN_LIMIT coefficients (unit: °C)
constexpr float    OtWarnLimitSlopeM  = 1.0f;    // m coefficient (Resolution: 1°C/LSB)
constexpr float    OtWarnLimitOffsetB = 0.0f;    // b coefficient
constexpr float    OtWarnLimitExpMult = 1.0f;    // 10^(-R), where R=0
constexpr uint16_t OtWarnLimitMask    = 0x00FF;  // 8-bit data mask

// READ_VIN (0x88) - Input Voltage coefficients
constexpr float    VinSlopeM  = 8.0f;    // m coefficient (Resolution: 0.125V/LSB)
constexpr float    VinOffsetB = 0.0f;    // b coefficient
constexpr float    VinExpMult = 1.0f;    // 10^(-R), where R=0
constexpr uint16_t VinMask    = 0x07FF;  // 11-bit data mask
}  // namespace

Mp29540::Mp29540(Port port, uint8_t address) : PowerSensor(port, address)
{
    _ot_warn_limit_coeff = {
        OtWarnLimitSlopeM, OtWarnLimitOffsetB, OtWarnLimitExpMult, OtWarnLimitMask};
    _vin_coeff = {VinSlopeM, VinOffsetB, VinExpMult, VinMask};
}

I2cStatus Mp29540::init()
{
    // SMBALERT_MASK1 (1Bh) lives on Page 0 and is a 16-bit field where each bit ENABLES a
    // source to assert ALT_P# (1 = no mask / assert, 0 = masked -- note this is inverted vs.
    // standard PMBus). Unmask the over-temperature warning (bit10, OT_WARN_MASK) and
    // over-temperature protection/fault (bit11, OTP_MASK) so a thermal event pulls ALT_P# ->
    // HSCC_SMBUS_ALT_N -> NSM GPIO event.
    constexpr uint16_t OtWarnUnmask = 1u << 10;  // OT_WARN_MASK
    constexpr uint16_t OtpUnmask    = 1u << 11;  // OTP_MASK (OT fault)
    // SMBALERT_MASK1 (1Bh) is MP29540-specific. HSC parts mask alerts at a different register
    // (ALERT_MASK 0xD8), so this address is intentionally NOT in the shared
    // PowerSensor::Register enum.
    constexpr uint8_t SmbAlertMask1Reg = 0x1B;

    uint8_t original_page = 0;
    auto    status        = get_current_page(original_page);
    if (status != I2cStatus::Ok) {
        return status;
    }

    status = switch_to_page(Page0);
    if (status != I2cStatus::Ok) {
        return status;
    }

    uint16_t mask = 0;
    status        = read_reg_16bits(SmbAlertMask1Reg, mask);
    if (status == I2cStatus::Ok) {
        mask   |= (OtWarnUnmask | OtpUnmask);
        status  = write_reg_16bits(SmbAlertMask1Reg, mask);
    }

    // Always restore the caller's page, even if the mask read/write failed.
    const I2cStatus restore_status = switch_to_page(original_page);
    if (status != I2cStatus::Ok) {
        return status;
    }
    if (restore_status != I2cStatus::Ok) {
        return restore_status;
    }

    // Clear any stale boot-time latched faults for a clean status baseline.
    return clear_faults();
}

I2cStatus Mp29540::read_vout(uint32_t& microvolts)
{
    // 1. Calculate VOUT_DIVIDER using helper function (handles all page switching)
    float vout_divider = 0.0f;
    auto  status       = calculate_vout_divider(vout_divider);
    if (status != I2cStatus::Ok) {
        return status;
    }

    // 2. Read VID_STEP and VOUT values from Page 0 (current page should be restored by helper)
    uint16_t vid_step_raw = 0;
    uint16_t vout_raw     = 0;
    status                = read_reg_16bits(Register::VidStep, vid_step_raw);  // VID_STEP
    if (status != I2cStatus::Ok) {
        return status;
    }

    status = read_reg_16bits(PowerSensor::Register::ReadVout, vout_raw);  // READ_VOUT
    if (status != I2cStatus::Ok) {
        return status;
    }

    // 3. Perform calculations according to MP29540 datasheet
    // READ_VOUT_PMBUS (8Bh) Format: VID
    // The READ_VOUT_PMBUS command on Page 0 returns rail 1's VOUT PMBus report value
    // Bits 15:12 = RESERVED, Bits 11:0 = READ_VOUT
    // Formula from datasheet: VOUT = READ_VOUT (8Bh) × VID_STEP/Vout_divider
    // Where VID_STEP is determined by 29h (on Page 0), bits[12:10]

    // Extract 12-bit VOUT value (bits 11:0) from READ_VOUT register
    const uint16_t vout_value = vout_raw & VoutValueMask;

    // Extract VID_STEP (bits 12:10) from VID_STEP register (29h on Page 0)
    const uint16_t vid_step = (vid_step_raw >> VidStepShift) & VidStepMask;

    // Apply MP29540 VID formula from datasheet:
    // VOUT = READ_VOUT × VID_STEP / VOUT_DIVIDER
    // Note: VID_STEP encoding from bits[12:10] determines the step size
    const auto vid_step_value = static_cast<float>(vid_step);  // VID_STEP from register 29h

    // Calculate final voltage using the datasheet formula
    const auto voltage_volts = static_cast<float>(vout_value) * vid_step_value / vout_divider;

    // Convert to microvolts (µV)
    microvolts = static_cast<uint32_t>(voltage_volts * VoltsToMicrovolts);

    return I2cStatus::Ok;
}

I2cStatus Mp29540::read_temperature(uint16_t& temperature)
{
    // Read temperature from READ_TEMPERATURE register (16-bit)
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(PowerSensor::Register::ReadTemperature, raw_value);

    if (status == I2cStatus::Ok) {
        // MPS MP29540 READ_TEMPERATURE_PMBUS conversion from datasheet
        // Format: Linear11
        // Formula: Temperature (°C) = Mantissa × 2^Exponent
        // Resolution: 1°C/LSB
        // Bits 15:11 = Exponent (5-bit signed), Bits 10:0 = Mantissa (11-bit signed)

        // Linear11 format bit field constants

        // Extract 5-bit exponent (bits 15:11) and handle sign extension
        auto exponent = static_cast<int8_t>((raw_value >> ExponentShift) & ExponentMask);
        // Intentional: Safe integer promotion for sign bit check in Linear11 format
        // coverity[cert_str34_c_violation]
        if ((exponent & ExponentSignBit) != 0) {  // If bit 4 = 1 (negative)
            // Intentional: Sign extension for Linear11 format per MP29540 datasheet
            // ExponentSignExtend (0xE0) correctly sets upper bits for negative exponents
            // coverity[cert_int31_c_violation]
            exponent |= static_cast<int8_t>(ExponentSignExtend);  // Sign extend to 8-bit
                                                                  // signed
        }

        // Extract 11-bit mantissa (bits 10:0) and handle sign extension
        auto mantissa = static_cast<int16_t>(raw_value & MantissaMask);
        if ((mantissa & MantissaSignBit) != 0) {  // If bit 10 = 1 (negative)
            // Intentional: Sign extension for Linear11 format per MP29540 datasheet
            // MantissaSignExtend (0xF800) correctly sets upper bits for negative mantissa
            // coverity[cert_int31_c_violation]
            mantissa |= static_cast<int16_t>(MantissaSignExtend);  // Sign extend to 16-bit
                                                                   // signed
        }

        // Apply Linear11 formula: Temperature = mantissa × 2^exponent
        float temp_celsius = 0.0f;
        if (exponent >= ZeroExponent) {
            // Intentional: Safe widening conversion from int8_t exponent to int for shift
            // Exponent range is 0 to 15, well within int bounds for shift operation
            // coverity[cert_str34_c_violation]
            temp_celsius = static_cast<float>(mantissa * (1 << exponent));  // 2^exponent for
                                                                            // positive exponent
        }
        else {
            // For negative exponents, calculate 2^(-exponent) by using absolute value
            // 5-bit signed exponent range is -16 to +15, so abs_exponent range is 1 to 16
            // coverity[cert_str34_c_violation]
            const int abs_exponent = -exponent;
            // Defensive check: ensure shift amount is within valid range to avoid undefined
            // behavior
            if (abs_exponent > 0 && abs_exponent < 32) {
                temp_celsius = static_cast<float>(mantissa)
                             / static_cast<float>(1 << abs_exponent);
            }
            else {
                temp_celsius = 0.0f;  // Invalid exponent, set to safe default
            }
        }

        // Convert to unsigned integer (°C)
        temperature = static_cast<uint16_t>(temp_celsius);
    }

    return status;
}

I2cStatus Mp29540::read_input_power(uint32_t& milliwatts)
{
    // Read estimated input power from READ_PIN_EST register (16-bit)
    uint16_t raw_value = 0;
    auto     status    = read_reg_16bits(Register::ReadPinEst, raw_value);

    if (status == I2cStatus::Ok) {
        // MPS MP29540 READ_PIN_EST_PMBUS conversion from datasheet
        // Format: Unsigned Binary
        // Formula: PIN (W) = READ_PIN_EST_PMBUS × 2 × 2^EXPONENT
        // Bits 15:11 = EXPONENT, Bits 10:0 = READ_PIN_EST_PMBUS

        // Power calculation constants

        // Extract EXPONENT (bits 15:11) and power value (bits 10:0)
        const uint8_t  exponent      = (raw_value >> PowerExponentShift) & PowerExponentMask;
        const uint16_t pin_est_value = raw_value & PowerValueMask;

        // Apply datasheet formula: PIN (W) = pin_est_value × 2 × 2^exponent
        const auto power_watts = static_cast<float>(pin_est_value) * PowerMultiplier
                               * static_cast<float>(1U << exponent);  // 2^exponent

        // Convert to milliwatts (mW)
        milliwatts = static_cast<uint32_t>(power_watts * WattsToMilliwatts);
    }

    return status;
}

I2cStatus Mp29540::switch_to_page(uint8_t page_number)
{
    // Write to PAGE register (0x00) to switch pages
    return write_reg(Register::Page, page_number);
}

I2cStatus Mp29540::get_current_page(uint8_t& current_page)
{
    // Read current page from PAGE register (0x00)
    return read_reg(Register::Page, current_page);
}

I2cStatus Mp29540::calculate_vout_divider(float& vout_divider)
{
    uint8_t original_page = 0;

    // 1. Save current page
    auto status = get_current_page(original_page);
    if (status != I2cStatus::Ok) {
        return status;
    }

    // 2. Switch to Page 1 to read resistor values
    status = switch_to_page(Page1);
    if (status != I2cStatus::Ok) {
        return status;
    }

    uint16_t rbot_raw = 0;
    uint16_t rtop_raw = 0;
    status            = read_reg_16bits(Register::MfrVoutRbot, rbot_raw);  // MFR_VOUT_RBOT
    if (status != I2cStatus::Ok) {
        switch_to_page(original_page);  // Try to restore page
        return status;
    }

    status = read_reg_16bits(Register::MfrVoutRtop, rtop_raw);  // MFR_VOUT_RTOP
    if (status != I2cStatus::Ok) {
        switch_to_page(original_page);  // Try to restore page
        return status;
    }

    // Extract resistor values from RBOT and RTOP registers
    // RBOT: bits 11:0 (12-bit), bits 15:12 are reserved
    // RTOP: bits 14:0 (15-bit), bit 15 is reserved
    const uint16_t rbot = rbot_raw & RbotValueMask;  // 12-bit value (0x0FFF)
    const uint16_t rtop = rtop_raw & RtopValueMask;  // 15-bit value (0x7FFF)

    // 3. Switch back to original page
    status = switch_to_page(original_page);
    if (status != I2cStatus::Ok) {
        return status;
    }

    // 4. Calculate VOUT_DIVIDER = MFR_VOUT_RBOT / (MFR_VOUT_RBOT + MFR_VOUT_RTOP)
    // This represents the voltage divider ratio from the external resistor network
    if ((rbot + rtop) == ZeroDivisor) {
        return I2cStatus::Error;  // Avoid division by zero
    }

    vout_divider = static_cast<float>(rbot) / static_cast<float>(rbot + rtop);

    if (vout_divider == ZeroDivider) {
        return I2cStatus::Error;  // Avoid division by zero
    }

    return I2cStatus::Ok;
}

I2cStatus Mp29540::read_ot_warn_limit(uint8_t& limit)
{
    uint8_t original_page = 0;

    // Save current page
    auto status = get_current_page(original_page);
    if (status != I2cStatus::Ok) {
        return status;
    }

    // Switch to Page 0 to read resistor values
    status = switch_to_page(Page0);
    if (status != I2cStatus::Ok) {
        return status;
    }

    uint16_t raw_value = 0;
    status             = read_reg_16bits(PowerSensor::Register::OtWarnLimit, raw_value);

    if (status != I2cStatus::Ok) {
        switch_to_page(original_page);
        return status;
    }
    // Extract valid bits using subclass-specific mask
    const uint16_t y = raw_value & _ot_warn_limit_coeff.mask;

    // Apply Direct Format: T = (Y × 10^(-R) - b) / m
    const float temp_celsius = (static_cast<float>(y) * _ot_warn_limit_coeff.exp_mult
                                - _ot_warn_limit_coeff.b)
                             / _ot_warn_limit_coeff.m;

    limit = static_cast<uint8_t>(temp_celsius);

    // Switch back to original page
    status = switch_to_page(original_page);

    return status;
}

I2cStatus Mp29540::write_ot_warn_limit(uint8_t limit)
{
    uint8_t original_page = 0;

    // Save current page
    auto status = get_current_page(original_page);
    if (status != I2cStatus::Ok) {
        return status;
    }

    // Switch to Page 0 to write resistor values
    status = switch_to_page(Page0);
    if (status != I2cStatus::Ok) {
        return status;
    }

    const float temp_celsius = (static_cast<float>(limit) * _ot_warn_limit_coeff.m
                                + _ot_warn_limit_coeff.b)
                             / _ot_warn_limit_coeff.exp_mult;
    const uint16_t raw_value = static_cast<uint16_t>(temp_celsius) & _ot_warn_limit_coeff.mask;

    status = write_reg_16bits(PowerSensor::Register::OtWarnLimit, raw_value);
    if (status != I2cStatus::Ok) {
        switch_to_page(original_page);
        return status;
    }

    // Switch back to original page
    status = switch_to_page(original_page);

    return status;
}
