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
#pragma once
#include <cstdint>
namespace nv::gpio {
using GpioPin  = uint32_t;
using GpioPort = uint32_t;

constexpr static GpioPin  InvalidGpioPin  = UINT32_MAX;
constexpr static GpioPort InvalidGpioPort = UINT32_MAX;

enum class Direction : uint8_t
{
    Input,
    Output,
};

enum class Status : uint32_t
{
    Ok,
    Error,
    InvalidParam,
};

enum class InterruptDetection : uint8_t
{
    InterruptRising = 0,
    InterruptFalling,
    InterruptBothEdge,
    InterruptHigh,
    InterruptLow,
    InterruptDisabled,
};

enum class InterruptSelect : uint8_t
{
    InterruptSelect0 = 0,
    InterruptSelect1 = 1,
};

enum class GpioState : uint8_t
{
    Low  = 0,
    High = 1,
};

enum class GpioPullStrength : uint16_t
{
    Low  = 0,
    High = 1,
};

enum class GpioPullDir : uint16_t
{
    PullDown = 0,
    PullUp   = 1,
    Disabled = 2,
};

enum class GpioOpenDrain : uint16_t
{
    Disable = 0,
    Enable  = 1,
};

}  // namespace nv::gpio
