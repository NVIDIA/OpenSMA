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

#include "nv/gpio/driver_bm.h"

#include <array>
#include <utility>

#include "fsl_gpio.h"

namespace nv::gpio::bm {

Status Driver::write(GpioPort port, GpioPin pin, uint8_t data)
{
    if (!is_pin_valid(port, pin)) {
        return Status::InvalidParam;
    }

    // NOLINTNEXTLINE: SDK definition
    std::array<GPIO_Type*, sys::gpio::NumInstance> bases GPIO_BASE_PTRS;
    GPIO_PinWrite(bases.at(port), pin, data);

    return Status::Ok;
}

uint8_t Driver::read(GpioPort port, GpioPin pin)
{
    if (!is_pin_valid(port, pin)) {
        return 0;
    }

    // NOLINTNEXTLINE: SDK definition
    std::array<GPIO_Type*, sys::gpio::NumInstance> bases GPIO_BASE_PTRS;
    return GPIO_PinRead(bases.at(port), pin);
}

Status Driver::init_interrupt(GpioPort           port,
                              GpioPin            pin,
                              InterruptDetection det,
                              InterruptSelect    select)
{
    if (!is_pin_valid(port, pin)) {
        return Status::InvalidParam;
    }

    // NOLINTNEXTLINE: SDK definition
    std::array<GPIO_Type*, sys::gpio::NumInstance> bases GPIO_BASE_PTRS;
    GPIO_Type*                                           inst = bases.at(port);

    gpio_interrupt_config_t config{};
    switch (det) {
        case InterruptDetection::InterruptRising  : config = kGPIO_InterruptRisingEdge; break;
        case InterruptDetection::InterruptFalling : config = kGPIO_InterruptFallingEdge; break;
        case InterruptDetection::InterruptBothEdge: config = kGPIO_InterruptEitherEdge; break;
        case InterruptDetection::InterruptHigh    : config = kGPIO_InterruptLogicOne; break;
        case InterruptDetection::InterruptLow     : config = kGPIO_InterruptLogicZero; break;
        case InterruptDetection::InterruptDisabled:
            config = kGPIO_InterruptStatusFlagDisabled;
            break;
        default: return Status::InvalidParam;
    }

    gpio_interrupt_selection_t sel = (select == InterruptSelect::InterruptSelect0)
                                       ? kGPIO_InterruptOutput0
                                       : kGPIO_InterruptOutput1;

    GPIO_SetPinInterruptConfig(inst, pin, config);
    GPIO_SetPinInterruptChannel(inst, pin, sel);

    auto irq = static_cast<IRQn_Type>(GPIO00_IRQn + (port * sys::gpio::InterruptPerPort)
                                      + std::to_underlying(select));
    EnableIRQ(irq);

    return Status::Ok;
}

}  // namespace nv::gpio::bm
