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

#include <chrono>
#include <cstdint>
#include <span>
#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <portmacro.h>
#include <task.h>

#include "nv/nv.h"

using namespace nv;
using namespace nv::ipc;

static_assert(int(Task::Priority::Idle) == tskIDLE_PRIORITY);
static_assert(int(Task::Priority::Max) <= configMAX_PRIORITIES);
static_assert(int(Task::Priority::Timer) == configTIMER_TASK_PRIORITY);

bool Task::setup(const std::span<uint8_t>&                  stack_region,
                 [[maybe_unused]] const std::span<uint8_t>& private_region,
                 Priority                                   priority,
                 EntryPoint                                 entrypoint)
{
    NV_ASSERT(handle() == nullptr, "Task already setup");
    nv::info("creating task '%s'\n", _name.data());
    _stack_region = stack_region;
    handle()      = xTaskCreateStatic(
        entrypoint,
        _name.data(),
        stack_region.size() / sizeof(StackType_t),
        this,
        static_cast<UBaseType_t>(priority),
        reinterpret_cast<StackType_t*>(stack_region.data()),  // NOLINT(*-reinterpret-cast)
        &tcb());
    return true;
}

// GBS:BEGIN NO COVERAGE FIXME!!
void Task::setup_shared_region(MemoryRegion_t& mr) const {}

void Task::setup_peripheral_region(MemoryRegion_t& mr) const {}

void Task::setup_rom_api_region(MemoryRegion_t& mr) const {}

void Task::setup_private_region(MemoryRegion_t& mr, const std::span<uint8_t>& priv) const {}
// GBS:END NO COVERAGE FIXME!!

void Task::suspend() const
{
    nv::info("suspending task '%s'\n", _name.data());
    vTaskSuspend(handle());
}

void Task::resume() const
{
    nv::info("resuming task '%s'\n", _name.data());
    vTaskResume(handle());
}

void Task::delay(std::chrono::microseconds us) const
{
    constexpr auto OneSecInUsecs = 1'000'000;
    vTaskDelay((configTICK_RATE_HZ * us.count()) / OneSecInUsecs);
}

Task::Status Task::status() const
{
    nv::assert(handle() != nullptr);
    TaskStatus_t details{};
    vTaskGetInfo(handle(), &details, false, eInvalid);
    switch (details.eCurrentState) {
        case eSuspended: return Status::Suspended;
        case eDeleted  :
        case eInvalid  : return Status::Dead;
        default        : break;
    }
    return Status::Running;
}
