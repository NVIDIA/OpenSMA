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
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <portmacrocommon.h>
#include <task.h>

#include "board/clock_config.h"
#include "board/peripherals.h"
#include "board/pin_mux.h"

#include "nv/common/debug.h"
#include "nv/common/preproc.h"
#include "nv/common/system.h"
#include "nv/flash/task.h"
#include "nv/ipc/supervisor.h"
#include "nv/ipc/task.h"
#include "nv/nv.h"
#include "sys/ipc/task.h"
#include NV_IPC_CONFIG_H

// extern "C" int ada_print_hello();
// extern "C" void adainit();

namespace nv::testrunner {

class Task : public ipc::Task
{
public:
    Task() noexcept : ipc::Task(nv::ipc::TaskId::TestRunner, "TEST") {}

    static void make()
    {
        constexpr auto StackSize = std::max(1024, int(configMINIMAL_STACK_SIZE));

        NV_SHARED_BSS static Task                      task;
        NV_STACK static sys::ipc::TaskStack<StackSize> stack;

        const std::span<uint8_t> priv;
        task.setup(stack.span(), priv, Priority::Norm, Task::entrypoint);
    }

    static void entrypoint(void* params)
    {
        NV_ASSERT(params != nullptr);

        using namespace std::chrono_literals;
        auto& task = *static_cast<nv::ipc::Task*>(params);
        nv::info("MCU Firmware Test Task\n");
        //        ada_print_hello(); // prints hello by calling Nv.Common.Console back to
        //        Dbg_Putchar
        task.delay(250ms);
        task.suspend();
    }
};
}  // namespace nv::testrunner

int main()
{
    // adainit();

    // setup board
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    nv::info("\033c");

    // create a TestRunner task and startup supervisor
    using namespace nv;

    testrunner::Task::make();
    flash::Task::make();
    ipc::Supervisor::inst().startup(0, nullptr);

    common::System::inst().scheduler_start();
    while (true) {}
}

// Required hooks for FreeRTOS with static allocation enabled
extern "C" void vApplicationGetTimerTaskMemory(StaticTask_t** taskTcbBuffer,
                                               StackType_t**  taskStackBuffer,
                                               uint32_t*      taskStackSize)
{
    static StaticTask_t                                          tcb;
    static std::array<StackType_t, configTIMER_TASK_STACK_DEPTH> stack;
    *taskTcbBuffer   = &tcb;
    *taskStackBuffer = stack.data();
    *taskStackSize   = stack.size();
}

extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** taskTcbBuffer,
                                              StackType_t**  taskStackBuffer,
                                              uint32_t*      taskStackSize)
{
    static StaticTask_t                                      tcb;
    static std::array<StackType_t, configMINIMAL_STACK_SIZE> stack;
    *taskTcbBuffer   = &tcb;
    *taskStackBuffer = stack.data();
    *taskStackSize   = stack.size();
}
