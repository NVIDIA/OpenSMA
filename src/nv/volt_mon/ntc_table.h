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

#include <cstddef>
#include <cstdint>

namespace nv::volt_mon {

/**
 * @brief NTC Thermistor Lookup Table
 *
 * Based on: JAS103F398FB28W03X (Walsin/Joyin)
 * - R25 = 10KΩ ± 1%
 * - B25/85 = 3977K ± 1%
 * - Temperature Range: -40°C ~ +125°C
 *
 * Circuit: 1KΩ pull-up resistor to 3.3V, NTC to GND
 * V_adc = 3.3V × R_ntc / (R_ntc + 1KΩ)
 */

// Temperature range
constexpr int16_t NtcTempMin   = -40;  // °C
constexpr int16_t NtcTempMax   = 125;  // °C
constexpr size_t  NtcTableSize = 166;  // -40 to 125, inclusive

// Pull-up resistor value (ohms)
constexpr uint32_t NtcPullupResistor = 1000;  // 1KΩ

// Reference voltage (mV)
constexpr uint32_t NtcVref = 3300;  // 3.3V

// Temperature unit conversion factor (0.1°C to °C)
constexpr int16_t NtcTempScale = 10;

// Resistance unit conversion factor (table stores in 10Ω units)
constexpr uint32_t NtcResistanceScale = 10;

// ADC maximum value (16-bit)
constexpr uint32_t AdcMaxValue = 65535;

// Invalid temperature value
constexpr int16_t NtcTempInvalid = -999;

/**
 * @brief NTC R-T Table (Resistance in units of 10Ω)
 *
 * Index 0 = -40°C, Index 165 = 125°C
 * Value unit: 10Ω (e.g., 33727 = 337.27KΩ)
 *
 * Total size: 166 × 2 = 332 bytes
 */
// clang-format off
constexpr uint16_t NtcResistanceTable[NtcTableSize] = {
    // -40°C to -31°C
    33727, 31544, 29518, 27636, 25886, 24258, 22744, 21334, 20021, 18798,
    // -30°C to -21°C
    17657, 16593, 15599, 14672, 13806, 12997, 12240, 11532, 10869, 10249,
    // -20°C to -11°C
     9668,  9124,  8613,  8135,  7686,  7264,  6869,  6497,  6148,  5819,
    // -10°C to -1°C
     5510,  5220,  4946,  4689,  4446,  4217,  4002,  3799,  3607,  3426,
    //   0°C to  9°C
     3255,  3094,  2942,  2798,  2663,  2533,  2412,  2297,  2188,  2084,
    //  10°C to 19°C
     1987,  1894,  1806,  1723,  1644,  1570,  1499,  1431,  1367,  1306,
    //  20°C to 29°C
     1249,  1194,  1142,  1092,  1045,  1000,   957,   917,   878,   841,
    //  30°C to 39°C
      806,   773,   741,   710,   681,   654,   627,   602,   578,   555,
    //  40°C to 49°C
      533,   512,   492,   473,   455,   437,   421,   405,   389,   375,
    //  50°C to 59°C
      361,   347,   334,   322,   310,   299,   288,   278,   268,   258,
    //  60°C to 69°C
      249,   240,   232,   224,   216,   209,   201,   194,   188,   181,
    //  70°C to 79°C
      175,   169,   164,   158,   153,   148,   143,   139,   134,   130,
    //  80°C to 89°C
      126,   122,   118,   114,   111,   107,   104,   100,    97,    94,
    //  90°C to 99°C
       92,    89,    86,    84,    81,    79,    76,    74,    72,    70,
    // 100°C to 109°C
       68,    66,    64,    62,    60,    58,    57,    55,    54,    52,
    // 110°C to 119°C
       51,    49,    48,    47,    45,    44,    43,    42,    41,    40,
    // 120°C to 125°C
       39,    38,    37,    36,    35,    34
};
// clang-format on

/**
 * @brief Convert ADC reading to temperature
 *
 * @param adcReading 16-bit ADC reading (0-65535)
 * @param adcVref ADC reference voltage in mV (default 3300)
 * @return Temperature in °C (×10 for 0.1°C resolution), or NtcTempInvalid if out of range
 *
 * @note Return value is in 0.1°C units (e.g., 250 = 25.0°C)
 */
int16_t ntc_adc_to_temperature(uint16_t adcReading, uint16_t adcVref = NtcVref);

/**
 * @brief Convert voltage (mV) to temperature
 *
 * @param voltageMv Voltage in mV
 * @return Temperature in °C (×10 for 0.1°C resolution), or NtcTempInvalid if out of range
 */
int16_t ntc_voltage_to_temperature(uint16_t voltageMv);

/**
 * @brief Convert NTC resistance to temperature
 *
 * @param resistanceOhm NTC resistance in ohms
 * @return Temperature in °C (×10 for 0.1°C resolution), or NtcTempInvalid if out of range
 */
int16_t ntc_resistance_to_temperature(uint32_t resistanceOhm);

/**
 * @brief Get NTC resistance for a given temperature (for testing/validation)
 *
 * @param tempCelsius Temperature in °C
 * @return Resistance in ohms, or 0 if out of range
 */
uint32_t ntc_temperature_to_resistance(int16_t tempCelsius);

/**
 * @brief Convert temperature to ADC value (constexpr for compile-time config)
 *
 * @param tempCelsius Temperature in °C (-40 to 125)
 * @return ADC value (16-bit), or 0xFFFF if out of range
 *
 * @note This is a constexpr function for use in config.h
 *       Conversion: Temp → Resistance → Voltage → ADC
 */
constexpr uint16_t ntc_temp_to_adc_value(int16_t tempCelsius)
{
    // Range check
    if (tempCelsius < NtcTempMin || tempCelsius > NtcTempMax) {
        return 0xFFFF;  // Invalid
    }

    // Get resistance from table (units: 10Ω)
    const size_t   index      = static_cast<size_t>(tempCelsius - NtcTempMin);
    const uint32_t resistance = NtcResistanceTable[index] * 10;  // Convert to Ω

    // Calculate voltage: V = Vref × R_ntc / (R_ntc + R_pullup)
    const uint32_t voltageMv = NtcVref * resistance / (resistance + NtcPullupResistor);

    // Convert voltage to ADC value: ADC = voltage * 65535 / Vref
    // Use uint64_t to avoid overflow warning from static analysis
    const auto adcValue = static_cast<uint64_t>(voltageMv) * AdcMaxValue / NtcVref;
    return (adcValue > UINT16_MAX) ? UINT16_MAX : static_cast<uint16_t>(adcValue);
}

}  // namespace nv::volt_mon
