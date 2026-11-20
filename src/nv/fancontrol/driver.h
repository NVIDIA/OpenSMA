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

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "nv/fancontrol/common.h"

namespace nv::fancontrol {

/**
 * @brief Fan control driver - Direct PWM hardware control
 *
 * Simple driver for controlling fan PWM duty cycles without task overhead.
 * Provides direct, synchronous hardware control functions.
 */
class Driver
{
public:
    /**
     * @brief Initialize the fan control hardware and set default PWM values
     *
     * This should be called once during system initialization.
     */
    static void init();

    /**
     * @brief Set PWM duty cycle for a specific fan
     *
     * @param duty_cycle PWM duty cycle percentage (0-100)
     * @param fan_index Fan index (0-based)
     */
    static void set_fan_pwm(uint32_t duty_cycle, size_t fan_index);

    /**
     * @brief Stop a specific fan (set PWM to 0%)
     *
     * @param fan_index Fan index (0-based)
     */
    static void stop_fan_pwm(size_t fan_index);

    /**
     * @brief Get a fan's current (last-commanded) PWM duty cycle.
     *
     * PWM is an output (there is no duty sensor), so this returns the value
     * that was last written for the fan.
     *
     * @param fan_index Fan index (0-based)
     * @return duty cycle percentage (0-100)
     */
    static uint8_t get_fan_pwm(size_t fan_index);

    /**
     * @brief Number of PWM fan channels, from the project config.h.
     */
    static size_t fan_count();

    /**
     * @brief Drive the fans along the project fan curve for the given
     *        temperatures.
     *
     * Push-model entry point for the thermal poll loop: call it after each
     * temperature read. temps[i] drives fan i through the config.h FanCurve[i]
     * (linear interpolation between breakpoints, clamped at both ends); the
     * project config.h documents which temperature belongs to which fan.
     * A temps[i] of InvalidTemp (no valid reading) fails safe to the fan's
     * config.h FanDefaultDuty.
     *
     * Does nothing in FanMode::Manual (a VDM client owns the fans). Fans with
     * an empty curve, or without a matching temps entry, keep their current
     * duty. A project that never calls update() keeps fully manual control.
     *
     * @param temps one temperature (degrees Celsius) per fan, index-aligned
     *              with the config.h fan indexes
     */
    static void update(std::span<const int8_t> temps);

    /**
     * @brief Current fan control mode (Auto = curves, Manual = VDM).
     */
    static FanMode get_fan_mode();

    /**
     * @brief Select the fan control mode.
     *
     * Auto hands the fans back to the curves on the next update(). Manual is
     * also entered implicitly by any set_fan_pwm() / stop_fan_pwm() call.
     */
    static void set_fan_mode(FanMode mode);

    /**
     * @brief Measure a fan's tach speed (blocking: up to ~200 ms for a
     *        stopped fan).
     *
     * @param tach_index tach input index (0-based, config.h TachInputs order)
     * @return fan speed in RPM, 0 when the fan is stopped or the index has
     *         no tach input
     */
    static uint16_t get_fan_tach(size_t tach_index);

    /**
     * @brief Number of fan tach inputs, from the project config.h.
     */
    static size_t tach_count();

private:
    /**
     * @brief Internal function to set PWM duty cycle with validation
     *
     * @param index Fan index
     * @param duty_cycle PWM duty cycle percentage (0-100)
     */
    static void set_pwm_duty_cycle(size_t index, uint32_t duty_cycle);
};

}  // namespace nv::fancontrol
