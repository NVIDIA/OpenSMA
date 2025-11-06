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
#include <span>
#include <FreeRTOS.h>
#include <task.h>

#include "nv/common/utils.h"
#include "sys/common/address_map.h"

namespace sys::ipc {

/// Generic memory region information for mcxn547 & mcxn556.
enum class MpuRegion : uint32_t
{
    PeripheralRegionAddr = 0x40000000 | sys::common::SecureAddr,
    PeripheralRegionSize = 0x140000,
    RomApiRegionAddr     = 0x13000000,
    RomApiRegionSize     = 0x40000,
};
/**
 * Used to ensure correct alignment and sizing of a task's static stack. Kept seperate
 * from the Task class to permit the compilier to better pack the Task and TaskStack.
 * Should be declared as a static global for each task.
 * all task stack need 32B align since CM33 MPU region size must 32B align.
 */
template<std::size_t stack_size>
requires(nv::common::is_32B_align(stack_size) && stack_size >= configMINIMAL_STACK_SIZE)
// coverity[definition]
// coverity[entity_not_emitted]
struct TaskStack
{
    alignas(32) std::array<StackType_t, stack_size> _stack;
    // coverity[routine_not_emitted]
    auto begin() { return _stack.begin(); }
    // coverity[routine_not_emitted]
    auto end() { return _stack.end(); }
    // coverity[routine_not_emitted]
    auto span()
    {
        // NOLINTBEGIN(*-reinterpret-cast)
        return std::span<uint8_t>(reinterpret_cast<uint8_t*>(begin()),
                                  reinterpret_cast<uint8_t*>(end()));
        // NOLINTEND(*-reinterpret-cast)
    }
};

/// FreeRTOS Posix Task Data.
class Task
{
public:
    auto& handle() { return _handle; }
    auto& handle() const { return _handle; }
    auto& tcb() { return _tcb; }

private:
    StaticTask_t _tcb{};
    TaskHandle_t _handle{};
};

class TaskPublic
{
public:
    auto& handle() { return _handle; }
    auto& handle() const { return _handle; }

private:
    TaskHandle_t _handle{};
};

}  // namespace sys::ipc
