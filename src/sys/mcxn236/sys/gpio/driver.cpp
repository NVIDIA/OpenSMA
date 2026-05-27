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

#include <array>
#include <FreeRTOSConfig.h>
#include <portmacrocommon.h>

#include "mpu_syscall_numbers.h"

#include "nv/common/utils.h"
#include "nv/nv.h"
#include "sys/common/utils.h"

using namespace nv::gpio;

#if defined(__cplusplus)
extern "C" {
#endif

NV_PRIVILEGED_FUNCTION Status Gpio_Init_Interrupt_Priv(GpioPort           port,
                                                       GpioPin            pin,
                                                       InterruptDetection det,
                                                       InterruptSelect    select)
{
    return Driver::init_interrupt_impl(port, pin, det, select);
}
#if defined(__cplusplus)
}
#endif

void sys::gpio::Driver::init()
{
    for (uint32_t i = 0; i < sys::gpio::PortsNumber; i++) {
        GPIO_Type* inst = get_gpio_instance(i);
        GPIO_EnablePinControlNonPrivilege(inst, ValidPinMasks.at(i));
        GPIO_EnableInterruptControlNonPrivilege(inst, 1);
    }
}

void Driver::init()
{
    sys::gpio::Driver::init();
}

GPIO_Type* sys::gpio::Driver::get_gpio_instance(nv::gpio::GpioPort port)
{
    (nv::common::assert)(port < sys::gpio::PortsNumber);
    switch (port) {
        case 0: {
            return GPIO0;
        } break;
        case 1: {
            return GPIO1;
        } break;
        case 2: {
            return GPIO2;
        } break;
        case 3: {
            return GPIO3;
        } break;
        case 4: {
            return GPIO4;
        } break;
        case 5: {
            return GPIO5;
        } break;
        default: break;
    }

    return nullptr;
}

PORT_Type* sys::gpio::Driver::get_port_instance(nv::gpio::GpioPort port)
{
    (nv::common::assert)(port < sys::gpio::PortsNumber);
    switch (port) {
        case 0: {
            return PORT0;
        } break;
        case 1: {
            return PORT1;
        } break;
        case 2: {
            return PORT2;
        } break;
        case 3: {
            return PORT3;
        } break;
        case 4: {
            return PORT4;
        } break;
        case 5: {
            return PORT5;
        } break;
        default: break;
    }

    return nullptr;
}

Status Driver::init_nonpriv_access(GpioPort port, GpioPin pin)
{
    if (!is_pin_valid(port, pin)) {
        nv::info("port %d Pin %d not valid\n", port, pin);
        return Status::InvalidParam;
    }

    GPIO_Type* inst = get_gpio_instance(port);
    GPIO_EnablePinControlNonPrivilege(inst, (1U << pin));
    GPIO_EnableInterruptControlNonPrivilege(inst, 1);

    return Status::Ok;
}

Status Driver::init_pin(GpioPort port, GpioPin pin, Direction dir, GpioState gpio_state)
{
    // nv::info("init pin:%d port:%d\n", pin, port);

    if (!is_pin_valid(port, pin)) {
        nv::info("port %d Pin %d not valid\n", port, pin);
        return Status::InvalidParam;
    }

    GPIO_Type*                 inst   = get_gpio_instance(port);
    const gpio_pin_direction_t Kdir   = dir == Direction::Input ? kGPIO_DigitalInput
                                                                : kGPIO_DigitalOutput;
    const gpio_pin_config_t    Config = {.pinDirection = Kdir,
                                         .outputLogic  = std::to_underlying(gpio_state)};

    GPIO_PinInit(inst, pin, &Config);

    return Status::Ok;
}

