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

}  // namespace sys::pwm_ctimer
