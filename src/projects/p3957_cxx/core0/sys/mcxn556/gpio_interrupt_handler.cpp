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
#include "board/peripherals.h"

#include "nv/gpio/common.h"
#include "nv/nv.h"
#include "nv/ahs/task.h"
#include "nv/volt_mon/ahs/mcu_internal_temp.h"
#include "sys/adc/adc.h"
#include "sys/gpio/constant.h"
#include "sys/gpio/driver.h"

// returns if the peripheral and command combo is in the inputPins struct
bool isInInputAdcPins(uint8_t peripheral, uint32_t command, nv::nhp::E1sInputPins inputPins)
{
    if ((peripheral == inputPins.pgood_peripheral) && (command == inputPins.pgood_channel)) {
        return true;
    }
    return false;
}

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
    }
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
#if (NUM_HOTPLUG_INSTANCES > 1U)
        else if (isInInputPins(port, flag, nv::nhp::Nhp1driveInputPins.at(i), pinReturn)) {
            nhpInstance = 1;
            // driveIndex  = i + (nv::nhp::NumE1sDrives * nhpInstance);
            driveIndex = i;
            return true;
        }
#endif
#if (NUM_HOTPLUG_INSTANCES > 2U)
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
#endif
    }
    return false;
}

// returns true, an nhp task pointer, and a callback for the given gpio pin
// returns false if pin doesnt match any nhp instance
bool getNhpAdcCallback(uint8_t  peripheral,
                       uint32_t command,
                       uint8_t& nhpInstance,
                       uint8_t& driveIndex)
{
    for (int i = 0; i < nv::nhp::NumE1sDrives; i++) {
        if (isInInputAdcPins(peripheral, command, nv::nhp::Nhp0driveInputPins.at(i))) {
            nhpInstance = 0;
            driveIndex  = i;
            return true;
        }
#if (NUM_HOTPLUG_INSTANCES > 1U)
        if (isInInputAdcPins(peripheral, command, nv::nhp::Nhp1driveInputPins.at(i))) {
            nhpInstance = 1;
            driveIndex  = i;
            return true;
        }
#endif
#if (NUM_HOTPLUG_INSTANCES > 2U)
        if (isInInputAdcPins(peripheral, command, nv::nhp::Nhp2driveInputPins.at(i))) {
            nhpInstance = 2;
            driveIndex  = i;
            return true;
        }
        if (isInInputAdcPins(peripheral, command, nv::nhp::Nhp3driveInputPins.at(i))) {
            nhpInstance = 3;
            driveIndex  = i;
            return true;
        }
#endif
    }
    return false;
}

void process_pgood_adc_result(sys::adc::AdcPeripheral             peripheral,
                              const sys::adc::ADC::AdcConvResult& result)
{
    uint8_t nhpInstance   = 0;
    uint8_t nhpDriveIndex = 0;
    if (getNhpAdcCallback(peripheral, result.commandIdSource, nhpInstance, nhpDriveIndex)) {
        nv::ahs::Task::adc_interrupt_callback(result.convValue, nhpInstance, nhpDriveIndex);
    }
}