Status Driver::init_pin_cfg(GpioPort         port,
                            GpioPin          pin,
                            GpioPullDir      pullDir,
                            GpioPullStrength pullStrength,
                            GpioOpenDrain    openDrain)
{
    if (!is_pin_valid(port, pin)) {
        nv::info("port %d Pin %d not valid\n", port, pin);
        return Status::InvalidParam;
    }

    auto _pullDir = kPORT_PullDisable;
    switch (pullDir) {
        case GpioPullDir::PullDown: {
            _pullDir = kPORT_PullDown;
        } break;
        case GpioPullDir::PullUp: {
            _pullDir = kPORT_PullUp;
        } break;
        case GpioPullDir::Disabled: {
            _pullDir = kPORT_PullDisable;
        } break;
        default: {
            return Status::InvalidParam;
        } break;
    }

    auto _pullStrength = (pullStrength == GpioPullStrength::Low) ? kPORT_LowDriveStrength
                                                                 : kPORT_HighDriveStrength;
    auto _openDrain    = (openDrain == GpioOpenDrain::Disable) ? kPORT_OpenDrainDisable
                                                               : kPORT_OpenDrainEnable;

    const port_pin_config_t cfg = {
        .pullSelect          = static_cast<uint16_t>(_pullDir),
        .pullValueSelect     = static_cast<uint16_t>(_pullStrength),
        .slewRate            = static_cast<uint16_t>(kPORT_FastSlewRate),
        .passiveFilterEnable = static_cast<uint16_t>(kPORT_PassiveFilterDisable),
        .openDrainEnable     = static_cast<uint16_t>(_openDrain),
        .driveStrength       = static_cast<uint16_t>(kPORT_LowDriveStrength),
        .mux                 = static_cast<uint16_t>(kPORT_MuxAlt0),
        .inputBuffer         = static_cast<uint16_t>(kPORT_InputBufferEnable),
        .invertInput         = static_cast<uint16_t>(kPORT_InputNormal),
        .lockRegister        = static_cast<uint16_t>(kPORT_UnlockRegister),
    };

    PORT_SetPinConfig(get_port_instance(port), pin, &cfg);

    return Status::Ok;
}

Status Driver::init_interrupt(GpioPort           port,
                              GpioPin            pin,
                              InterruptDetection det,
                              InterruptSelect    select)
{
    return init_interrupt_svc(port, pin, det, select);
}

NV_SYS_CALL Status Driver::init_interrupt_svc(GpioPort           port,
                                              GpioPin            pin,
                                              InterruptDetection det,
                                              InterruptSelect    select)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern Gpio_Init_Interrupt_Priv                      \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_Gpio_Init_Interrupt                    \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_Gpio_Init_Interrupt                  \n"
        " Privileged_Gpio_Init_Interrupt:                       \n"
        "     pop {r0}                                          \n"
        "     b Gpio_Init_Interrupt_Priv                        \n"
        " Unprivileged_Gpio_Init_Interrupt:                     \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_Gpio_Init_Interrupt)
        : "memory");
#else
    return Gpio_Init_Interrupt_Priv(port, pin, det, select);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION Status Driver::init_interrupt_impl(GpioPort           port,
                                                          GpioPin            pin,
                                                          InterruptDetection det,
                                                          InterruptSelect    select)
{
    if (!is_pin_valid(port, pin)) {
        nv::info("port %d Pin %d not valid\n", port, pin);
        return Status::InvalidParam;
    }

    GPIO_Type*                 inst = get_gpio_instance(port);
    gpio_interrupt_config_t    config{};
    gpio_interrupt_selection_t sel_config{};
    sel_config = select == InterruptSelect::InterruptSelect0 ? kGPIO_InterruptOutput0
                                                             : kGPIO_InterruptOutput1;

    switch (det) {
        case InterruptDetection::InterruptRising: {
            config = kGPIO_InterruptRisingEdge;
        } break;
        case InterruptDetection::InterruptFalling: {
            config = kGPIO_InterruptFallingEdge;
        } break;
        case InterruptDetection::InterruptBothEdge: {
            config = kGPIO_InterruptEitherEdge;
        } break;
        case InterruptDetection::InterruptHigh: {
            config = kGPIO_InterruptLogicOne;
        } break;
        case InterruptDetection::InterruptLow: {
            config = kGPIO_InterruptLogicZero;
        } break;
        case InterruptDetection::InterruptDisabled: {
            config = kGPIO_InterruptStatusFlagDisabled;
        } break;
        default: {
            return Status::InvalidParam;
        } break;
    }
    GPIO_SetPinInterruptConfig(inst, pin, config);
    GPIO_SetPinInterruptChannel(inst, pin, sel_config);

    auto irq = static_cast<IRQn_Type>(GPIO00_IRQn + (port * sys::gpio::InterruptPerPort)
                                      + std::to_underlying(select));
    EnableIRQ(irq);

    // nv::info("init_interrupt inst:0x%x pin:%d\n", inst, pin);

    return Status::Ok;
}

