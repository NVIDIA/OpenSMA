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

// NOLINTBEGIN - Suppress clang-tidy warnings for NTC lookup table implementation

#include "nv/volt_mon/ntc_table.h"

namespace nv::volt_mon {

namespace {

/**
 * @brief Convert voltage to NTC resistance
 *
 * Circuit: Vcc -- [R_pullup] -- ADC_IN -- [NTC] -- GND
 * V_adc = Vcc × R_ntc / (R_ntc + R_pullup)
 * R_ntc = R_pullup × V_adc / (Vcc - V_adc)
 *
 * @param voltageMv Voltage at ADC input in mV
 * @return NTC resistance in ohms
 */
uint32_t voltage_to_resistance(uint16_t voltageMv)
{
    if (voltageMv >= NtcVref) {
        // Voltage at or above Vref means NTC is open (infinite resistance)
        return UINT32_MAX;
    }
    if (voltageMv == 0) {
        // Voltage is 0 means NTC is shorted (zero resistance)
        return 0;
    }

    // R_ntc = R_pullup × V_adc / (Vcc - V_adc)
    const uint32_t numerator   = NtcPullupResistor * static_cast<uint32_t>(voltageMv);
    const uint32_t denominator = NtcVref - voltageMv;

    return numerator / denominator;
}

}  // namespace

/**
 * @brief Convert NTC resistance to temperature using lookup table with linear interpolation
 *
 * @param resistanceOhm NTC resistance in ohms
 * @return Temperature in 0.1°C units, or NtcTempInvalid if out of range
 */
int16_t ntc_resistance_to_temperature(uint32_t resistanceOhm)
{
    // Convert to table units (10Ω)
    const uint32_t rScaled = resistanceOhm / NtcResistanceScale;

    // Check bounds - clamp to valid range instead of returning invalid
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    if (rScaled > NtcResistanceTable[0]) {
        // Resistance too high -> temperature too low (< -40°C)
        // Return minimum temperature
        return NtcTempMin * NtcTempScale;  // -40°C in 0.1°C units
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    if (rScaled < NtcResistanceTable[NtcTableSize - 1]) {
        // Resistance too low -> temperature too high (> 125°C)
        // Return maximum temperature
        return NtcTempMax * NtcTempScale;  // 125°C in 0.1°C units
    }

    // Binary search for the temperature range
    size_t low  = 0;
    size_t high = NtcTableSize - 1;

    while (low < high - 1) {
        const size_t mid = (low + high) / 2;
        if (mid >= NtcTableSize) {
            break;  // Safety check for Coverity
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        if (rScaled > NtcResistanceTable[mid]) {
            high = mid;
        }
        else {
            low = mid;
        }
    }

    // Linear interpolation between table[low] and table[high]
    // table[low] corresponds to temperature (NtcTempMin + low)
    // table[high] corresponds to temperature (NtcTempMin + high)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    const uint16_t rHigh = NtcResistanceTable[low];  // Higher resistance = lower temp
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    const uint16_t rLow = NtcResistanceTable[high];  // Lower resistance = higher temp

    // Avoid division by zero
    if (rHigh == rLow) {
        return static_cast<int16_t>((NtcTempMin + static_cast<int16_t>(low)) * NtcTempScale);
    }

    // Calculate interpolated temperature (in 0.1°C units)
    // ratio = (rHigh - rScaled) / (rHigh - rLow)
    // temp = tempLow + ratio * (tempHigh - tempLow)
    const int32_t tempLow  = (NtcTempMin + static_cast<int16_t>(low)) * NtcTempScale;
    const int32_t tempHigh = (NtcTempMin + static_cast<int16_t>(high)) * NtcTempScale;

    // Use int64_t to avoid overflow in multiplication and division
    const int64_t numerator   = static_cast<int64_t>(rHigh - rScaled) * (tempHigh - tempLow);
    const int64_t denominator = static_cast<int64_t>(rHigh - rLow);
    // 1. Guard against Division by Zero
    // 2. Guard against Signed Overflow (INT64_MIN / -1)
    if (denominator == 0 || (numerator == INT64_MIN && denominator == -1)) {
        return NtcTempInvalid;
    }
    const auto tempResult = tempLow + (numerator / denominator);

    // Clamp to valid temperature range (in 0.1°C units: -400 to 1250)
    constexpr int64_t tempMin = static_cast<int64_t>(NtcTempMin) * NtcTempScale;
    constexpr int64_t tempMax = static_cast<int64_t>(NtcTempMax) * NtcTempScale;

    if (tempResult < tempMin) {
        return static_cast<int16_t>(tempMin);
    }
    if (tempResult > tempMax) {
        return static_cast<int16_t>(tempMax);
    }
    return static_cast<int16_t>(tempResult);
}

/**
 * @brief Convert voltage (mV) to temperature
 */
int16_t ntc_voltage_to_temperature(uint16_t voltageMv)
{
    const uint32_t resistance = voltage_to_resistance(voltageMv);
    return ntc_resistance_to_temperature(resistance);
}

/**
 * @brief Convert ADC reading to temperature
 */
int16_t ntc_adc_to_temperature(uint16_t adcReading, uint16_t adcVref)
{
    // Convert ADC reading to voltage (mV)
    // voltage = adcReading × Vref / 65535
    const uint32_t voltageMv = (static_cast<uint32_t>(adcReading) * adcVref) / AdcMaxValue;

    // voltageMv is always <= adcVref which fits in uint16_t
    const auto voltageMv16 = (voltageMv > UINT16_MAX) ? UINT16_MAX
                                                      : static_cast<uint16_t>(voltageMv);
    return ntc_voltage_to_temperature(voltageMv16);
}

/**
 * @brief Get NTC resistance for a given temperature
 */
uint32_t ntc_temperature_to_resistance(int16_t tempCelsius)
{
    if (tempCelsius < NtcTempMin || tempCelsius > NtcTempMax) {
        return 0;
    }

    const auto index = static_cast<size_t>(tempCelsius - NtcTempMin);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return static_cast<uint32_t>(NtcResistanceTable[index]) * NtcResistanceScale;
}

}  // namespace nv::volt_mon

// NOLINTEND
