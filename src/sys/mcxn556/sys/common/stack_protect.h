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
#if defined(__SSP__) || defined(__SSP_STRONG__) || defined(__SSP_ALL__)                        \
    || defined(__SSP_EXPLICIT__)
constexpr bool SSP_ENABLED = true;
#else
constexpr bool SSP_ENABLED = false;
#endif

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-dcl37-c,cert-dcl51-cpp)
extern uintptr_t __stack_chk_guard;

#include "fsl_device_registers.h"

// Software-triggered NVIC line used to run stack-check-fail logging in handler mode (on the
// MSP), off the already-corrupted task stack. Reserved85 (IRQn 69) is an unused/reserved vector
// on every supported part (MCXN236/MCXN547/MCXN556, both cores) and is not assigned by any
// board's TrustZone resource_config. If a future SDK ever assigns it, switch to Reserved86_IRQn
// (70), also reserved. The vector is filled by Reserved85_IRQHandler in fault.cpp.
constexpr IRQn_Type StackChkLogIRQn = Reserved85_IRQn;

// Return address captured from the failing frame in __stack_chk_fail; read by the handler.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-dcl37-c,cert-dcl51-cpp)
extern "C" volatile uint32_t g_stack_chk_return_addr;

// Privileged implementation that arms and fires StackChkLogIRQn. Only safe to call with
// privileges already raised (i.e. from handler mode, or via StackProtect_TriggerLog below).
// Defined in stack_protect.cpp, entered through the MPU wrappers' system call table.
extern "C" void StackProtect_TriggerLog_Priv(void);

// Entry point for __stack_chk_fail: branches directly to StackProtect_TriggerLog_Priv if
// already privileged, otherwise issues the NV_SYSTEM_CALL_StackProtect_TriggerLog SVC to raise
// privilege first. NVIC registers are privileged-only, so an unprivileged task calling the
// NVIC_* functions directly takes a BusFault.
extern "C" void StackProtect_TriggerLog(void);
