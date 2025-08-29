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
#include "nv/ctimer/ctimer.h"

#include <limits>

#include "nv/common/utils.h"
#include "nv/nv.h"
using namespace sys::ctimer;

Ticks sys::ctimer::Driver::get_counter_difference(Ticks start_count, Ticks cur_count)
{
    Ticks diff_ticks{};
    if (cur_count >= start_count) {
        diff_ticks = cur_count - start_count;
    }
    else {
        const Ticks MaxTicks = 0xFFFFFFFF;
        diff_ticks           = MaxTicks - (start_count - cur_count);
    }
    return diff_ticks;
}

void nv::ctimer::Driver::init()
{
    sys::ctimer::Driver::init();
}

uint32_t Driver::get_prescale_to_us()
{
    return (get_freq() / OneSecUs);
}

void sys::ctimer::Driver::init()
{
    ctimer_config_t time_config;
    const uint32_t  PrescaleToUs = get_prescale_to_us();
    CTIMER_GetDefaultConfig(&time_config);
    time_config.prescale = (PrescaleToUs - 1);  // 0: every clock, 1: every two clock...
    CTIMER_Init(SYS_CTIMER_CTIMER, &time_config);
    CTIMER_StartTimer(SYS_CTIMER_CTIMER);
}

uint32_t sys::ctimer::Driver::read_ticks()
{
    return CTIMER_GetTimerCountValue(SYS_CTIMER_CTIMER);
}

uint32_t sys::ctimer::Driver::get_prescale_counter()
{
    return SYS_CTIMER_CTIMER->PC;
}

uint32_t sys::ctimer::Driver::ticks_to_us(Ticks ticks)
{
    return ticks;
}

Ticks Driver::get_current_ticks()
{
    return read_ticks();
}

Ticks Driver::us_to_ticks(uint32_t us)
{
    return us;
}

void sys::ctimer::Driver::delay_for_us(uint32_t us)
{
    if (us > 0) {
        auto       start_count = get_current_ticks();
        auto       cur_count   = get_current_ticks();
        const auto TargetTicks = us_to_ticks(us);
        while (TargetTicks > get_counter_difference(start_count, cur_count)) {
            cur_count = get_current_ticks();
        }
    }
}

nv::ctimer::NV_Ticks nv::ctimer::Driver::read_ticks()
{
    return sys::ctimer::Driver::get_current_ticks();
}

uint32_t nv::ctimer::Driver::ticks_to_us(NV_Ticks ticks)
{
    return sys::ctimer::Driver::ticks_to_us(ticks);
}

void nv::ctimer::Driver::delay_for_us(uint32_t us)
{
    sys::ctimer::Driver::delay_for_us(us);
}

nv::ctimer::NV_Ticks nv::ctimer::Driver::us_to_ticks(uint32_t us)
{
    return sys::ctimer::Driver::us_to_ticks(us);
}
