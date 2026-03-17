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

// PWM channel enumeration
enum class Channel : uint8_t
{
    ChannelA = 0,  // PWM Channel A
    ChannelB = 1,  // PWM Channel B
};

class Driver
{
public:
    /**
     * @brief Initialize the PWM0 peripheral
     *
     * Note: x86 stub implementation - no actual hardware
     */
    static void init();

    /**
     * @brief Set PWM duty cycle for a specific channel
     *
     * Note: x86 stub implementation - no actual hardware
     *
     * @param channel PWM channel (ChannelA or ChannelB)
     * @param duty_cycle_percent Duty cycle in percent (0-100)
     */
    static void set_pwm(Channel channel, uint8_t duty_cycle_percent);

private:
    Driver()  = delete;
    ~Driver() = delete;
};

}  // namespace pwm0
}  // namespace sys

#endif  // SYS_PWM_PWM0_H_
