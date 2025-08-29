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
#include <cstdio>
#include <cstring>
#include <utility>

#include "nv/common/debuglevel.h"
#include "nv/common/system.h"
#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"

using namespace nv::common;

// We are using varargs here to avoid code bloating from templating.
void Console::print(DebugLevel lvl, const char* fmt, ...)  // NOLINT(cert-dcl50-cpp)
{
    if (!_enabled) {
        return;
    }  // disabled

    // Select terminal color and output pipe by debuglevel
    static std::array<std::pair<FILE*, const char*>, 6> cfg{
        {{stdout, ""},
         {stderr, "\x1b[0;31mFATAL \x1B[0m"},
         {stderr, "\x1b[0;31mERROR \x1B[0m"},
         {stderr, "\x1b[0;33mWARN  \x1B[0m"},
         {stdout, "\x1b[0;32mDBG   \x1B[0m"},
         {stdout, "\x1b[1;37mINFO  \x1B[0m"}}
    };
    auto& v    = cfg.at(common::to_underlying(lvl));
    auto& task = nv::ipc::Supervisor::inst().current_task().name();

    const ScopedCritical Critical;
    // NOLINTBEGIN
    if (_last != '\n') fputc('\n', v.first);
    fprintf(v.first, "%s[%s]: ", v.second, task.data());
    va_list va;
    va_start(va, fmt);
    vfprintf(cfg[static_cast<int>(lvl)].first, fmt, va);
    va_end(va);
    // NOLINTEND
    if (fmt[strlen(fmt) - 1] != '\n') {
        IgnoreReturn = fputc('\n', v.first);
    }
    _last = '\n';  // coverity[cert_con43_c_violation] not thread safe but ok just debug code
}
