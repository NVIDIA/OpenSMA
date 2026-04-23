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

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "nv/gpio/common.h"

namespace nv::iox {

enum class Status
{
    Ok,
    Error,

    InvalidOp,
    InvalidReg,
    InvalidRead,
    InvalidWrite,
};

/**
 * based on I2C protocol:
 *   0 - write
 *   1 - read
 */
enum class Operation
{
    Write,
    Read,

    Invalid,
};

enum class Register
{
    InputPort0  = 0,
    InputPort1  = 1,
    OutputPort0 = 2,
    OutputPort1 = 3,
    Polarity0   = 4,
    Polarity1   = 5,
    Config0     = 6,
    Config1     = 7,
    Invalid,
};

/**
 * @note: unUsed and vrPort are the same from the perspective of iox
 *        used to indicate there is NO real gpio associated with this PCA9555 pin
 *        The only difference is from user's perspective, just to make it more readable
 *        all pins defined as unUsed/vrPort always have an internal variable in the iox reg
 * table
 */
constexpr static nv::gpio::GpioPort vrPort = nv::gpio::InvalidGpioPort;

constexpr static uint8_t regNum = 8;
constexpr static uint8_t pinNum = 16;

enum class FilterEnable : uint8_t
{
    Disable = 0,
    Enable,
};

enum class DefaultValue : uint8_t
{
    Low  = 0,
    High = 1,
};

struct PinConfig
{
    nv::gpio::GpioPort port;
    nv::gpio::GpioPin  pin;

    nv::gpio::Direction dir;
    nv::gpio::GpioState val;

    nv::gpio::GpioPullDir      pullDir;
    nv::gpio::GpioPullStrength pullStrength;
    nv::gpio::GpioOpenDrain    openDrain;

    FilterEnable filter     = FilterEnable::Disable;
    DefaultValue defaultVal = DefaultValue::High;
};

using I2CAddress = uint8_t;

struct IoxConfig
{
    I2CAddress                    addr;
    std::array<PinConfig, pinNum> pinConfig;

    template<typename... Args>
    requires(sizeof...(Args) == pinNum) && (std::is_convertible_v<Args, PinConfig> && ...)
    constexpr IoxConfig(I2CAddress address, Args... args)
    : addr(address)
    , pinConfig{{PinConfig(std::forward<Args>(args))...}}
    {
        static_assert(sizeof...(Args) == pinNum,
                      "Must provide exactly pinNum PinConfig elements");
    }
};

}  // namespace nv::iox