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
#include <assert.h>

#include "fsl_gpio.h"
#include "fsl_port.h"

#include "nv/gpio/common.h"
#include "sys/gpio/constant.h"

namespace sys::gpio {

class Driver
{
public:
    template<uint32_t port, uint32_t pin>
    static void write(uint8_t data)
    {
        static_assert(is_pin_valid(port, pin), "Invalid pin");
        GPIO_PinWrite(std::to_array(GPIO_BASE_PTRS)[port], pin, data);
    }

    template<uint32_t port, uint32_t pin>
    static uint8_t read()
    {
        static_assert(is_pin_valid(port, pin), "Invalid pin");
        return GPIO_PinRead(std::to_array(GPIO_BASE_PTRS)[port], pin);
    }

protected:
    static void init();

    constexpr static std::array<uint32_t, NumInstance> ValidPinMasks{Gpio0ValidMask,
                                                                     Gpio1ValidMask,
                                                                     Gpio2ValidMask,
                                                                     Gpio3ValidMask,
                                                                     Gpio4ValidMask,
                                                                     Gpio5ValidMask};

    constexpr static bool is_pin_valid(nv::gpio::GpioPort port, nv::gpio::GpioPin pin)
    {
        if (pin >= PinsPerPort || port >= std::size(GPIO_BASE_ADDRS)) {
            return false;
        }
        return (ValidPinMasks[port] & (1U << pin)) != 0;
    }

    static GPIO_Type* get_gpio_instance(nv::gpio::GpioPort port);

    static PORT_Type* get_port_instance(nv::gpio::GpioPort port);
};

}  // namespace sys::gpio