// Capable to read both input and output pin value
Status Driver::read(GpioPort port, GpioPin pin, uint8_t& data)
{
    if (!is_pin_valid(port, pin)) {
        nv::info("port %d Pin %d not valid\n", port, pin);
        return Status::InvalidParam;
    }

    GPIO_Type* inst    = get_gpio_instance(port);
    uint32_t   ReadVal = 0;

    if (inst->PDDR & (1u << pin)) {
        // Output pin
        ReadVal = inst->PDOR & (1u << pin) ? 1 : 0;
    }
    else {
        // Input pin
        ReadVal = GPIO_PinRead(inst, pin);
    }

    data = static_cast<uint8_t>(ReadVal);

    // nv::info("pin:%d Read_val:%d\n", pin, ReadVal);

    return Status::Ok;
}

Status Driver::read_force(GpioPort port, GpioPin pin, uint8_t& data)
{
    if (!is_pin_valid(port, pin)) {
        nv::info("port %d Pin %d not valid\n", port, pin);
        return Status::InvalidParam;
    }

    constexpr std::array gpio_clocks = {
        kCLOCK_Gpio0, kCLOCK_Gpio1, kCLOCK_Gpio2, kCLOCK_Gpio3, kCLOCK_Gpio4};
    constexpr std::array port_clocks = {
        kCLOCK_Port0, kCLOCK_Port1, kCLOCK_Port2, kCLOCK_Port3, kCLOCK_Port4};
    static_assert(gpio_clocks.size() == port_clocks.size());

    if (port >= gpio_clocks.size()) {
        return Status::Error;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    CLOCK_EnableClock(gpio_clocks[port]);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    CLOCK_EnableClock(port_clocks[port]);

    PORT_Type* port_inst = get_port_instance(port);
    GPIO_Type* gpio_inst = get_gpio_instance(port);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    const auto saved_pcr  = port_inst->PCR[pin];
    const auto saved_pddr = gpio_inst->PDDR;

    const port_pin_config_t gpio_input_cfg = {
        .pullSelect          = static_cast<uint16_t>(kPORT_PullDisable),
        .pullValueSelect     = static_cast<uint16_t>(kPORT_LowDriveStrength),
        .slewRate            = static_cast<uint16_t>(kPORT_FastSlewRate),
        .passiveFilterEnable = static_cast<uint16_t>(kPORT_PassiveFilterDisable),
        .openDrainEnable     = static_cast<uint16_t>(kPORT_OpenDrainDisable),
        .driveStrength       = static_cast<uint16_t>(kPORT_LowDriveStrength),
        .mux                 = static_cast<uint16_t>(kPORT_MuxAlt0),
        .inputBuffer         = static_cast<uint16_t>(kPORT_InputBufferEnable),
        .invertInput         = static_cast<uint16_t>(kPORT_InputNormal),
        .lockRegister        = static_cast<uint16_t>(kPORT_UnlockRegister),
    };
    PORT_SetPinConfig(port_inst, pin, &gpio_input_cfg);

    gpio_inst->PDDR &= ~(1u << pin);

    SDK_DelayAtLeastUs(5, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

    data = static_cast<uint8_t>(GPIO_PinRead(gpio_inst, pin));

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    port_inst->PCR[pin] = saved_pcr;
    gpio_inst->PDDR     = saved_pddr;

    return Status::Ok;
}

Status Driver::read_gpio_port(GpioPort port, uint32_t& gpioBitmap)
{
    if (port >= sys::gpio::PortsNumber) {
        return Status::InvalidParam;
    }

    GPIO_Type* inst = get_gpio_instance(port);
    if (inst == nullptr) {
        return Status::Error;
    }

    gpioBitmap = inst->PDIR;

    return Status::Ok;
}

Status Driver::write(GpioPort port, GpioPin pin, const uint8_t data)
{
    if (!is_pin_valid(port, pin)) {
        nv::info("port %d Pin %d not valid\n", port, pin);
        return Status::InvalidParam;
    }

    GPIO_Type* inst = get_gpio_instance(port);
    GPIO_PinWrite(inst, pin, data);
    // nv::info("pin:%d Data to write:%d\n", pin, data);

    return Status::Ok;
}

Status Driver::getDirection(GpioPort port, GpioPin pin, Direction& dir)
{
    if (!is_pin_valid(port, pin)) {
        return Status::InvalidParam;
    }

    GPIO_Type* inst = get_gpio_instance(port);
    if (inst == nullptr) {
        return Status::Error;
    }

    // Check PDDR (Pin Data Direction Register): 1 = output, 0 = input
    const bool is_output = (inst->PDDR & (1u << pin)) != 0;
    dir                  = is_output ? Direction::Output : Direction::Input;

    return Status::Ok;
}
