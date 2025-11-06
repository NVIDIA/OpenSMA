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

#include <mpu_wrappers.h>

#include "nv/ipc/task.h"
#include "sys/common/utils.h"
#include NV_IPC_CONFIG_H

using namespace nv;

const ipc::Task& ipc::Supervisor::current_task() const
{
    // WARNING: Do not call print from this method

    if (xPortIsInsideInterrupt() == false) {
        auto handle = xTaskGetCurrentTaskHandle();
        for (auto task : _tasks_pub) {
            if (task.handle() == handle) {
                return *task.task;
            }
        }
    }

    class NullTask : public Task
    {
    public:
        NullTask() noexcept : ipc::Task(TaskId::End, "ISR") {}
    };

    static const NullTask RootTask;

    return RootTask;  // indidate in ISR
}

ipc::TaskId ipc::Supervisor::current_task_id() const
{
    ipc::TaskId id{};

    auto task_id = uxTaskGetTaskNumber(nullptr);

    if (task_id != 0) {
        task_id--;
        id = static_cast<ipc::TaskId>(task_id);
    }
    else {
        if (xPortIsInsideInterrupt()) {
            /* privileged */

            const char* name = pcTaskGetName(nullptr);
            if (name[0] == 'I') {
                // Idle Task
                id = ipc::TaskId::Idle;
            }
            else {
                // Timer Task
                id = ipc::TaskId::Timer;
            }
        }
        else {
            // always timer here
            id = ipc::TaskId::Timer;
        }
    }

    return id;
}

[[noreturn]] void ipc::Supervisor::on_terminate()
{
    nv::fatal("terminating\n");
}

[[noreturn]] void ipc::Supervisor::on_exit(int status)
{
    nv::debug("exiting with %d...\n", status);
    // coverity[no_escape] - Intentional infinite loop for system halt
    while (true) {}
}

// hooks for nostdlib nolibc
// NOLINTBEGIN(cert-dcl58-cpp)
namespace std {
// coverity[cert_dcl58_cpp_violation] - std::terminate is not defined in nostdlib
void terminate() noexcept
{
    // coverity[no_escape] - Intentional infinite loop for system termination
    while (true) {}
}
}  // namespace std
// NOLINTEND(cert-dcl58-cpp)

extern "C" void _exit(int status)  // NOLINT
{
    ipc::Supervisor::inst().on_exit(status);
}

uint32_t sys::ipc::get_os_ticks()
{
    if (xPortIsInsideInterrupt()) {
        return static_cast<uint32_t>(xTaskGetTickCountFromISR());
    }
    return static_cast<uint32_t>(xTaskGetTickCount());
}

bool sys::ipc::is_scheduler_run()
{
    return !((xPortIsInsideInterrupt())
             || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
}

bool sys::ipc::is_in_isr()
{
    return xPortIsInsideInterrupt();
}