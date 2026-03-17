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

#include "sys/pwm/pwm0.h"

#include <cassert>

#include "board/peripherals.h"
#include "fsl_pwm.h"

namespace sys::pwm0 {

void Driver::init()
{
    // Set both channels to 50% duty cycle initially
    PWM_UpdatePwmDutycycle(PWM0_PERIPHERAL, PWM0_SM0, kPWM_PwmA, kPWM_SignedCenterAligned, 50);
    PWM_UpdatePwmDutycycle(PWM0_PERIPHERAL, PWM0_SM0, kPWM_PwmB, kPWM_SignedCenterAligned, 50);

    // Set load okay bit to load registers from buffer
    PWM_SetPwmLdok(PWM0_PERIPHERAL, kPWM_Control_Module_0, true);

    // Start PWM timer
    PWM_StartTimer(PWM0_PERIPHERAL, kPWM_Control_Module_0);
}

void Driver::set_pwm(Channel channel, uint8_t duty_cycle_percent)
{
    // Clamp duty cycle to valid range (0-100)
    if (duty_cycle_percent > 100) {
        duty_cycle_percent = 100;
    }

    // Select the appropriate PWM channel
    pwm_channels_t pwm_channel;
    switch (channel) {
        case Channel::ChannelA: pwm_channel = kPWM_PwmA; break;
        case Channel::ChannelB: pwm_channel = kPWM_PwmB; break;
        default:
            // Invalid channel
            assert(false);
            return;
    }

    PWM_UpdatePwmDutycycle(
        PWM0_PERIPHERAL, PWM0_SM0, pwm_channel, kPWM_SignedCenterAligned, duty_cycle_percent);

    // Set load okay bit to load the new value from buffer
    PWM_SetPwmLdok(PWM0_PERIPHERAL, kPWM_Control_Module_0, true);
}

}  // namespace sys::pwm0
