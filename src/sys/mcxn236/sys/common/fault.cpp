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
#include <cstdint>

#include "FreeRTOS.h"
#include "task.h"

#include "nv/logger/log.h"
#include "nv/logger/log_fault.h"
#include "nv/nv.h"

#include "nv/watchdog/runtime.h"
#include NV_IPC_CONFIG_H

extern "C" {

// NOLINTBEGIN
#define HFSR_REGISTER_ADDRESS  (0xE000ED2CU)
#define MMFSR_REGISTER_ADDRESS (0xE000ED28U)
#define MMFAR_REGISTER_ADDRESS (0xE000ED34U)
#define BFAR_REGISTER_ADDRESS  (0xE000ED38U)

#if 0
static inline uint32_t __get_PSP(void)
{
    uint32_t result;
    asm volatile("MRS %0, psp" : "=r"(result));
    return (result);
}
#endif

void NMI_Handler(void)
{
    nv::fatal("NMI_Handler\r\n");
}

void Log_Fault(nv::logger::Fault fault, uint32_t lr_value, uint32_t msp)
{
    // coverity[cert_int36_c_violation] casting PSP to uint32_t*
    uint32_t* PSP = (uint32_t*)(__get_PSP());
    // coverity[cert_int36_c_violation] casting MSP to uint32_t*
    uint32_t*          MSP        = (uint32_t*)(msp);
    constexpr uint32_t SPSEL_MASK = 0x1 << 2;
    bool               is_msp     = (lr_value & SPSEL_MASK) == 0;

    nv::logger::FaultBuffer fault_buffer{};
    uint8_t                 index_offset = 0;

    if (fault == nv::logger::Fault::Hard) {
        memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
        memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
        memcpy(
            fault_buffer.data() + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
        memcpy(
            fault_buffer.data() + sizeof(uint32_t) * 3, (uint32_t*)(HFSR_REGISTER_ADDRESS), 4);
        index_offset = 4;
    }
    else if (fault == nv::logger::Fault::Memory) {
        memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
        memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
        index_offset = 2;
    }
    else if (fault == nv::logger::Fault::Bus) {
        memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
        memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
        index_offset = 2;
    }
    else if (fault == nv::logger::Fault::Usage) {
        memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
        memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
        memcpy(
            fault_buffer.data() + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
        index_offset = 3;
    }

    constexpr uint32_t CopySize = sizeof(uint32_t) * 8;
    if (is_msp) {
        uint32_t offset = 2;
        memcpy(fault_buffer.data() + sizeof(uint32_t) * index_offset, MSP + offset, CopySize);
    }
    else {
        memcpy(fault_buffer.data() + sizeof(uint32_t) * index_offset, PSP, CopySize);
    }

    nv::logger::FaultLogger::fault(fault, fault_buffer, nv::logger::FaultDataSize);

    uint32_t hfsr = *(uint32_t*)HFSR_REGISTER_ADDRESS;
    uint32_t cfsr = *(uint32_t*)MMFSR_REGISTER_ADDRESS;
    nv::bootloader::Driver::write_application_fault_record(
        sys::bootloader::Driver::ApplicationFaultMagic, cfsr, hfsr, 0x0);
}

void HardFault_Handler(void)
{
    uint32_t lr_value = 0;
    lr_value          = (uint32_t)__builtin_return_address(0);
    uint32_t msp      = (uint32_t)__get_MSP();
    Log_Fault(nv::logger::Fault::Hard, lr_value, msp);

    if constexpr (nv::ipc::EnableRuntimeWdt) {
        sys::watchdog::WwdtDriver::trigger_wdt_reset_if_enabled(sys::watchdog::wwdt1);
    }
    // coverity[no_escape] suppress warning for Infinite loop with no exit
    while (1) {};
#if 0
    nv::error("HardFault_Handler\r\n");

    uint32_t* fault_stack = (uint32_t*)__get_PSP();
    nv::error("fault_stack[0x%x]\r\n", *(uint32_t*)fault_stack);
    for (int i = 0; i < 8; i++) {
        nv::error("[0x%x][0x%x]\r\n", fault_stack + i, *(fault_stack + i));
    }

    nv::logger::FaultBuffer fault_buffer{};
    constexpr uint32_t      CopySize = sizeof(fault_stack[0]) * 8;
    memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t) * 3, (uint32_t*)(HFSR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t) * 4, fault_stack, CopySize);
    nv::logger::FaultLogger::fault(
        nv::logger::Fault::Hard, fault_buffer, nv::logger::FaultDataSize);
    // coverity[no_escape] suppress warning for while(1) loop
    while (true) {}
#endif
}

void MemManage_Handler(void)
{
    uint32_t lr_value = 0;
    lr_value          = (uint32_t)__builtin_return_address(0);
    uint32_t msp      = (uint32_t)__get_MSP();
    Log_Fault(nv::logger::Fault::Memory, lr_value, msp);

    if constexpr (nv::ipc::EnableRuntimeWdt) {
        sys::watchdog::WwdtDriver::trigger_wdt_reset_if_enabled(sys::watchdog::wwdt1);
    }
    // coverity[no_escape] suppress warning for Infinite loop with no exit
    while (1) {};

#if 0
    nv::error("MemManage_Handler\r\n");

    nv::error("0xE000ED28[0x%x]\r\n", *(uint32_t*)0xE000ED28);
    nv::error("0xE000ED34[0x%x]\r\n", *(uint32_t*)0xE000ED34);

    uint32_t* fault_stack = (uint32_t*)__get_PSP();
    nv::error("fault_stack[0x%x]\r\n", fault_stack);

    /* Saved by hardware */
    /*
    enum register_stack_t {
       REG_R0,
       REG_R1,
       REG_R2,
       REG_R3,
       REG_R12,
       REG_LR,
       REG_PC,
       REG_xPSR
    };
    */

    for (int i = 0; i < 8; i++) {
        nv::error("[0x%x][0x%x]\r\n", fault_stack + i, *(fault_stack + i));
    }
    nv::logger::FaultBuffer fault_buffer{};
    constexpr uint32_t      CopySize = sizeof(fault_stack[0]) * 8;
    memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t) * 2, fault_stack, CopySize);
    nv::logger::FaultLogger::fault(
        nv::logger::Fault::Memory, fault_buffer, nv::logger::FaultDataSize);

    // coverity[no_escape] suppress warning for while(1) loop
    while (true) {}
