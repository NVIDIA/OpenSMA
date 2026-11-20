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
#include "error-inject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "fsl_common.h"
#include "task.h"

namespace sys::common {

[[noreturn]] void ErrorInject::trigger_MemManageFault()
{
    /**
     * deference null pointer to trigger memory management fault
     */
    // coverity[zero_deref] - intentional NULL dereference for fault injection
    // coverity[cert_exp34_c_violation] - intentional NULL dereference for fault injection
    *(static_cast<volatile uint32_t*>(
        nullptr)) = 1;  // NOLINT(clang-analyzer-core.NullDereference)

    // avoid clang-tidy warning
    __builtin_unreachable();
}

[[noreturn]] void ErrorInject::trigger_WatchdogReset()
{
    /**
     * runtime watchdog must be enabled to be able to trigger watchdog reset
     * -> src/projects/<project>/config.h
     *    -> constexpr bool EnableRuntimeWdt = true;
     */

#if 0
    // TODO: The code below does not work for all MCUs and requires fixing, https://nvbugspro.nvidia.com/bug/6462487
    __disable_irq();  // disable all interrupts
    // coverity[no_escape]
    while (true) {}  // hang here
#else
    configASSERT(0);  // the assert is just a work around
#endif

    // avoid clang-tidy warning
    __builtin_unreachable();
}

void ErrorInject::trigger_StackCheckFail()
{
    /**
     * Smash this frame's stack canary so the compiler-inserted epilogue check fails on
     * return and calls __stack_chk_fail (src/sys/.../common/stack_protect.cpp), which
     * defers the fault logging to Reserved85_IRQHandler running on the MSP.
     *
     * Requires -fstack-protector-strong (SSP_ENABLED): the local array below forces a
     * canary onto this frame. This method is intentionally NOT [[noreturn]] and does not
     * call __builtin_unreachable() -- both would let the compiler drop the epilogue where
     * the canary check lives, so the check would never fire.
     */
    constexpr size_t        kBufSize         = 16U;
    constexpr size_t        kOverwriteMargin = 16U;
    constexpr unsigned char kFillByte        = 0xAAU;

    std::array<volatile char, kBufSize> buf{};

    // Launder buf's address through a volatile integer so the compiler cannot apply
    // array-bounds / stringop-overflow analysis to the deliberate out-of-bounds write
    // below (which would otherwise warn or be optimized away as UB).
    // NOLINTNEXTLINE(*-reinterpret-cast,*-no-int-to-ptr)
    volatile auto base = reinterpret_cast<uintptr_t>(&buf[0]);

    // Overwrite past the end of buf: the canary sits just above the array, so this
    // corrupts it (and possibly the saved registers -- harmless, the canary check runs
    // first). The mismatch is detected when this function returns.
    // coverity[overrun-local] - intentional out-of-bounds write for fault injection
    // coverity[cert_arr30_c_violation] - intentional out-of-bounds write for fault injection
    // NOLINTNEXTLINE(*-reinterpret-cast,*-no-int-to-ptr)
    memset(reinterpret_cast<void*>(base), kFillByte, kBufSize + kOverwriteMargin);
}

}  // namespace sys::common
