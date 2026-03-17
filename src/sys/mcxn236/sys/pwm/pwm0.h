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

#ifndef SYS_PWM_PWM0_H_
#define SYS_PWM_PWM0_H_

#include <cstdint>

namespace sys {
namespace pwm0 {

#define PWM0_PERIPHERAL PWM0
#define PWM0_SM0        kPWM_Module_0

constexpr uint8_t MaxPwmPercent     = 100;
constexpr uint8_t DefaultPwmPercent = 50;

// PWM channel enumeration
enum class Channel : uint8_t
{
    ChannelA = 0,  // PWM0_A0 - typically connected to FAN1
    ChannelB = 1,  // PWM0_B0 - typically connected to FAN0
    ChannelEnd,
    Invalid = ChannelEnd
};

class Driver
{
public:
    /**
     * @brief Initialize the PWM0 peripheral
     *
     * This function initializes the PWM0 module with default settings:
     * - 16 kHz frequency
     * - Center-aligned mode
     * - Both channels initially set to 50% duty cycle
     * - PWM outputs enabled and timer started
     */
    static void init();

    /**
     * @brief Set PWM duty cycle for a specific channel
     *
     * @param channel PWM channel (ChannelA or ChannelB)
     * @param duty_cycle_percent Duty cycle in percent (0-100)
     *                          0 = always low, 100 = always high
     */
    static void set_pwm(Channel channel, uint8_t duty_cycle_percent);

private:
    Driver()  = delete;
    ~Driver() = delete;
};

}  // namespace pwm0
}  // namespace sys

#endif  // SYS_PWM_PWM0_H_
