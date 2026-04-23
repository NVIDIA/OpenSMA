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

#include NV_IPC_CONFIG_H
#include "nv/common/fixed_point.h"
#include "nv/soc_pwr_smoothing/mpf.h"
#include "sys/gpio/driver.h"
#include "sys/adc/adc.h"
#include "sys/dac/dac.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace nv::soc_pwr_smoothing {

template<uint32_t port, uint32_t pin, bool active_low = true>
class GpioOutDev : public mpf::FuncBlock
{
public:
    struct Ports
    {
        mpf::port::In<bool> assert;  // True to assert, false to deassert
    };

    static void evaluate(const Ports& ports)
    {
        // For active-low: assert=true → GPIO=LOW, assert=false → GPIO=HIGH
        // For active-high: assert=true → GPIO=HIGH, assert=false → GPIO=LOW
        const bool set_high = active_low ? !ports.assert : ports.assert;
        sys::gpio::Driver::write<port, pin>(set_high);
    }
};

template<uint32_t port, uint32_t pin, bool active_low = true>
class GpioInDev : public mpf::FuncBlock
{
public:
    struct Ports
    {
        mpf::port::Out<bool> assert;  // True when asserted, false when deasserted
    };

    static void evaluate(Ports ports)
    {
        bool pin_level = sys::gpio::Driver::read<port, pin>();
        ports.assert   = active_low ? !pin_level : pin_level;
    }
};

// Pops a result from the SoC ADC FIFO and converts it to both percentage and voltage.
// Note that it is the caller's responsibility to ensure that there is a result in
// the FIFO that can be popped.
class StateOfChargeDev : public mpf::FuncBlock
{
public:
    struct Ports
    {
        // The state of charge/percentage of energy
        mpf::port::Out<SFXP22_10> percent;
        // The state of charge voltage in volts
        mpf::port::Out<SFXP22_10> soc_voltage_V;
        // Raw ADC code (for calibration)
        mpf::port::Out<uint16_t> adc_raw;
    };

    static void evaluate(Ports ports)
    {
        const uint16_t
            raw_adc = sys::adc::ADC::pop_fifo_conv_value<SocAdcPeripheral, SocAdcFifoNum>();

        // Expose raw ADC code for calibration
        ports.adc_raw = raw_adc;

        // Convert ADC raw to rack voltage in volts
        ports.soc_voltage_V = adc_raw_to_voltage(raw_adc);

        // Convert rack voltage to SoC percentage
        ports.percent = soc_voltage_to_percent(ports.soc_voltage_V);
    }

    // Converts rack voltage (in volts) to SoC percentage.
    // Linear mapping: 1.0V = 0%, 7.0V = 100%
    // Voltage below 1.0V or above 7.0V is clamped to [0%, 100%]
    // Made public to support override logic in PowerManager
    static SFXP22_10 soc_voltage_to_percent(SFXP22_10 v_rack_V)
    {
        // *** Compile time calculation *** //

        constexpr SFXP22_10 v_min_fp = to_sfxp22_10(SocVoltageMin);  // 1.0 V

        // Pre-compute percent per volt (eliminates runtime division)
        // = 100% / voltage_range = 100 / (7.0 - 1.0) = 16.6667 %/V
        constexpr float     percent_per_volt    = 100.0f / (SocVoltageMax - SocVoltageMin);
        constexpr SFXP22_10 percent_per_volt_fp = to_sfxp22_10(percent_per_volt);

        constexpr SFXP22_10 percent_0_fp   = to_sfxp22_10(0);
        constexpr SFXP22_10 percent_100_fp = to_sfxp22_10(100);

        // Verify no 32-bit overflow for multiplication (v_offset × percent_per_volt_fp)
        // Max v_offset = 6.0V (6144 in SFXP22_10) × percent_per_volt (17067 in SFXP22_10)
        // Product before >>10 shift: ~104M, well within int32 range (2.1B max)
        // This allows sfxp22_10_multiply() to use fast 32-bit multiplication
        constexpr SFXP22_10 v_max_offset_fp = to_sfxp22_10(SocVoltageMax - SocVoltageMin);
        static_assert(no_s32_multiply_overflow(v_max_offset_fp, percent_per_volt_fp),
                      "Percentage multiplication overflow - voltage range too large");

        // *** Runtime calculation *** //

        // Calculate percentage: v_offset × percent_per_volt
        SFXP22_10 v_offset   = v_rack_V - v_min_fp;
        SFXP22_10 percentage = sfxp22_10_multiply(v_offset, percent_per_volt_fp);

        // Clamp to [0%, 100%] after calculation to ensure constant execution time
        return std::clamp(percentage, percent_0_fp, percent_100_fp);
    }

protected:
    // Converts raw ADC value to rack voltage in volts.
    // Combines ADC conversion and voltage divider scaling into a single operation.
    static SFXP22_10 adc_raw_to_voltage(uint16_t adc_raw)
    {
        // *** Compile time calculation *** //

        constexpr uint32_t resolution = sys::adc::ADC::resolution_bits(SocAdcHiResMode, true);
        constexpr int32_t  max_adc_value = (1U << resolution) - 1;  // 65535 (actual max)
        constexpr int32_t  adc_divisor = (1U << resolution);  // 65536 (for fast shift division)

        // Voltage divider inverse ratio: (R1 + R2) / R2
        // This converts the voltage measured at ADC back to the original rack voltage
        constexpr float divider_inverse_ratio = static_cast<float>(SocVoltageDividerR1
                                                                   + SocVoltageDividerR2)
                                              / static_cast<float>(SocVoltageDividerR2);

        // Base voltage scale: V_ref × divider_inverse_ratio (in volts)
        constexpr float voltage_scale_base = SocAdcRefVoltageV * divider_inverse_ratio;

        // Apply correction factor to compensate for using 2^n division instead of (2^n - 1)
        // This allows fast bit-shift division while maintaining accuracy
        constexpr float correction_factor = static_cast<float>(adc_divisor)
                                          / static_cast<float>(max_adc_value);
        constexpr float     voltage_scale_V  = voltage_scale_base * correction_factor;
        constexpr SFXP22_10 voltage_scale_fp = to_sfxp22_10(voltage_scale_V);

        // Verify no 32-bit overflow for multiplication (adc_raw × voltage_scale_fp)
        static_assert(no_s32_multiply_overflow(voltage_scale_fp, max_adc_value),
                      "ADC multiplication overflow - voltage scale too large");

        // *** Runtime calculation *** //

        // Fast bit-shift division: v_rack_V = (adc_raw × voltage_scale) / 2^resolution
        // The corrected voltage_scale ensures this gives the same result as dividing by (2^n -
        // 1)
        SFXP22_10 v_rack_V = (adc_raw * voltage_scale_fp) >> resolution;

        return v_rack_V;
    }
};

