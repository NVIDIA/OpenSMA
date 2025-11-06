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
#include "nv/nv.h"
#include "nv/logger/log_fault.h"
#include "sys/common/stack_protect.h"

extern "C" {
namespace {
// NOLINTNEXTLINE(cert-dcl37-c,cert-dcl51-cpp)
void __attribute__((noreturn)) __stack_chk_fail(void)
{
    if constexpr (SSP_ENABLED) {
        void* return_address = __builtin_return_address(0);
        // NOLINTNEXTLINE(*-reinterpret-cast)
        auto                    addr = reinterpret_cast<uint32_t>(return_address);
        nv::logger::FaultBuffer fault_buffer{};
        memcpy(fault_buffer.data(), &addr, sizeof(addr));
        nv::logger::FaultLogger::fault(
            nv::logger::Fault::StackChkFail, fault_buffer, nv::logger::FaultDataSize);
    }
    // coverity[no_escape] suppress warning for while(1) loop
    while (true) {}
}
}  // namespace
}