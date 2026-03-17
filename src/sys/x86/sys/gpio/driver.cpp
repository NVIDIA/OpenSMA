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
#include "nv/gpio/driver.h"

using namespace nv::gpio;

// NOLINTBEGIN

void sys::gpio::Driver::init() {}

void Driver::init()
{
    sys::gpio::Driver::init();
}

Status Driver::init_nonpriv_access(GpioPort port, GpioPin pin)
{
    return Status::Ok;
}

Status Driver::init_pin_cfg(GpioPort         port,
                            GpioPin          pin,
                            GpioPullDir      pullDir,
                            GpioPullStrength pullStrength,
                            GpioOpenDrain    openDrain)
{
    return Status::Ok;
}

Status Driver::init_pin(GpioPort port, GpioPin pin, Direction dir, GpioState gpio_state)
{
    return Status::Ok;
}

Status Driver::init_interrupt(GpioPort           port,
                              GpioPin            pin,
                              InterruptDetection det,
                              InterruptSelect    select)
{
    return Status::Ok;
}

Status Driver::read(GpioPort port, GpioPin pin, uint8_t& data)
{
    data = 0;
    return Status::Ok;
}

Status Driver::read_gpio_port(GpioPort port, uint32_t& gpioBitmap)
{
    gpioBitmap = 0;
    return Status::Ok;
}

Status Driver::write(GpioPort port, GpioPin pin, const uint8_t data)
{
    return Status::Ok;
}

Status Driver::getDirection(GpioPort port, GpioPin pin, Direction& dir)
{
    dir = Direction::Input;
    return Status::Ok;
}
// NOLINTEND