// Set a DAC to output a percentage of a maximum voltage. It is the caller's
// responsibility to ensure that the percentage passed is at most 100%
template<sys::dac::Dac::Peripheral peripheral>
class DacPercentDev : public mpf::FuncBlock
{
public:
    struct Ports
    {
        mpf::port::In<SFXP22_10> percent;
        // When true, raw_override_dac is written to the DAC instead of
        // percent_to_dac_raw(percent)
        mpf::port::In<bool>     raw_override_enabled;
        mpf::port::In<uint16_t> raw_override_dac;
        // Last code passed to Dac::set (12-bit effective; shadow, not bus readback)
        mpf::port::Out<uint16_t> last_dac_raw;
    };

    static void evaluate(Ports ports)
    {
        const uint32_t dac_raw = ports.raw_override_enabled
                                   ? static_cast<uint32_t>(ports.raw_override_dac)
                                   : percent_to_dac_raw(ports.percent);
        sys::dac::Dac::set(dac_raw, peripheral);
        constexpr uint32_t dac_mask = (1U << sys::dac::Dac::ResolutionBits) - 1U;
        ports.last_dac_raw          = static_cast<uint16_t>(dac_raw & dac_mask);
    }

protected:
    // Converts given percentage (0-100%) to a DAC raw value, scaling the output voltage
    // between [0V, OvrmMaxDacOutputV] rather than the full DAC range [0V, max_output_V].
    static uint32_t percent_to_dac_raw(SFXP22_10 percent)
    {
        // *** Compile time calculation *** //

        constexpr float max_output_V = sys::dac::Dac::MaxVoltage_mV / 1000.0f;
        static_assert(OvrmMaxDacOutputV <= max_output_V);

        constexpr uint32_t dac_steps = 1U << sys::dac::Dac::ResolutionBits;

        // Pre-compute maximum DAC raw value that outputs OvrmMaxDacOutputV
        // e.g., for 2.189V output on a 3.3V DAC: (2.189/3.3) * 4096 = 2730.624
        constexpr float    max_dac_raw_f = (OvrmMaxDacOutputV / max_output_V) * dac_steps;
        constexpr uint32_t max_dac_raw   = static_cast<uint32_t>(max_dac_raw_f + 0.5f);

        // Pre-compute DAC counts per percent (eliminates runtime division)
        // Use the precise float value for maximum accuracy: 992.97 / 100 = 9.9297
        constexpr float     dac_per_percent    = max_dac_raw_f / 100.0f;
        constexpr SFXP22_10 dac_per_percent_fp = to_sfxp22_10(dac_per_percent);

        static_assert(max_dac_raw < dac_steps, "max_dac_raw exceeds DAC resolution");

        // Verify that the maximum result (100% input) fits in SFXP22_10 (int32_t)
        // Max result = max_dac_raw in SFXP22_10 format = max_dac_raw * 1024
        constexpr int64_t max_result_fp = static_cast<int64_t>(max_dac_raw) << 10;
        static_assert(max_result_fp <= std::numeric_limits<SFXP22_10>::max(),
                      "DAC result exceeds SFXP22_10 range - OvrmMaxDacOutputV too large");

        // *** Runtime calculation *** //

        // dac_raw = percent × dac_per_percent
        // Use 64-bit intermediate to avoid overflow for large OvrmMaxDacOutputV values
        // Both operands are SFXP22_10 (scaled by 2^10), so product is scaled by 2^20
        // Shift right by 10 to get result in SFXP22_10 format
        const int64_t dac_raw_64 = (static_cast<int64_t>(percent)
                                    * static_cast<int64_t>(dac_per_percent_fp))
                                >> 10;
        const SFXP22_10 dac_raw_fp = static_cast<SFXP22_10>(dac_raw_64);

        // Convert from SFXP22_10 to SFXP32_0 (integer), then to uint32_t
        return static_cast<uint32_t>(sfxp22_10_to_sfxp32_0(dac_raw_fp));
    }
};

}  // namespace nv::soc_pwr_smoothing
