/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include <cstdint>
#include <span>

// Shared public types for nv::fancontrol (per-module common.h convention).
namespace nv::fancontrol {

/**
 * @brief Which hardware PWM peripheral drives the fans on this board.
 *
 * Selected per-project in config.h, e.g.:
 *   constexpr nv::fancontrol::PwmBackend FanPwm = nv::fancontrol::PwmBackend::Ctimer;
 *
 */
enum class PwmBackend : uint8_t
{
    FlexPwm0,
    Ctimer,
    Invalid = 0xFF,
};

/**
 * @brief One fan-curve breakpoint: at temp_c the fan runs at duty %.
 *
 * A curve is a span of breakpoints with strictly-ascending temp_c; between
 * breakpoints the duty is linearly interpolated (see fan_curve.h).
 */
struct CurvePoint
{
    int16_t temp_c;  ///< breakpoint temperature in degrees Celsius
    uint8_t duty;    ///< PWM duty (0-100 %) at temp_c
};

/**
 * @brief A project fan curve, defined per fan in config.h (FanCurve).
 *
 * An empty curve means the fan has no autonomous curve: update() leaves its
 * duty untouched (VDM-commanded PWM only).
 */
using Curve = std::span<const CurvePoint>;

/**
 * @brief "No valid reading" marker for a Driver::update() temperature.
 *
 * A fan whose temp source reports this falls back to its config.h
 * FanDefaultDuty (the fail-safe duty).
 */
constexpr int8_t InvalidTemp = INT8_MIN;

/**
 * @brief Fan control mode (FanControl VDM ops GetFanMode / SetFanMode).
 *
 * Auto is the boot default (value 0 so the zero-initialized mode variable
 * starts there). Any manual PWM command (VDM Set-PWM / stop) switches to
 * Manual, which freezes update(); SetFanMode(Auto) hands the fans back to
 * the curves on the next thermal poll.
 */
enum class FanMode : uint8_t
{
    Auto   = 0,  // fan curves drive the PWM
    Manual = 1,  // VDM-commanded PWM
};

}  // namespace nv::fancontrol
