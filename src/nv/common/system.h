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
#include "nv/common/utils.h"
#include "sys/common/system.h"

namespace nv::common {

enum class ExitValue
{
    Ok        = 0,  ///< Normal exit.
    Error     = 1,  ///< Exiting due to an unexpected error.
    MemFault  = 2,  ///< Exiting due to invalid memory access.
    HardFault = 3,

};

/// avoid clang-tidy warning
constexpr uint32_t msPerSec = 1000;

/**
 * Interface for system specific operations.
 *
 * The actual implementations are found in sys/{system}/sys/common/system.{h,cpp}
 */
class System
: public sys::common::System
, NonCopyable
{
public:
    System()  = default;
    ~System() = default;
    NV_COMMON_COPY_MOVE(System, delete);

    /// Singleton interface
    static System& inst();

    /// Start the system's scheduler.
    void scheduler_start();

    /// Stop the system's scheduler.
    void scheduler_stop(ExitValue exit_value);

    /// Enter a critical section, temporarily suspending interrupts and any switching to other
    /// tasks.
    void critical_enter();

    /// Leave a critiical section.
    void critical_exit();

    /// Get system tick time in ms
    uint32_t tick_time_ms();

    /// Get system task switch latency in ms
    uint32_t task_switch_latency();

    /// Disable the cache.
    void disable_cache();

    /// Enable the cache.
    void enable_cache();

    /// Abort the program, with an exit value if running on an emulator or unittesting.
    [[noreturn]] void abort(ExitValue i);

    ExitValue exit_value() const { return _exit_value; }

private:
    ExitValue _exit_value;
};

/// Enters and exits a critical section depending on the scope of the instantiation.
struct ScopedCritical
{
    NV_COMMON_COPY_MOVE_CE(ScopedCritical, delete);
    ScopedCritical() { System::inst().critical_enter(); }
    ~ScopedCritical() { System::inst().critical_exit(); }
};

}  // namespace nv::common
