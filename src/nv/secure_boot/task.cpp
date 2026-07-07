/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/secure_boot/task.h"

#include "nv/common/debug.h"
#include "nv/nv.h"

namespace nv::secure_boot {
namespace {

template<std::size_t N>
constexpr bool is_supported_secure_boot_ap_list(const std::array<nv::vrot::ApInfo, N>& list)
{
    if constexpr (N > 1) {
        return false;
    }
    for (const auto& ap : list) {
        if (static_cast<uint8_t>(ap.id) >= MaxApCount) {
            return false;
        }
    }
    return true;
}

static_assert(is_supported_secure_boot_ap_list(nv::vrot::ApList),
              "SecureBoot currently supports one AP with an ApId below MaxApCount");

}  // namespace

Task::Task(const nv::vrot::ApInfo& ap_info)
: ipc::Task(ipc::TaskId::SecureBoot, "SecureBoot")
, secure_boot(ap_info)
{}

void Task::make(const nv::vrot::ApInfo& ap_info)
{
    // Ignore duplicate make() calls; this task owns a single static instance.
    static bool created = false;
    if (created) {
        return;
    }
    created = true;

    constexpr auto           StackSize = std::max(1024, int(configMINIMAL_STACK_SIZE));
    NV_TASK_DATA static Task task(ap_info);
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::SecureBoot, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.main();
}

[[noreturn]] void Task::main()
{
    secure_boot.run();
}

}  // namespace nv::secure_boot
