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
#pragma once

#include <cstdint>
#include <span>

#include "nv/common/debuglevel.h"
#include "nv/common/preproc.h"
#include "nv/common/utils.h"

namespace nv::common {

/// All console output should go through this.
class Console
{
public:
    /// Main interface that can be disabled using 'enabled' flag.
    static void print(DebugLevel lvl, const char* fmt, ...);

    /// Permit programatically disabling all console output.
    /// This is useful when unittesting and wish to hide expected errors.
    static void enable(bool en = true) { _enabled = en; }
    static bool enabled() { return _enabled; }
    static char last() { return _last; }

    /// Scoped disablement of Console output.
    struct ScopeDisable
    {
        bool enable;
        ScopeDisable() noexcept : enable(_enabled) { _enabled = false; }
        ~ScopeDisable() noexcept { _enabled = enable; }
        NV_COMMON_COPY_MOVE_CE(ScopeDisable, delete);
    };
    struct ScopeEnable
    {
        bool enable;
        ScopeEnable() noexcept : enable(_enabled) { _enabled = true; }
        ~ScopeEnable() noexcept { _enabled = enable; }
        NV_COMMON_COPY_MOVE_CE(ScopeEnable, delete);
    };

#ifdef NV_UNITTEST
    static void print(const char* fmt, ...);
    static void print(int ch);
#endif

protected:
    static inline NV_SHARED_DATA bool _enabled = true;  // NOLINT(*-non-const-global-variables)
    static inline NV_SHARED_DATA char _last    = '\n';  // NOLINT(*-non-const-global-variables)
};

}  // namespace nv::common
