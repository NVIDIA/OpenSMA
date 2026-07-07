/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include NV_IPC_CONFIG_H
#include "driver.h"
#include "nv/common/preproc.h"
#include "nv/fancontrol/common.h"
#include "sys/pwm/pwm0.h"
#include "sys/pwm/pwm_ctimer.h"

#include <array>

namespace nv::fancontrol {

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS std::array<uint8_t, FanNum> g_fan_duty{};
}  // namespace

void Driver::init()
{
    // Bring up the PWM backend selected in config.h (FanPwm).
    if constexpr (FanPwm == PwmBackend::Ctimer) {
        sys::pwm_ctimer::Driver::init();
    }
    else {
        sys::pwm0::Driver::init();
    }

    // Per-PWM default duty is owned by the project config.h.
    for (size_t i = 0; i < FanNum; ++i) {
        set_pwm_duty_cycle(i, FanDefaultDuty.at(i));
    }
}

void Driver::set_fan_pwm(uint32_t duty_cycle, size_t fan_index)
{
    set_pwm_duty_cycle(fan_index, duty_cycle);
}

void Driver::stop_fan_pwm(size_t fan_index)
{
    set_pwm_duty_cycle(fan_index, 0);
}

void Driver::set_pwm_duty_cycle(size_t index, uint32_t duty_cycle)
{
    // Clamp duty cycle to valid range (0-100%)
    constexpr uint32_t MaxPwmDuty = 100;
    if (duty_cycle > MaxPwmDuty) {
        duty_cycle = MaxPwmDuty;
    }

    if (index >= FanNum) {
        return;  // invalid fan index
    }

    // Cache the commanded duty so Get-PWM can read it back.
    g_fan_duty.at(index) = static_cast<uint8_t>(duty_cycle);

    // Dispatch to the PWM backend selected in config.h.
    if constexpr (FanPwm == PwmBackend::Ctimer) {
        // CTIMER instance, period channel and per-fan match channel come from config.h.
        sys::pwm_ctimer::Driver::set_pwm(PwmCtimerInstance,
                                         PwmCtimerPeriodCh,
                                         PwmCtimerMatchCh.at(index),
                                         static_cast<uint8_t>(duty_cycle));
    }
    else {
        // index 0 -> PWM0_B0 (ChannelB), index 1 -> PWM0_A0 (ChannelA)
        sys::pwm0::Channel channel = sys::pwm0::Channel::ChannelB;
        switch (index) {
            case 0 : channel = sys::pwm0::Channel::ChannelB; break;
            case 1 : channel = sys::pwm0::Channel::ChannelA; break;
            default: return;  // invalid index
        }
        sys::pwm0::Driver::set_pwm(channel, static_cast<uint8_t>(duty_cycle));
    }
}

uint8_t Driver::get_fan_pwm(size_t fan_index)
{
    return (fan_index < FanNum) ? g_fan_duty.at(fan_index) : 0;
}

size_t Driver::fan_count()
{
    return FanNum;
}

}  // namespace nv::fancontrol