#endif
}

void BusFault_Handler(void)
{
    uint32_t lr_value = 0;
    lr_value          = (uint32_t)__builtin_return_address(0);
    uint32_t msp      = (uint32_t)__get_MSP();
    Log_Fault(nv::logger::Fault::Bus, lr_value, msp);

    if constexpr (nv::ipc::EnableRuntimeWdt) {
        sys::watchdog::WwdtDriver::trigger_wdt_reset_if_enabled(sys::watchdog::wwdt1);
    }
    // coverity[no_escape] suppress warning for Infinite loop with no exit
    while (1) {};

#if 0
    nv::error("BusFault_Handler\r\n");
    uint32_t*               fault_stack = (uint32_t*)__get_PSP();
    nv::logger::FaultBuffer fault_buffer{};
    constexpr uint32_t      CopySize = sizeof(fault_stack[0]) * 8;
    memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t) * 2, fault_stack, CopySize);
    nv::logger::FaultLogger::fault(
        nv::logger::Fault::Bus, fault_buffer, nv::logger::FaultDataSize);

    // coverity[no_escape] suppress warning for while(1) loop
    while (1) {};
#endif
}

void UsageFault_Handler(void)
{
    uint32_t lr_value = 0;
    lr_value          = (uint32_t)__builtin_return_address(0);
    uint32_t msp      = (uint32_t)__get_MSP();
    Log_Fault(nv::logger::Fault::Usage, lr_value, msp);

    if constexpr (nv::ipc::EnableRuntimeWdt) {
        sys::watchdog::WwdtDriver::trigger_wdt_reset_if_enabled(sys::watchdog::wwdt1);
    }
    // coverity[no_escape] suppress warning for Infinite loop with no exit
    while (1) {};

#if 0
    nv::error("UsageFault_Handler\r\n");
    uint32_t* fault_stack = (uint32_t*)__get_PSP();

    nv::logger::FaultBuffer fault_buffer{};
    constexpr uint32_t      CopySize = sizeof(fault_stack[0]) * 8;
    memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
    memcpy(fault_buffer.data() + sizeof(uint32_t) * 3, fault_stack, CopySize);
    nv::logger::FaultLogger::fault(
        nv::logger::Fault::Usage, fault_buffer, nv::logger::FaultDataSize);

    // coverity[no_escape] suppress warning for while(1) loop
    while (true) {}
#endif
}

void SecureFault_Handler(void)
{
    nv::fatal("SecureFault_Handler\r\n");
    nv::logger::FaultLogger::fault(nv::logger::Fault::Secure);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    // nv::fatal("Stack overflow in task %p:%s\n", xTask, pcTaskName);
    nv::logger::FaultBuffer buffer{};
    constexpr auto          MaxTaskLen = nv::logger::FaultDataSize;
    memcpy(buffer.data(), pcTaskName, MaxTaskLen);
    nv::logger::FaultLogger::fault(nv::logger::Fault::Sovf, buffer, nv::logger::FaultDataSize);

    uint32_t hfsr = *(uint32_t*)HFSR_REGISTER_ADDRESS;
    uint32_t cfsr = *(uint32_t*)MMFSR_REGISTER_ADDRESS;
    nv::bootloader::Driver::write_application_fault_record(
        sys::bootloader::Driver::ApplicationFaultMagic, cfsr, hfsr, 0x0);

    if constexpr (nv::ipc::EnableRuntimeWdt) {
        sys::watchdog::WwdtDriver::trigger_wdt_reset_if_enabled(sys::watchdog::wwdt1);
    }
    /* Loop forever */
    // coverity[no_escape] suppress warning for while(1) loop
    for (;;);
}
}

// NOLINTEND
