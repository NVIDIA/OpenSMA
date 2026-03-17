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

#include "driver.h"
#include "sys/pwm/pwm0.h"

namespace nv::fancontrol {

void Driver::init()
{
    // Initialize PWM0 driver (16kHz PWM, both channels at 50% initially)
    sys::pwm0::Driver::init();

    // Default PWM duty cycle percentages
    constexpr uint32_t Fan0DefaultPwmPercent = 80;  // CPU fan default PWM 80%
    constexpr uint32_t Fan1DefaultPwmPercent = 50;  // Side fan default PWM 50%

    set_pwm_duty_cycle(0, Fan0DefaultPwmPercent);
    set_pwm_duty_cycle(1, Fan1DefaultPwmPercent);
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

    // Map fan index to PWM channel
    // FAN0 (index 0) → GPIO3_1 → PWM0_B0 → ChannelB
    // FAN1 (index 1) → GPIO3_0 → PWM0_A0 → ChannelA
    sys::pwm0::Channel pwm_channel = sys::pwm0::Channel::ChannelB;
    switch (index) {
        case 0 : pwm_channel = sys::pwm0::Channel::ChannelB; break;
        case 1 : pwm_channel = sys::pwm0::Channel::ChannelA; break;
        default: return;  // Invalid index
    }

    // Set hardware PWM duty cycle (safe to cast after clamping)
    sys::pwm0::Driver::set_pwm(pwm_channel, static_cast<uint8_t>(duty_cycle));
}

}  // namespace nv::fancontrol
