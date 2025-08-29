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
#include "nv/ipc/task.h"

#include <limits>
#include <string_view>

#include "nv/ipc/supervisor.h"
#include "nv/logger/log.h"

#include NV_IPC_CONFIG_H

using namespace nv::ipc;

Task::Task(TaskId id, const std::string_view& name) noexcept : _id(id), _name(name)
{
    // Automatically register the task with the supervisor.
    // coverity[cert_int31_c_violation] - valid cast
    logger::info(logger::Event::IpcTaskRegisterTask, {static_cast<uint8_t>(_id)});
    Supervisor::inst().register_task(*this);
}

/**
 * @brief Get the task id and priority object
 *
 * @param taskIdAndPriority
 * @return void
 *
 * @note priority is 0xFF if the task is not running
 */
void Task::get_task_id_and_priority(TaskIdAndPriority& taskIdAndPriority)
{
    for (auto id = static_cast<uint8_t>(nv::ipc::TaskId::Begin);
         id < static_cast<uint8_t>(nv::ipc::TaskId::KernelEnd);
         id = id + 1) {
        auto& task = nv::ipc::Supervisor::inst().task_pub(static_cast<nv::ipc::TaskId>(id));
        // coverity[cert_int31_c_violation] safe to cast
        auto priority            = (task.handle() != nullptr)
                                     ? static_cast<Task::Priority>(uxTaskPriorityGet(task.handle()))
                                     : Task::Priority::Invalid;
        taskIdAndPriority.at(id) = std::make_pair(static_cast<nv::ipc::TaskId>(id), priority);
    }
}