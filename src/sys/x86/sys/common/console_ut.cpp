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

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "nv/common/system.h"
#include "nv/common/utils.h"

using namespace nv::common;

// This is just a hook for unittesting.
void Console::print(const char* fmt, ...)  // NOLINT(cert-dcl50-cpp)
{
    if (_enabled) {
        const ScopedCritical critical;  // TODO:
        // NOLINTBEGIN
        va_list va;
        va_start(va, fmt);
        vprintf(fmt, va);
        va_end(va);
        fflush(stdout);
        // NOLINTEND
        _last = fmt[strlen(fmt) - 1];
    }
}

// This is just a hook for unittesting.
void Console::print(int c)
{
    if (_enabled) {
        const ScopedCritical critical;
        IgnoreReturn = fputc(c, stdout);
        _last        = char(c);
    }
}