void drain_adc_fifo(sys::adc::AdcPeripheral peripheral, uint8_t fifo_num)
{
    sys::adc::ADC::AdcConvResult discard{};
    while (sys::adc::ADC::get_fifo_count(peripheral, fifo_num) != 0) {
        if (!sys::adc::ADC::get_adc_reading(peripheral, discard, fifo_num)) {
            break;
        }
    }
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
        nv::ahs::Task::gpio_interrupt_callback(port, pin, nhpInstance, nhpDriveIndex);
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

void ADC_Common_IRQHandler(sys::adc::AdcPeripheral peripheral, uint8_t fifo_num)
{
    // Generic PGOOD/VMON drain for Trigger 0/1/2 (no MCU-temp CMD15 on these chains).
    constexpr int MaxIterations = 1000;
    int           iterations    = 0;

    while (sys::adc::ADC::get_fifo_count(peripheral, fifo_num) != 0
           && iterations++ < MaxIterations) {
        sys::adc::ADC::AdcConvResult result{};
        if (!sys::adc::ADC::get_adc_reading(peripheral, result, fifo_num)) {
            nv::error("Failed to read ADC FIFO\n");
            return;
        }
        process_pgood_adc_result(peripheral, result);
    }
}

// ADC0 Trigger 3 → FIFO1: fixed batch CMD7, CMD8, CMD15(VBE1), CMD15(VBE8).
void ADC_Trigger3_IRQHandler(sys::adc::AdcPeripheral peripheral)
{
    constexpr uint8_t  Fifo1         = 1U;
    constexpr uint32_t ExpectedCount = 4U;

    if (sys::adc::ADC::get_fifo_count(peripheral, Fifo1) != ExpectedCount) {
        nv::error("ADC0 Trig3: unexpected FIFO1 count\n");
        drain_adc_fifo(peripheral, Fifo1);
        return;
    }

    sys::adc::ADC::AdcConvResult result{};

    // 1) CMD7 — SSD6 PGOOD
    if (!sys::adc::ADC::get_adc_reading(peripheral, result, Fifo1)
        || result.commandIdSource != ADC0_CMD_VMON_SSD6) {
        nv::error("ADC0 Trig3: expected CMD7\n");
        drain_adc_fifo(peripheral, Fifo1);
        return;
    }
    process_pgood_adc_result(peripheral, result);

    // 2) CMD8 — SSD7 PGOOD
    if (!sys::adc::ADC::get_adc_reading(peripheral, result, Fifo1)
        || result.commandIdSource != ADC0_CMD_VMON_SSD7) {
        nv::error("ADC0 Trig3: expected CMD8\n");
        drain_adc_fifo(peripheral, Fifo1);
        return;
    }
    process_pgood_adc_result(peripheral, result);

    // 3) CMD15 — VBE1
    if (!sys::adc::ADC::get_adc_reading(peripheral, result, Fifo1)
        || result.commandIdSource != ADC0_CMD_INTERNAL_TEMP) {
        nv::error("ADC0 Trig3: expected CMD15 VBE1\n");
        drain_adc_fifo(peripheral, Fifo1);
        return;
    }
    const uint16_t vbe1 = result.convValue;

    // 4) CMD15 — VBE8
    if (!sys::adc::ADC::get_adc_reading(peripheral, result, Fifo1)
        || result.commandIdSource != ADC0_CMD_INTERNAL_TEMP) {
        nv::error("ADC0 Trig3: expected CMD15 VBE8\n");
        drain_adc_fifo(peripheral, Fifo1);
        return;
    }
    const uint16_t vbe8 = result.convValue;

    nv::mcu_internal_temp::ahs::accumulate_pair_from_isr(vbe1, vbe8);
}

// ADC n
void ADCn_IRQHandler(ADC_Type* baseAddr, uint8_t peripheral)
{
    // nv::debug("ADCn IRQ Handler: %d\n", peripheral);
    uint32_t flags   = LPADC_GetStatusFlags(baseAddr);
    uint32_t trigger = LPADC_GetTriggerStatusFlags(baseAddr);
    LPADC_ClearTriggerStatusFlags(baseAddr, trigger);
    LPADC_ClearStatusFlags(baseAddr, flags);

    // Check for various ADC errors and exceptions
    if (flags & kLPADC_ResultFIFO0OverflowFlag) {
        nv::error("FIFO 0 Overflow\n");
    }
    if (flags & kLPADC_ResultFIFO1OverflowFlag) {
        nv::error("FIFO 1 overflow\n");
    }
    if (flags & kLPADC_TriggerExceptionFlag) {
        nv::error("Trigger Exception\n");
    }
    if (flags & kLPADC_TriggerCompletionFlag) {
        if (trigger & kLPADC_Trigger0CompletedFlag) {
            ADC_Common_IRQHandler(peripheral, 0U);
        }
        if (trigger & kLPADC_Trigger1CompletedFlag) {
            ADC_Common_IRQHandler(peripheral, 1U);
        }
        if (trigger & kLPADC_Trigger2CompletedFlag) {
            // ADC1 Trigger 2 → FIFO0: AHS SSD4/SSD5 VMON.
            if (peripheral == 1U) {
                ADC_Common_IRQHandler(peripheral, 0U);
            }
        }
        if (trigger & kLPADC_Trigger3CompletedFlag) {
            // ADC0 Trigger 3 → FIFO1: SSD6/SSD7 PGOOD + CMD15 MCU temp.
            if (peripheral == 0U) {
                ADC_Trigger3_IRQHandler(peripheral);
            }
        }
    }
}

void ADC0_IRQHandler()
{
    ADCn_IRQHandler(ADC0, 0U);
}

void ADC1_IRQHandler()
{
    ADCn_IRQHandler(ADC1, 1U);
}

}  // end of extern "C"

// Board/Project Specific ADC Trigger Routine for HotPlug

void projectTryRunAdcTrigger(void)
{
    // Publish any completed temp window before starting the next soft-trigger.
    nv::mcu_internal_temp::ahs::service();

    static uint8_t trigger = 0;

    switch (trigger) {
        case 0:
            if (sys::adc::ADC::adc_ready(0U)) {
                sys::adc::ADC::trigger_read(0U, (1 << trigger));
                trigger = 1;
            }
            break;
        case 1:
            if (sys::adc::ADC::adc_ready(0U)) {
                sys::adc::ADC::trigger_read(0U, (1 << trigger));
                trigger = 2;
            }
            break;
        case 2:
            if (sys::adc::ADC::adc_ready(1U)) {
                sys::adc::ADC::trigger_read(1U, (1 << trigger));
                trigger = 3;
            }
            break;
        case 3:
            if (sys::adc::ADC::adc_ready(0U)) {
                sys::adc::ADC::trigger_read(0U, (1 << trigger));
                trigger = 0;
            }
            break;
        default:
            nv::error("Invalid ADC trigger\n");
            trigger = 0;
            break;
    }
}

// NOLINTEND
