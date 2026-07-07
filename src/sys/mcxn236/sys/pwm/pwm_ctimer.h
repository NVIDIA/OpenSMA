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

#ifndef SYS_PWM_PWM_CTIMER_H_
#define SYS_PWM_PWM_CTIMER_H_

#include <cstdint>

namespace sys::pwm_ctimer {

constexpr uint8_t MaxPwmPercent = 100;

// CTIMER peripheral instance that drives the PWM outputs.
enum class Instance : uint8_t
{
    Ctimer0 = 0,
    Ctimer1 = 1,
    Ctimer2 = 2,
    Ctimer3 = 3,
    Ctimer4 = 4,
    None    = 0xFF,  // CTIMER backend not used by this project
};

// CTIMER match register. Used both for the PWM period channel and per-output duty channels.
enum class Channel : uint8_t
{
    Channel0 = 0,
    Channel1 = 1,
    Channel2 = 2,
    Channel3 = 3,
    None     = 0xFF,  // unused channel (paired with Instance::None)
};

class Driver
{
public:
    /**
     * @brief Prepare the CTIMER PWM driver for runtime duty updates.
     *
     * The CTIMER PWM period and match channels are configured by the
     * MCUXpresso-generated CTIMERx_init(), invoked from BOARD_InitPeripherals()
     * before this runs. This entry point is kept for symmetry with sys::pwm0
     * and as a hook if explicit (re)setup is later needed.
     */
    static void init();

    /**
     * @brief Set the PWM duty cycle on a CTIMER match output.
     *
     * The CTIMER instance, period channel and match channel are supplied by the
     * caller (sourced from the project config.h) so this driver is not tied to a
     * specific CTIMER instance or channel layout.
     *
     * @param instance           CTIMER instance that drives the PWM
     * @param period_channel     match channel defining the PWM period
     * @param match_channel      match channel for this output's duty
     * @param duty_cycle_percent duty cycle in percent (0-100)
     */
    static void set_pwm(Instance instance,
                        Channel  period_channel,
                        Channel  match_channel,
                        uint8_t  duty_cycle_percent);

private:
    Driver()  = delete;
    ~Driver() = delete;
};

}  // namespace sys::pwm_ctimer

#endif  // SYS_PWM_PWM_CTIMER_H_
