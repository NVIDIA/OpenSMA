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
#include "nv/mctp/task.h"

#include <chrono>

#include "nv/bootloader.h"
#include "nv/common/debug.h"
#include "nv/common/preproc.h"
#include "nv/flash/driver.h"
#include "nv/ipc/timer.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"

namespace nv::mctp {
void Task::make()
{
    using namespace std::chrono_literals;
    // leave 1186 bytes
    constexpr auto StackSize = std::max(2080 + 2080, int(configMINIMAL_STACK_SIZE));

    NV_TASK_DATA static Task                       task;
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));

    task.setup(stack.span(), Priv, Priority::Mctp, Task::entrypoint);

    // uuid only get before kernel start
    flash::Driver::get_uuid(task.uuid);

    ipc::Timer::make(
        ipc::TimerId::MctpEnumerate, 1s, mctp::Driver::on_enumeration_done_timer, false);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);

    using namespace std::chrono_literals;
    auto& task = *static_cast<nv::mctp::Task*>(params);

    Driver drv(task, task.uuid);
    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Mctp);
    drv.main();
}
}  // namespace nv::mctp
