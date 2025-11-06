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
#include "nv/common/console.h"

#include <array>
#include <cstdarg>

#include "fsl_debug_console.h"

#include "nv/common/debuglevel.h"
// #include "nv/common/system.h" //critical temporary removed
#include <cstdint>
#include <cstring>

#include "nv/common/preproc.h"
#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/uart_string.h"

using namespace nv::common;

// We are using varargs here to avoid code bloating from templating.
// coverity[cert_dcl50_cpp_violation] - intentional use of varargs
void Console::print(DebugLevel lvl, const char* fmt, ...)  // NOLINT(cert-dcl50-cpp)
{
    if (!_enabled) {
        return;
    }  // disabled
    (void)lvl;
#if 0
    // Select terminal color and output pipe by debuglevel
    NV_SHARED_DATA static std::array<const char*, 6> cfg{"",  // None
                                                         "\x1b[0;31mFATAL \x1B[0m",
                                                         "\x1b[0;31mERROR \x1B[0m",
                                                         "\x1b[0;33mWARN  \x1B[0m",
                                                         "\x1b[0;32mDBG   \x1B[0m",
                                                         "\x1b[1;37mINFO  \x1B[0m"};
    auto&                                            v = cfg.at(common::to_underlying(lvl));
    auto& task = nv::ipc::Supervisor::inst().current_task().name();

    // critical is privilege function, cant call directly
    // const ScopedCritical critical;  // TODO:

    // NOLINTBEGIN
    DbgConsole_Printf("%s[%s] ", v, task.data());
    va_list va;
    va_start(va, fmt);
    DbgConsole_Vprintf(fmt, va);
    va_end(va);
    // NOLINTEND
#endif

    // NOLINTBEGIN
    va_list va;
    va_start(va, fmt);
    nv::logger::format_to_logger(fmt, va);
    va_end(va);
    // NOLINTEND
}
