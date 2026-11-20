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
#include <FreeRTOSConfig.h>
#include <portmacrocommon.h>

#include "mpu_syscall_numbers.h"

#include "nv/common/preproc.h"
#include "nv/nv.h"
#include "sys/common/stack_protect.h"

extern "C" {

NV_PRIVILEGED_FUNCTION void StackProtect_TriggerLog_Priv(void)
{
    // Priority 0 is above the FreeRTOS BASEPRI cutoff, so the log fires even if the canary
    // check happened inside a critical section. The handler calls no FreeRTOS APIs.
    NVIC_SetPriority(StackChkLogIRQn, 0);
    NVIC_EnableIRQ(StackChkLogIRQn);
    NVIC_SetPendingIRQ(StackChkLogIRQn);
}

// NVIC registers are privileged-only, so an unprivileged caller takes a BusFault touching them
// directly. Mirrors the MPU-wrappers system-call pattern (e.g. Gpio_Init_Interrupt_Priv in
// sys/gpio/driver.cpp): branch straight to the privileged handler if already privileged
// (IPSR != 0, or CONTROL.nPRIV == 0), otherwise trap into the SVC handler to raise privilege.
NV_SYS_CALL void StackProtect_TriggerLog(void)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern StackProtect_TriggerLog_Priv                  \n"
        "                                                       \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_StackProtect_TriggerLog                \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_StackProtect_TriggerLog               \n"
        " Privileged_StackProtect_TriggerLog:                   \n"
        "     b StackProtect_TriggerLog_Priv                    \n"
        " Unprivileged_StackProtect_TriggerLog:                 \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_StackProtect_TriggerLog)
        : "memory");
#else
    StackProtect_TriggerLog_Priv();
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

namespace {
// NOLINTNEXTLINE(cert-dcl37-c,cert-dcl51-cpp)
void __attribute__((noreturn)) __stack_chk_fail(void)
{
    if constexpr (SSP_ENABLED) {
        // The task stack is already smashed/near-overflow here, so do the *minimum* on it:
        // capture the faulting return address, then hand the actual logging off to
        // Reserved85_IRQHandler (see fault.cpp), which runs on a fresh stack in handler mode
        // (MSP). Running heavy logging on this corrupted stack risks an MPU fault that would
        // pre-empt the log entirely. Enabling the line here (rather than at init) targets the
        // faulting core's own NVIC, so it works on both cores of dual-core parts regardless of
        // which init path ran.
        // NOLINTNEXTLINE(*-reinterpret-cast)
        g_stack_chk_return_addr = reinterpret_cast<uint32_t>(__builtin_return_address(0));
        __DSB();
        StackProtect_TriggerLog();
        __DSB();
        __ISB();
    }
    // coverity[no_escape] suppress warning for while(1) loop
    while (true) {}
}
}  // namespace
}