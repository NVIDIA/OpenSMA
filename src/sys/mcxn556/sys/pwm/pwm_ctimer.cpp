/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include "sys/pwm/pwm_ctimer.h"

#include <cassert>

#include "FreeRTOS.h"
#include "task.h"

#include "fsl_clock.h"
#include "fsl_ctimer.h"

namespace sys::pwm_ctimer {

namespace {

// Resolve a CTIMER instance selector to its peripheral base pointer.
CTIMER_Type* ctimer_base(Instance instance)
{
    switch (instance) {
        case Instance::Ctimer0: return CTIMER0;
        case Instance::Ctimer1: return CTIMER1;
        case Instance::Ctimer2: return CTIMER2;
        case Instance::Ctimer3: return CTIMER3;
        case Instance::Ctimer4: return CTIMER4;
        case Instance::None   : break;  // backend unused
    }
    return nullptr;  // None / out-of-range instance
}

// Map a Channel selector to the SDK match register enum.
ctimer_match_t to_match(Channel channel)
{
    switch (channel) {
        case Channel::Channel0: return kCTIMER_Match_0;
        case Channel::Channel1: return kCTIMER_Match_1;
        case Channel::Channel2: return kCTIMER_Match_2;
        case Channel::Channel3: return kCTIMER_Match_3;
        case Channel::None    : break;
    }
    assert(false);  // None / invalid channel
    return kCTIMER_Match_0;
}

// Give-up time per tach edge; a stopped fan produces no edges and reads 0 RPM.
constexpr uint32_t TachEdgeTimeoutMs = 100U;

// Converts a per-second tick rate into the per-minute RPM figure.
constexpr uint64_t SecondsPerMinute = 60U;

// Wait for a fresh rising-edge capture event by polling the IR latch (no ISR).
bool wait_capture_edge(CTIMER_Type* base, uint32_t flag_mask)
{
    base->IR                      = flag_mask;  // W1C: discard any stale event
    const TickType_t TimeoutTicks = pdMS_TO_TICKS(TachEdgeTimeoutMs);
    for (TickType_t waited = 0; waited < TimeoutTicks; ++waited) {
        if ((base->IR & flag_mask) != 0U) {
            return true;
        }
        vTaskDelay(1);
    }
    return (base->IR & flag_mask) != 0U;
}

}  // namespace

void Driver::init()
{
    // CTIMER PWM is configured by the board-generated CTIMERx_init().
}

void Driver::set_pwm(Instance instance,
                     Channel  period_channel,
                     Channel  match_channel,
                     uint8_t  duty_cycle_percent)
{
    if (duty_cycle_percent > MaxPwmPercent) {
        duty_cycle_percent = MaxPwmPercent;
    }

    CTIMER_Type* base = ctimer_base(instance);
    if (base == nullptr) {
        assert(false);  // unknown / unavailable CTIMER instance
        return;
    }

    CTIMER_UpdatePwmDutycycle(
        base, to_match(period_channel), to_match(match_channel), duty_cycle_percent);
}

uint32_t Driver::measure_tach_rpm(Instance instance, CaptureChannel capture_channel)
{
    CTIMER_Type* base = ctimer_base(instance);
    if (base == nullptr || capture_channel == CaptureChannel::None) {
        return 0;
    }

    const auto     cap       = static_cast<uint32_t>(capture_channel);
    const uint32_t flag_mask = static_cast<uint32_t>(kCTIMER_Capture0Flag) << cap;

    // Tick rate of the (already running) counter: module clock / prescaler.
    // Derived at measure time so this never assumes who configured the timer.
    const uint32_t clock_hz = CLOCK_GetCTimerClkFreq(static_cast<uint32_t>(instance));
    const uint32_t tick_hz  = clock_hz / (base->PR + 1U);
    if (tick_hz == 0U) {
        return 0;  // counter is not clocked / not set up
    }

    // Rising-edge capture; setting CAPnI only latches the event in IR for
    // polling - the CTIMER NVIC line is never enabled, so no interrupt fires.
    CTIMER_SetupCapture(
        base, static_cast<ctimer_capture_channel_t>(cap), kCTIMER_Capture_RiseEdge, false);
    base->CCR |= CTIMER_CCR_CAP0I_MASK << (cap * 3U);

    // Two consecutive rising edges bound one tach pulse period.
    uint32_t rpm = 0;
    if (wait_capture_edge(base, flag_mask)) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        const uint32_t first_edge = base->CR[cap];
        if (wait_capture_edge(base, flag_mask)) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            const uint32_t period_ticks = base->CR[cap] - first_edge;  // unsigned: wrap-safe
            if (period_ticks != 0U) {
                rpm = static_cast<uint32_t>((SecondsPerMinute * tick_hz) / period_ticks);
            }
        }
    }

    // Stop watching the pin between measurements.
    base->CCR &= ~((CTIMER_CCR_CAP0RE_MASK | CTIMER_CCR_CAP0FE_MASK | CTIMER_CCR_CAP0I_MASK)
                   << (cap * 3U));

    return rpm;
}

}  // namespace sys::pwm_ctimer
