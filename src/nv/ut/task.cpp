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
#include "nv/ut/task.h"

#include <chrono>
#include <thread>
#include <FreeRTOS.h>
#include <portmacro.h>
#include <task.h>
#include <timers.h>

#include "testrunner/config.h"

#include "nv/common/system.h"
#include "nv/ipc/supervisor.h"
#include "nv/nv.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace nv::ut;
using namespace std::chrono_literals;

ut::Task::Task() noexcept : ipc::Task(ipc::TaskId::Unittest, "UNIT") {}

void ut::Task::make()
{
    constexpr auto      StackSize = std::max(8192, int(configMINIMAL_STACK_SIZE));
    static nv::ut::Task task;
    static sys::ipc::TaskStack<StackSize> stack;  // TODO: trim

    const std::span<uint8_t> priv;
    task.setup(stack.span(), priv, Priority::Norm, Task::entrypoint);
}

void ut::Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<nv::ipc::Task*>(params);
    nv::info("GBS Unittesting...\n");
    task.delay(1ms);  // for coverage only
    auto ex = ut::main(0, nullptr) > 0 ? common::ExitValue::Error : common::ExitValue::Ok;
    common::System::inst().scheduler_stop(ex);
}

#if 0

TEST(TaskCoverage, Resume)
{
    auto& task = ipc::Supervisor::inst().task(ipc::TaskId::TestRunner);
    ensure::is_eq(task.id(), ipc::TaskId::TestRunner);

    ensure::is_eq(task.status(), ipc::Task::Status::Suspended);
    task.resume();
    // currently deletes itself after 1ms
    ensure::is_eq(task.status(), ipc::Task::Status::Running);
    std::this_thread::sleep_for(100ms);
    ensure::is_eq(task.status(), ipc::Task::Status::Dead);
};

#endif
