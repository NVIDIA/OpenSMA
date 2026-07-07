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

#include "nv/gpu_pwr_controller/task.h"

namespace nv::gpu_pwr_controller {

Task::Task() noexcept
: nv::ipc::Task(nv::ipc::TaskId::GpuPwrController, "GpuPwrController")
, _manager()
{
    nv::info("Finished GPU Pwr Controller Task initialization\n");
}

void Task::make()
{
    NV_TASK_DATA static Task task;
    constexpr auto           StackSize = std::max(2048, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::GpuPwrController, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

void Task::start()
{
    nv::info("Starting GPU Pwr Controller Task\n");
    _manager.main();
}

}  // namespace nv::gpu_pwr_controller