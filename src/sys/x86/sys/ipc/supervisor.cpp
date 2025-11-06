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
#include "nv/ipc/supervisor.h"

#include <FreeRTOS.h>
#include <task.h>
#include <unistd.h>

#include "nv/ipc/task.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

const Task& Supervisor::current_task() const
{
    // Do not call print from this method
    auto handle = xTaskGetCurrentTaskHandle();
    for (auto task : _tasks) {
        if (task && task->handle() == handle) {
            return *task;
        }
    }

    class NullTask : public Task
    {
    public:
        NullTask() noexcept : ipc::Task(TaskId::End, "ROOT") {}
    };

    static const NullTask RootTask;

    return RootTask;  // Not really root.
}

ipc::TaskId ipc::Supervisor::current_task_id() const
{
    ipc::TaskId id{};
    auto        cur_handle = xTaskGetCurrentTaskHandle();

    if (cur_handle == xTaskGetIdleTaskHandle()) {
        id = ipc::TaskId::Idle;
    }
    else if (cur_handle == xTimerGetTimerDaemonTaskHandle()) {
        id = ipc::TaskId::Timer;
    }
    else {
        id = static_cast<ipc::TaskId>(uxTaskGetTaskNumber(cur_handle));
    }

    return id;
}

[[noreturn]] void nv::ipc::Supervisor::on_terminate()
{
    nv::fatal("terminating\n");
}

[[noreturn]] void nv::ipc::Supervisor::on_exit(int status)
{
    nv::debug("exiting with %d...\n", status);
    _exit(status);
}

}  // namespace nv::ipc

uint32_t sys::ipc::get_os_ticks()
{
    if (is_in_isr()) {
        return static_cast<uint32_t>(xTaskGetTickCountFromISR());
    }
    return static_cast<uint32_t>(xTaskGetTickCount());
}

bool sys::ipc::is_scheduler_run()
{
    return (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
}

bool sys::ipc::is_in_isr()
{
    return false;
}
