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
#include <cstdint>
#include "fsl_common.h"

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
    __disable_irq();  // disable all interrupts
    // coverity[no_escape]
    while (true) {}  // hang here

    // avoid clang-tidy warning
    __builtin_unreachable();
}

}  // namespace sys::common