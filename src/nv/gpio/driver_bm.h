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
#pragma once

#include <array>
#include <cstdint>

#include "nv/gpio/common.h"

namespace nv::gpio::bm {

/**
 * @brief Light weight GPIO HAL for SRAM-constrained baremetal cores.
 */
class Driver
{
public:
    static Status  write(GpioPort port, GpioPin pin, uint8_t data);
    static uint8_t read(GpioPort port, GpioPin pin);
    static Status
    init_interrupt(GpioPort port, GpioPin pin, InterruptDetection det, InterruptSelect select);

private:
    constexpr static std::array<uint32_t, sys::gpio::PortsNumber> ValidPinMasks{
        sys::gpio::Gpio0ValidMask,
        sys::gpio::Gpio1ValidMask,
        sys::gpio::Gpio2ValidMask,
        sys::gpio::Gpio3ValidMask,
        sys::gpio::Gpio4ValidMask,
        sys::gpio::Gpio5ValidMask};

    constexpr static bool is_pin_valid(GpioPort port, GpioPin pin)
    {
        if (pin >= sys::gpio::PinsPerPort || port >= sys::gpio::PortsNumber) {
            return false;
        }
        return (ValidPinMasks[port] & (1U << pin)) != 0;
    }
};

}  // namespace nv::gpio::bm
