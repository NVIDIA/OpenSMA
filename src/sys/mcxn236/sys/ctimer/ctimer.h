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
#include <array>
#include <cstdint>
#include <limits>

#include "fsl_ctimer.h"

#include "nv/nv.h"

#include NV_IPC_CONFIG_H

#define SYS_CTIMER_CTIMER CTIMER3
// #define SYS_CTIMER_CTIMER_CLK_FREQ CLOCK_GetCTimerClkFreq(TimerInstance)
namespace sys::ctimer {

using Ticks = uint32_t;
class Driver
{
public:
    constexpr inline static uint32_t get_freq() { return CtimerFreq; };
    static uint32_t                  get_prescale_to_us();
    static void                      delay_for_us(uint32_t us);
    static Ticks                     get_counter_difference(Ticks start_count, Ticks cur_count);

protected:
    static void     init();
    static uint32_t read_ticks();
    static uint32_t ticks_to_us(Ticks ticks);

    static Ticks get_current_ticks();
    static Ticks us_to_ticks(uint32_t us);

    // constexpr static uint32_t TimerInstance = 3;
    static constexpr uint32_t OneSecUs = 1'000'000;
    static constexpr uint32_t OneSecMs = 1'000;
    static constexpr uint32_t OneMsUs  = 1'000;

    // If the timer frequency changed, this constant need to be modified.
    static constexpr uint32_t CtimerFreq = nv::ipc::CtimerFrequency;

    static uint32_t get_prescale_counter();
};

#if 0
struct Ticks
{
    uint32_t timer_counter;
    uint32_t prescale_counter;
    bool     operator>(const Ticks& rhs) const
    {
        return timer_counter != rhs.timer_counter ? (timer_counter > rhs.timer_counter)
                                                  : (prescale_counter > rhs.prescale_counter);
    }

    inline Ticks operator+(const Ticks& rhs) const
    {
        Ticks    result{};
        uint32_t prescale    = nv::common::add(prescale_counter, rhs.prescale_counter);
        uint32_t carry       = (prescale / Driver::get_prescale_to_ms());
        result.timer_counter = nv::common::add(
            nv::common::add(timer_counter, rhs.timer_counter), carry);
        result.prescale_counter = (prescale % Driver::get_prescale_to_ms());
        return result;
    }

    inline Ticks operator-(const Ticks& rhs) const
    {
        Ticks result{};
        if (rhs > *this) {
            return {0, 0};
        }
        if (prescale_counter >= rhs.prescale_counter) {
            result.prescale_counter = prescale_counter - rhs.prescale_counter;
            result.timer_counter    = timer_counter - rhs.timer_counter;
        }
        else {
            result.prescale_counter = nv::common::sub(
                nv::common::add(prescale_counter, Driver::get_prescale_to_ms()),
                rhs.prescale_counter);
            result.timer_counter = nv::common::sub((timer_counter - 1), rhs.timer_counter);
        }
        return result;
    }
};
#endif

}  // namespace sys::ctimer