/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES.
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

/**
 * @file ahs/mcu_internal_temp.cpp
 * @brief MCU internal temperature when ADC0 is owned by AHS (e.g. P3957).
 *
 * Temp samples arrive on the shared PGOOD Trigger-3 chain (CMD15). The ISR
 * accumulates integer VBE pairs while Collecting; service() (task) takes Ready,
 * computes Celsius, publishes tempCelsius, and returns to Collecting.
 *
 * The McuInternalTemp instance (tempCelsius) lives in NV_SHARED_DATA so
 * unprivileged NSM/MCTP can read it. AbsoluteZero is the unread sentinel.
 * g_accum / g_state are ISR↔AHS only and do not need shared placement.
 */

#include "nv/volt_mon/ahs/mcu_internal_temp.h"

#include "nv/nv.h"
#include "nv/volt_mon/adc.h"

#include <atomic>
#include <cstdint>

namespace nv::mcu_internal_temp {

using namespace nv::ipc::voltage_monitor_config;

namespace {

/** Number of VBE pairs averaged before handing a window to task context. */
constexpr uint32_t AvgWindowPairs = 64U;

/** Plausible on-die range; also rejects NaN/Inf without pulling in <cmath>. */
constexpr float TempCelsiusMin = -60.0F;
constexpr float TempCelsiusMax = 150.0F;

enum class State : uint8_t
{
    Collecting = 0,
    Ready      = 1,
};

struct Accumulator
{
    uint32_t sum_vbe1{0};
    uint32_t sum_vbe8{0};
    uint32_t pair_count{0};
};

// ISR + AHS task only (both privileged) — plain .bss is fine.
Accumulator        g_accum{};
std::atomic<State> g_state{State::Collecting};

}  // namespace

McuInternalTemp::McuInternalTemp() : sensor(mcu_internal_temp_get_sensor_config())
{
    sensor.tempCelsius = AbsoluteZero;
}

McuInternalTemp& McuInternalTemp::inst()
{
    static NV_SHARED_DATA McuInternalTemp mcuInternalTemp;
    return mcuInternalTemp;
}

void McuInternalTemp::init() {}

Status McuInternalTemp::cache_temperature_celsius(Reading vbe1Reading, Reading vbe8Reading)
{
    const auto vbe1 = static_cast<uint16_t>(vbe1Reading >> ConvResultShift);
    const auto vbe8 = static_cast<uint16_t>(vbe8Reading >> ConvResultShift);
    if (vbe1 == 0U || vbe8 == 0U) {
        return Status::AdcIsrNoValidReading;
    }

    // Negated in-range test so NaN/Inf (all comparisons false) is also rejected.
    const float temp = calculate_temperature(vbe1, vbe8);
    if (!(temp > TempCelsiusMin && temp < TempCelsiusMax)) {
        return Status::AdcIsrNoValidReading;
    }

    sensor.tempCelsius = temp;
    return Status::Ok;
}

void McuInternalTemp::reset_readings() {}

void McuInternalTemp::process_reading([[maybe_unused]] Reading  adcReading,
                                      [[maybe_unused]] uint32_t loopCountIndex)
{}

Status McuInternalTemp::get_temperature_celsius(float& temperature)
{
    // AbsoluteZero means no valid window has been published yet.
    if (sensor.tempCelsius == AbsoluteZero) {
        return Status::AdcIsrNoValidReading;
    }
    temperature = sensor.tempCelsius;
    return Status::Ok;
}

namespace ahs {

void accumulate_pair_from_isr(uint16_t vbe1, uint16_t vbe8)
{
    // While Ready the completed window is owned by the task — drain still
    // happens in the IRQ handler, but do not touch the accumulator.
    if (g_state.load(std::memory_order_acquire) != State::Collecting) {
        return;
    }

    // Drop empty conversions (seen as 0.0°C after NaN→uint cast in NSM).
    if (vbe1 == 0U || vbe8 == 0U) {
        return;
    }

    g_accum.sum_vbe1 += vbe1;
    g_accum.sum_vbe8 += vbe8;
    g_accum.pair_count++;

    if (g_accum.pair_count >= AvgWindowPairs) {
        g_state.store(State::Ready, std::memory_order_release);
    }
}

void service()
{
    if (g_state.load(std::memory_order_acquire) != State::Ready) {
        return;
    }

    const auto avg_vbe1 = static_cast<uint16_t>(g_accum.sum_vbe1 / AvgWindowPairs);
    const auto avg_vbe8 = static_cast<uint16_t>(g_accum.sum_vbe8 / AvgWindowPairs);

    (void)McuInternalTemp::inst().cache_temperature_celsius(avg_vbe1, avg_vbe8);

    g_accum.sum_vbe1   = 0;
    g_accum.sum_vbe8   = 0;
    g_accum.pair_count = 0;
    g_state.store(State::Collecting, std::memory_order_release);
}

}  // namespace ahs

}  // namespace nv::mcu_internal_temp
