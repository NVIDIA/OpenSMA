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

#include "nv/volt_mon/mcu_internal_temp.h"
#include "sys/adc/adc.h"

namespace nv::mcu_internal_temp {

using namespace nv::ipc::voltage_monitor_config;

namespace {

// ADC conversion constants
constexpr uint32_t ConvResultShift = 3U;  // Right shift for ADC result

/*
 * MCU internal temperature command runs in `kLPADC_SampleChannelDiffBothSide`
 * with `loopCount = 1`, which produces two FIFO0 entries per trigger: the first
 * carries VBE1 (LOOPCNT=0) and the second carries VBE8 (LOOPCNT=1). The dispatcher
 * forwards `AdcConvResult::loopCountIndex` (sourced from the LPADC RESFIFO LOOPCNT
 * field) so this consumer can pair the two readings without relying on FIFO order
 * alone.
 *
 * Reference: MCXNX4XRM "LPADC RESFIFO register" (LOOPCNT field) and the temperature
 * sensor section describing the VBE1/VBE8 sequence.
 */
constexpr uint32_t Vbe1LoopIndex = 0U;
constexpr uint32_t Vbe8LoopIndex = 1U;

struct VbeCache
{
    Reading cachedVbe1Reading{};
    bool    haveCachedVbe1Reading = false;
};

/*
 * Per-tick VBE1/VBE8 pair-cache state.
 *
 * This deliberately lives in the timer implementation rather than as private
 * members of `McuInternalTemp` so the public header stays layout-neutral between
 * the legacy ISR-driven and timer-driven implementations. The function-local
 * static keeps the mutable cache out of namespace scope, which also satisfies
 * clang-tidy's non-const global variable check.
 */
VbeCache& vbe_cache()
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static VbeCache cache;
    return cache;
}

float calculate_temperature(uint16_t vbe1, uint16_t vbe8)
{
    constexpr float A     = sys::adc::ADC::get_temp_parameter_a();
    constexpr float B     = sys::adc::ADC::get_temp_parameter_b();
    constexpr float alpha = sys::adc::ADC::get_temp_parameter_alpha();

    const float delta = alpha * ((float)vbe8 - (float)vbe1);
    return A * (delta / ((float)vbe8 + delta)) - B;
}

}  // namespace

McuInternalTemp::McuInternalTemp() : sensor(mcu_internal_temp_get_sensor_config()) {}

McuInternalTemp& McuInternalTemp::inst()
{
    static NV_SHARED_DATA McuInternalTemp mcuInternalTemp;
    return mcuInternalTemp;
}

Status McuInternalTemp::cache_temperature_celsius(Reading vbe1Reading, Reading vbe8Reading)
{
    const auto vbe1 = static_cast<uint16_t>(vbe1Reading >> ConvResultShift);
    const auto vbe8 = static_cast<uint16_t>(vbe8Reading >> ConvResultShift);

    sensor.tempCelsius = calculate_temperature(vbe1, vbe8);

    return Status::Ok;
}

void McuInternalTemp::reset_readings()
{
    auto& cache                 = vbe_cache();
    cache.haveCachedVbe1Reading = false;
    cache.cachedVbe1Reading     = 0;
}

void McuInternalTemp::process_reading(Reading adcReading, uint32_t loopCountIndex)
{
    auto& cache = vbe_cache();

    if (loopCountIndex == Vbe1LoopIndex) {
        cache.cachedVbe1Reading     = adcReading;
        cache.haveCachedVbe1Reading = true;
        return;
    }

    if (loopCountIndex == Vbe8LoopIndex && cache.haveCachedVbe1Reading) {
        cache_temperature_celsius(cache.cachedVbe1Reading, adcReading);
        cache.haveCachedVbe1Reading = false;
    }
}

Status McuInternalTemp::get_temperature_celsius(float& temperature)
{
    temperature = sensor.tempCelsius;
    return Status::Ok;
}

}  // namespace nv::mcu_internal_temp
