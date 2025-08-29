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
#include "nv/common/system.h"

#include <cstdio>
#include <cstdlib>
#include <FreeRTOS.h>
#include <portmacro.h>
#include <task.h>

#include "nv/common/utils.h"

using namespace nv::common;

System& System::inst()
{
    static auto&& system = System();  // NOLINT(*-global-variables)
    setlinebuf(stdout);
    setlinebuf(stderr);
    return system;
}

void System::scheduler_start()
{
    vTaskStartScheduler();
}

void System::scheduler_stop(ExitValue exit_value)
{
    vTaskEndScheduler();
    _exit_value = exit_value;
}

void System::critical_enter()
{
    vPortEnterCritical();
}

void System::critical_exit()
{
    vPortExitCritical();
}

// GBS:BEGIN NO COVERAGE FIXME!!
void System::abort(ExitValue ev)
{
    _exit_value = ev;
    std::exit(to_underlying(_exit_value));
}
// GBS:END NO COVERAGE FIXME!!

uint32_t System::tick_time_ms()
{
    return msPerSec / configTICK_RATE_HZ;
}

uint32_t System::task_switch_latency()
{
    return msPerSec / configTICK_RATE_HZ;
}