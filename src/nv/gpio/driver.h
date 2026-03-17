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
#include "nv/gpio/common.h"
#include "sys/gpio/driver.h"

namespace nv::gpio {

class Driver : protected sys::gpio::Driver
{
public:
    Driver();

    static void init();

    static Status init_nonpriv_access(GpioPort port, GpioPin pin);
    static Status init_pin(GpioPort port, GpioPin pin, Direction dir, GpioState gpio_state);
    static Status init_pin_cfg(GpioPort         port,
                               GpioPin          pin,
                               GpioPullDir      pullDir,
                               GpioPullStrength pullStrength,
                               GpioOpenDrain    openDrain);

    static Status
    init_interrupt(GpioPort port, GpioPin pin, InterruptDetection det, InterruptSelect select);

    static Status init_interrupt_impl(GpioPort           port,
                                      GpioPin            pin,
                                      InterruptDetection det,
                                      InterruptSelect    select);
    static Status init_interrupt_svc(GpioPort           port,
                                     GpioPin            pin,
                                     InterruptDetection det,
                                     InterruptSelect    select);

    static Status read(GpioPort port, GpioPin pin, uint8_t& data);
    static Status read_virtual_physical_gpio(GpioPort port, GpioPin pin, uint8_t& data);
    static Status read_gpio_port(GpioPort port, uint32_t& gpioBitmap);
    static Status write(GpioPort port, GpioPin pin, const uint8_t Data);
    static Status getDirection(GpioPort port, GpioPin pin, Direction& dir);
};

}  // namespace nv::gpio