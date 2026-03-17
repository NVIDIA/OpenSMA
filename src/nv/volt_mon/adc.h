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

#include "common.h"
#include <atomic>
#include "sys/adc/adc.h"
#include <array>

#include NV_IPC_CONFIG_H

namespace nv::volt_mon {

// ADC configuration constants (from sys::adc abstraction layer)
// Defined here to isolate SDK header pollution from common.h

// Leak detection sensor configuration
constexpr uint8_t LeakAdcSampleTime = sys::adc::ADC_SAMPLE_TIME_ADCK131;
constexpr uint8_t LeakAdcHwAverage  = sys::adc::ADC_HW_AVERAGE_COUNT1024;

// Busbar temperature sensor configuration
constexpr uint8_t BusbarAdcSampleTime = sys::adc::ADC_SAMPLE_TIME_ADCK131;
constexpr uint8_t BusbarAdcHwAverage  = sys::adc::ADC_HW_AVERAGE_COUNT1024;

// MCU internal temperature sensor configuration (requires longer sample time)
constexpr uint8_t TempAdcSampleTime = sys::adc::ADC_SAMPLE_TIME_ADCK131;
constexpr uint8_t TempAdcHwAverage  = sys::adc::ADC_HW_AVERAGE_COUNT128;

// Power Good voltage sensor configuration
constexpr uint8_t PgoodVoltAdcSampleTime = sys::adc::ADC_SAMPLE_TIME_ADCK131;
constexpr uint8_t PgoodVoltAdcHwAverage  = sys::adc::ADC_HW_AVERAGE_COUNT128;

}  // namespace nv::volt_mon

namespace nv::mcu_internal_temp {

// Temperature sensor constants
constexpr uint32_t TempSensorChannel = 26;

// ADC conversion constants
constexpr uint32_t ConvResultShift = 3U;        // Right shift for ADC result
constexpr float    AbsoluteZero    = -273.15f;  // Absolute zero in Celsius

/**
 * @brief Calculate temperature from VBE differential readings
 *
 * Uses NXP's bandgap temperature sensor formula:
 * Temperature = A * [alpha * (vbe8 - vbe1) / (vbe8 + alpha * (vbe8 - vbe1))] - B
 *
 * where:
 * - A = FSL_FEATURE_LPADC_TEMP_PARAMETER_A (slope, 783)
 * - B = FSL_FEATURE_LPADC_TEMP_PARAMETER_B (offset, 297)
 * - alpha = FSL_FEATURE_LPADC_TEMP_PARAMETER_ALPHA (current ratio, 9.63)
 *
 * @param vbe1 First VBE reading (1x current density) - runtime value
 * @param vbe8 Second VBE reading (8x current density) - runtime value
 * @return Temperature in Celsius
 */
inline float calculate_temperature(uint16_t vbe1, uint16_t vbe8)
{
    constexpr float A     = sys::adc::ADC::get_temp_parameter_a();
    constexpr float B     = sys::adc::ADC::get_temp_parameter_b();
    constexpr float alpha = sys::adc::ADC::get_temp_parameter_alpha();

    const float delta = alpha * ((float)vbe8 - (float)vbe1);
    return A * (delta / ((float)vbe8 + delta)) - B;
}

}  // namespace nv::mcu_internal_temp

namespace nv::volt_mon {

class Adc
{
private:
    Adc()                      = delete;
    ~Adc()                     = delete;
    Adc(const Adc&)            = delete;
    Adc& operator=(const Adc&) = delete;
    Adc(Adc&&)                 = delete;
    Adc& operator=(Adc&&)      = delete;

    static bool              inited;
    static std::atomic<bool> is_oneshot_converting;

public:
    static std::array<Status, static_cast<uint32_t>(AdcInstance::Total)> adcerr;

    static void   init();
    static void   stop_sampling(AdcInstance adcId);
    static void   start_sampling(AdcInstance adcId);
    static Status start_scanning(AdcInstance adcId);

    /**
     * Generic interface - accepts any sensor type via base class reference
     * Works with LeakDetectSensor, BusBarTempSensor, and any future sensor types
     * Note: Only accesses VoltMon fields (adcId, cmdOneShot, cmdTriggerSrc, reading)
     */
    static Status start_oneshot(VoltMon& sensor);

    static bool adc_isr_get_conv_result(AdcInstance                   adcId,
                                        sys::adc::ADC::AdcConvResult& result);

    static const char* to_float_string(uint16_t value);

    /**
     * Generic template interface - accepts any sensor type
     * Works with LeakDetectSensor, BusBarTempSensor, and any future sensor types
     * Automatically extracts sensor, id, state from the specific sensor type
     */
    template<typename SensorT>
    static void dbginfo(AdcMode        adcMode,
                        const SensorT& sensor,
                        Reading        reading,
                        Threshold      thLow,
                        Threshold      thHigh);

    /**
     * @brief RAII guard for ADC oneshot conversion lock
     *
     * Automatically acquires the oneshot lock on construction and releases it on destruction.
     * Ensures the lock is always released, even if exceptions occur.
     *
     * Usage:
     * @code
     * {
     *     auto guard = Adc::OneShotGuard();
     *     if (!guard) {
     *         return Status::AdcOneShotConversionOnGoing;  // Lock already held
     *     }
     *     // ... perform ADC operations ...
     *     // Lock automatically released when guard goes out of scope
     * }
     * @endcode
     */
    class OneShotGuard
    {
    public:
        OneShotGuard() : locked_(false)
        {
            bool expected = false;
            if (is_oneshot_converting.compare_exchange_strong(expected, true)) {
                locked_ = true;
            }
        }

        ~OneShotGuard()
        {
            if (locked_) {
                is_oneshot_converting.store(false);
            }
        }

        // Non-copyable, non-movable
        OneShotGuard(const OneShotGuard&)            = delete;
        OneShotGuard& operator=(const OneShotGuard&) = delete;
        OneShotGuard(OneShotGuard&&)                 = delete;
        OneShotGuard& operator=(OneShotGuard&&)      = delete;

        // Check if lock was successfully acquired
        explicit operator bool() const { return locked_; }

    private:
        bool locked_;
    };

    static bool is_adc_error(AdcInstance adcId)
    {
        return adcerr.at(static_cast<uint32_t>(adcId)) != Status::Ok;
    }

private:
    static void dbginfo_impl(AdcMode   adcMode,
                             Sensor    sensorType,
                             SensorId  id,
                             State     state,
                             Reading   reading,
                             Threshold thLow,
                             Threshold thHigh);
};

// Template implementation must be in header
template<typename SensorT>
void Adc::dbginfo(
    AdcMode adcMode, const SensorT& sensor, Reading reading, Threshold thLow, Threshold thHigh)
{
    dbginfo_impl(adcMode, sensor.sensor, sensor.id, sensor.state, reading, thLow, thHigh);
}

}  // namespace nv::volt_mon