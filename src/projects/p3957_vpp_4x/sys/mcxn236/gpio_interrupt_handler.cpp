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

#include NV_IPC_CONFIG_H
#include "nhp_config.h"

#include "nv/gpio/common.h"
#include "nv/nv.h"
#include "nv/vpp/task.h"
#include "sys/gpio/constant.h"
#include "sys/gpio/driver.h"

// returns if the port and flag (pin bitmask) combo is in the inputPins struct
// also gives the pin number it matched with
bool isInInputPins(nv::gpio::GpioPort    port,
                   uint32_t              flag,
                   nv::nhp::E1sInputPins inputPins,
                   nv::gpio::GpioPin&    pinReturn)
{
    if ((port == inputPins.prsnt_l_port)
        && (flag & nv::common::bit<uint32_t>(inputPins.prsnt_l_pin))) {
        pinReturn = inputPins.prsnt_l_pin;
        return true;
    } /*
     else if ((port == inputPins.pgood_port)
              && (flag & nv::common::bit<uint32_t>(inputPins.pgood_pin))) {
         pinReturn = inputPins.pgood_pin;
         return true;
     }*/
    return false;
}

// returns true, an nhp task pointer, and a callback for the given gpio pin
// returns false if pin doesnt match any nhp instance
bool getNhpGpioCallback(nv::gpio::GpioPort port,
                        uint32_t           flag,
                        uint8_t&           nhpInstance,
                        uint8_t&           driveIndex,
                        nv::gpio::GpioPin& pinReturn)
{
    for (int i = 0; i < nv::nhp::NumE1sDrives; i++) {
        if (isInInputPins(port, flag, nv::nhp::Nhp0driveInputPins.at(i), pinReturn)) {
            nhpInstance = 0;
            // driveIndex  = i + (nv::nhp::NumE1sDrives * nhpInstance);
            driveIndex = i;
            return true;
        }
        else if (isInInputPins(port, flag, nv::nhp::Nhp1driveInputPins.at(i), pinReturn)) {
            nhpInstance = 1;
            // driveIndex  = i + (nv::nhp::NumE1sDrives * nhpInstance);
            driveIndex = i;
            return true;
        }
        else if (isInInputPins(port, flag, nv::nhp::Nhp2driveInputPins.at(i), pinReturn)) {
            nhpInstance = 2;
            // driveIndex  = i + (nv::nhp::NumE1sDrives * nhpInstance);
            driveIndex = i;
            return true;
        }
        else if (isInInputPins(port, flag, nv::nhp::Nhp3driveInputPins.at(i), pinReturn)) {
            nhpInstance = 3;
            // driveIndex  = i + (nv::nhp::NumE1sDrives * nhpInstance);
            driveIndex = i;
            return true;
        }
    }
    return false;
}

extern "C" {
// NOLINTBEGIN

void GPIO_ALL_IRQHandler(int8_t port, uint8_t irq, GPIO_Type* inst)
{
    // nv::info("GPIO%d%d_IRQHandler\n", port, irq);
    uint32_t flag = GPIO_GpioGetInterruptChannelFlags(inst, irq);
    // nv::info("flag:0x%x\n", flag);

    nv::gpio::GpioPin pin           = 0;
    uint8_t           nhpInstance   = 0;
    uint8_t           nhpDriveIndex = 0;
    if (getNhpGpioCallback(port, flag, nhpInstance, nhpDriveIndex, pin)) {
        nv::vpp::Task::gpio_interrupt_callback(port, pin, nhpInstance, nhpDriveIndex);
        GPIO_GpioClearInterruptChannelFlags(inst, 1U << pin, irq);
    }
    else {
        nv::warn("Interrupt from invalid pin\n");
        GPIO_GpioClearInterruptChannelFlags(inst, flag, irq);
    }
}

// GPIO0

void GPIO00_IRQHandler()
{
    uint8_t    port = 0;
    uint8_t    irq  = 0;
    GPIO_Type* inst = GPIO0;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

void GPIO01_IRQHandler()
{
    uint8_t    port = 0;
    uint8_t    irq  = 1;
    GPIO_Type* inst = GPIO0;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

// GPIO 1

void GPIO10_IRQHandler()
{
    uint8_t    port = 1;
    uint8_t    irq  = 0;
    GPIO_Type* inst = GPIO1;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

void GPIO11_IRQHandler()
{
    uint8_t    port = 1;
    uint8_t    irq  = 1;
    GPIO_Type* inst = GPIO1;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

// GPIO 2

void GPIO20_IRQHandler()
{
    uint8_t    port = 2;
    uint8_t    irq  = 0;
    GPIO_Type* inst = GPIO2;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

void GPIO21_IRQHandler()
{
    uint8_t    port = 2;
    uint8_t    irq  = 1;
    GPIO_Type* inst = GPIO2;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

// GPIO 3

void GPIO30_IRQHandler()
{
    uint8_t    port = 3;
    uint8_t    irq  = 0;
    GPIO_Type* inst = GPIO3;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

void GPIO31_IRQHandler()
{
    uint8_t    port = 3;
    uint8_t    irq  = 1;
    GPIO_Type* inst = GPIO3;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

// GPIO 4

void GPIO40_IRQHandler()
{
    uint8_t    port = 4;
    uint8_t    irq  = 0;
    GPIO_Type* inst = GPIO4;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

void GPIO41_IRQHandler()
{
    uint8_t    port = 4;
    uint8_t    irq  = 1;
    GPIO_Type* inst = GPIO4;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

// GPIO 5

void GPIO50_IRQHandler()
{
    uint8_t    port = 5;
    uint8_t    irq  = 0;
    GPIO_Type* inst = GPIO5;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

void GPIO51_IRQHandler()
{
    uint8_t    port = 5;
    uint8_t    irq  = 1;
    GPIO_Type* inst = GPIO5;
    GPIO_ALL_IRQHandler(port, irq, inst);
}

// ADC 0
void ADC0_IRQHandler(void)
{
    ADC_Type* baseAddr = ADC0;
    uint32_t  flags    = LPADC_GetStatusFlags(baseAddr);
    uint32_t  trigger  = LPADC_GetTriggerStatusFlags(baseAddr);
    LPADC_ClearTriggerStatusFlags(baseAddr, trigger);
    LPADC_ClearStatusFlags(baseAddr, flags);

    if (flags & kLPADC_ResultFIFO0OverflowFlag) {
        nv::error("FIFO 0 Overflow\n");
        // TODO reset fifo and exit
    }
    if (flags & kLPADC_ResultFIFO1OverflowFlag) {
        nv::error("FIFO 1 overflow\n");
        // TODO reset fifo and exit
    }
    if (flags & kLPADC_TriggerExceptionFlag) {
        nv::error("Trigger Exception\n");
        // TODO reset fifo and exit
    }
    // TODO make this scalable for N number of NHP tasks:
    // if we add a mapping of command -> task pointer in nhp_config.h and read the command off
    // the ADC fifo return struct and pass the whole struct to the callback
    if (flags & kLPADC_TriggerCompletionFlag) {
        if (trigger & kLPADC_Trigger0CompletedFlag) {
            nv::vpp::Task::adc_interrupt_callback(0U, 0U, 0U);
        }
        if (trigger & kLPADC_Trigger1CompletedFlag) {
            nv::vpp::Task::adc_interrupt_callback(1U, 0U, 1U);
        }
    }
}
}
// NOLINTEND
