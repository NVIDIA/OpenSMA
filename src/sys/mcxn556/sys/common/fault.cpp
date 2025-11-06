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
#include "sys/common/c2c_fault.h"
#include "sys/ipc/mcmgr_wrapper.h"
#include "mcmgr.h"
#include "nv/ipc/driver.h"
#include NV_IPC_CONFIG_H

#define HFSR_REGISTER_ADDRESS  (0xE000ED2CU)
#define MMFSR_REGISTER_ADDRESS (0xE000ED28U)
#define MMFAR_REGISTER_ADDRESS (0xE000ED34U)
#define BFAR_REGISTER_ADDRESS  (0xE000ED38U)

namespace {
void copy_dump_info_with_buffer(uint8_t*          buffer,
                                nv::logger::Fault fault,
                                uint32_t*         stack_pointer)
{
    uint8_t offset = 0;
    switch (fault) {
        case nv::logger::Fault::Hard:
            memcpy(buffer, (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t) * 3, (uint32_t*)(HFSR_REGISTER_ADDRESS), 4);
            offset = sizeof(uint32_t) * 4;
            break;
        case nv::logger::Fault::Memory:
            memcpy(buffer, (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
            offset = sizeof(uint32_t) * 2;
            break;
        case nv::logger::Fault::Bus:
            memcpy(buffer, (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t), (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
            offset = sizeof(uint32_t) * 2;
            break;
        case nv::logger::Fault::Usage:
            memcpy(buffer, (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
            offset = sizeof(uint32_t) * 3;
            break;
        case nv::logger::Fault::Secure:
            memcpy(buffer, (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS), 4);
            memcpy(buffer + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS), 4);
            offset = sizeof(uint32_t) * 3;
            break;
        default: break;
    }

    memcpy(buffer + offset, stack_pointer, sizeof(uint32_t) * 8);
}
}  // namespace

extern "C" {

// NOLINTBEGIN
// Security violation registers accessible via AHBSC->SEC_VIO_* structure members

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

// Return true if security violation information is from core1
bool HandleSecurityViolation(void)
{
    uint32_t sec_vio_info_valid = AHBSC->SEC_VIO_INFO_VALID;
    nv::error("SEC_VIO_INFO_VALID: 0x%x\r\n", sec_vio_info_valid);

    bool from_core1 = false;

    auto cur_core = nv::ipc::get_current_core();

    if (sec_vio_info_valid != 0) {
        nv::error("=== SECURITY VIOLATION INFORMATION ===\r\n");

        // Check bits 0-18 for individual AHB port violations
        for (int i = 0; i < 19; i++) {
            if (sec_vio_info_valid & (1U << i)) {
                nv::error("AHB Port %d violation detected:\r\n", i);

                // Log SEC_VIO_ADDR for this specific port using AHBSC structure
                // coverity[cert_ctr50_cpp_violation]
                uint32_t sec_vio_addr = AHBSC->SEC_VIO_ADDR[i];
                nv::error("  SEC_VIO_ADDR[%d]: 0x%x\r\n", i, sec_vio_addr);

                // Log SEC_VIO_MISC_INFO for this specific port using AHBSC structure
                // coverity[cert_ctr50_cpp_violation]
                uint32_t sec_vio_misc_info = AHBSC->SEC_VIO_MISC_INFO[i];
                nv::error("  SEC_VIO_MISC_INFO[%d]: 0x%x\r\n", i, sec_vio_misc_info);

                if (c2c_fault::is_sec_vio_from_core1(sec_vio_misc_info)) {
                    from_core1 = true;
                }

                if (cur_core == nv::ipc::CoreId::Core0) {
                    nv::logger::FaultBuffer fault_buffer{};
                    memcpy(fault_buffer.data(), &i, 4);
                    memcpy(fault_buffer.data() + sizeof(uint32_t), &sec_vio_addr, 4);
                    memcpy(fault_buffer.data() + sizeof(uint32_t) * 2, &sec_vio_misc_info, 4);
                    nv::logger::FaultLogger::fault(
                        nv::logger::Fault::SecVio, fault_buffer, nv::logger::FaultDataSize);
                }
            }
        }
    }
    else {
        nv::error("No security violation information\r\n");
    }

    return from_core1;
}

void Log_Fault(nv::logger::Fault fault, uint32_t lr_value, uint32_t msp)
{
    // TODO Check sec fault from core 1 also
    auto cur_core                   = nv::ipc::get_current_core();
    bool is_core1_trigger_sec_fault = false;
    if (fault == nv::logger::Fault::Secure) {
        // Both core in fault state
        if (cur_core == nv::ipc::CoreId::Core0) {
            is_core1_trigger_sec_fault = HandleSecurityViolation();
        }
        // coverity[dead_error_line] - dual core
        else {
            // coverity[dead_error_line] - Expect cannot reach here in core0
            HandleSecurityViolation();
        }
    }

    if (!is_core1_trigger_sec_fault) {
        if (c2c_fault::is_trigger_by_another_core_fault()) {
            c2c_fault::notify_another_core_ready();
        }
        else {
            // coverity[dead_error_line] - dual core
            auto event_type = cur_core == nv::ipc::CoreId::Core0
                                ? nv::ipc::task::EventType::Core0Fault
                                : nv::ipc::task::EventType::Core1Fault;
            (void)sys::ipc::task::Mcmgr::trigger_event_force(
                nv::ipc::get_peer_core(), event_type, 0);
            c2c_fault::wait_another_core_ready();
        }
    }
    else {
        c2c_fault::notify_another_core_ready();
    }

    // coverity[cert_int36_c_violation] casting PSP to uint32_t*
    uint32_t* PSP = (uint32_t*)(__get_PSP());
    // coverity[cert_int36_c_violation] casting MSP to uint32_t*
    uint32_t*          MSP        = (uint32_t*)(msp);
    constexpr uint32_t SPSEL_MASK = 0x1 << 2;
    bool               is_msp     = (lr_value & SPSEL_MASK) == 0;
    nv::error("is_msp: %d\r\n", is_msp);

    // nv::logger::FaultBuffer fault_buffer{};
    // uint8_t index_offset = 0;

    if (fault == nv::logger::Fault::Hard) {
        // memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
        // memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS),
        // 4); memcpy(
        //     fault_buffer.data() + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS),
        //     4);
        // memcpy(
        //     fault_buffer.data() + sizeof(uint32_t) * 3, (uint32_t*)(HFSR_REGISTER_ADDRESS),
        //     4);
        nv::error("Hard fault\r\n");
        nv::error("MMFSR: 0x%x\r\n", *(uint32_t*)MMFSR_REGISTER_ADDRESS);
        nv::error("MMFAR: 0x%x\r\n", *(uint32_t*)MMFAR_REGISTER_ADDRESS);
        nv::error("BFAR: 0x%x\r\n", *(uint32_t*)BFAR_REGISTER_ADDRESS);
        nv::error("HFSR: 0x%x\r\n", *(uint32_t*)HFSR_REGISTER_ADDRESS);
        // index_offset = 4;
    }
    else if (fault == nv::logger::Fault::Memory) {
        // memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
        // memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS),
        // 4);
        nv::error("Memory fault\r\n");
        nv::error("MMFSR: 0x%x\r\n", *(uint32_t*)MMFSR_REGISTER_ADDRESS);
        nv::error("MMFAR: 0x%x\r\n", *(uint32_t*)MMFAR_REGISTER_ADDRESS);
        // index_offset = 2;
    }
    else if (fault == nv::logger::Fault::Bus) {
        // memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
        // memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(BFAR_REGISTER_ADDRESS),
        // 4);
        nv::error("Bus fault\r\n");
        nv::error("MMFSR: 0x%x\r\n", *(uint32_t*)MMFSR_REGISTER_ADDRESS);
        nv::error("BFAR: 0x%x\r\n", *(uint32_t*)BFAR_REGISTER_ADDRESS);
        // index_offset = 2;
    }
    else if (fault == nv::logger::Fault::Usage) {
        // memcpy(fault_buffer.data(), (uint32_t*)(MMFSR_REGISTER_ADDRESS), 4);
        // memcpy(fault_buffer.data() + sizeof(uint32_t), (uint32_t*)(MMFAR_REGISTER_ADDRESS),
        // 4); memcpy(
        //     fault_buffer.data() + sizeof(uint32_t) * 2, (uint32_t*)(BFAR_REGISTER_ADDRESS),
        //     4);
        nv::error("Usage fault\r\n");
        nv::error("MMFSR: 0x%x\r\n", *(uint32_t*)MMFSR_REGISTER_ADDRESS);
        nv::error("MMFAR: 0x%x\r\n", *(uint32_t*)MMFAR_REGISTER_ADDRESS);
        nv::error("BFAR: 0x%x\r\n", *(uint32_t*)BFAR_REGISTER_ADDRESS);
        // index_offset = 3;
    }

    // constexpr uint32_t CopySize = sizeof(uint32_t) * 8;
    uint32_t* stack_ptr = nullptr;
    if (is_msp) {
        uint32_t offset = 2;
        // memcpy(fault_buffer.data() + sizeof(uint32_t) * index_offset, MSP + offset,
        // CopySize);
        stack_ptr = MSP + offset;
    }
    else {
        // memcpy(fault_buffer.data() + sizeof(uint32_t) * index_offset, PSP, CopySize);
        stack_ptr = PSP;
    }

    // nv::logger::FaultLogger::fault(fault, fault_buffer, nv::logger::FaultDataSize);

    uint32_t hfsr  = *(uint32_t*)HFSR_REGISTER_ADDRESS;
    uint32_t cfsr  = *(uint32_t*)MMFSR_REGISTER_ADDRESS;
    uint32_t mmfar = *(uint32_t*)MMFAR_REGISTER_ADDRESS;
    uint32_t bfar  = *(uint32_t*)BFAR_REGISTER_ADDRESS;
    // nv::bootloader::Driver::write_application_fault_record(
    //     sys::bootloader::Driver::ApplicationFaultMagic, cfsr, hfsr, 0x0);
    uint32_t r0_reg   = stack_ptr[0];  // R0
    uint32_t r1_reg   = stack_ptr[1];  // R1
    uint32_t r2_reg   = stack_ptr[2];  // R2
    uint32_t r3_reg   = stack_ptr[3];  // R3
    uint32_t r12_reg  = stack_ptr[4];  // R12
    uint32_t lr_reg   = stack_ptr[5];  // LR
    uint32_t pc_reg   = stack_ptr[6];  // PC
    uint32_t xpsr_reg = stack_ptr[7];  // xPSR

    nv::error(
        "CFSR: 0x%x, MMFAR: 0x%x, BFAR: 0x%x, HFSR: 0x%x, R0: 0x%x, R1: 0x%x, R2: 0x%x, R3: "
        "0x%x, R12: 0x%x, LR: 0x%x, PC: 0x%x, xPSR: 0x%x\n",
        cfsr,
        mmfar,
        bfar,
        hfsr,
        r0_reg,
        r1_reg,
        r2_reg,
        r3_reg,
        r12_reg,
        lr_reg,
        pc_reg,
        xpsr_reg);

    // Core 0: Wait core 1 dump and dump both core
    // Core 1: Dump self to buffer
    if (cur_core == nv::ipc::CoreId::Core0) {
        c2c_fault::wait_core1_dump_ready(
            nv::ipc::task::Driver::get_shared_memory_base_address());

        nv::logger::FaultBuffer fault_buffer{};
        copy_dump_info_with_buffer(fault_buffer.data(), fault, stack_ptr);
        nv::logger::FaultLogger::fault(fault, fault_buffer, nv::logger::FaultDataSize);

        c2c_fault::dump_core1_fault_info();
    }
    // coverity[dead_error_line] - dual core
    else if (cur_core == nv::ipc::CoreId::Core1) {
        nv::logger::FaultBuffer fault_buffer{};
        copy_dump_info_with_buffer(fault_buffer.data(), fault, stack_ptr);
        c2c_fault::core_1_write_fault_info(fault_buffer.data(), fault, true);
    }
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

void SEC_VIO_IRQHandler(void)
{
    uint32_t lr_value = 0;
    lr_value          = (uint32_t)__builtin_return_address(0);
    uint32_t msp      = (uint32_t)__get_MSP();
    Log_Fault(nv::logger::Fault::Secure, lr_value, msp);
    if constexpr (nv::ipc::EnableRuntimeWdt) {
        sys::watchdog::WwdtDriver::trigger_wdt_reset_if_enabled(sys::watchdog::wwdt1);
    }
    // coverity[no_escape] suppress warning for Infinite loop with no exit
    while (1) {};
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
    uint32_t lr_value = 0;
    lr_value          = (uint32_t)__builtin_return_address(0);
    uint32_t msp      = (uint32_t)__get_MSP();
    Log_Fault(nv::logger::Fault::Secure, lr_value, msp);

    if constexpr (nv::ipc::EnableRuntimeWdt) {
        sys::watchdog::WwdtDriver::trigger_wdt_reset_if_enabled(sys::watchdog::wwdt1);
    }
    // coverity[no_escape] suppress warning for Infinite loop with no exit
    while (1) {};
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
