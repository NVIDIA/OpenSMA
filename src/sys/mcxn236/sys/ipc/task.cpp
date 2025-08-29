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
#include <mpu_wrappers.h>
#include <portmacrocommon.h>
#include <task.h>

#include "FreeRTOSConfig.h"

#include "nv/nv.h"

extern char nv_linker_var_shared_ram_start;  // NOLINT(*-non-const-global-variables)
extern char nv_linker_var_shared_ram_end;    // NOLINT(*-non-const-global-variables)

using namespace nv;
using namespace nv::ipc;

static_assert(int(Task::Priority::Idle) == tskIDLE_PRIORITY);
static_assert(int(Task::Priority::Max) <= configMAX_PRIORITIES);
static_assert(int(Task::Priority::Timer) == configTIMER_TASK_PRIORITY);

bool Task::setup(const std::span<uint8_t>& stack_region,
                 const std::span<uint8_t>& private_region,
                 Priority                  priority,
                 EntryPoint                entrypoint)
{
    nv::info("Creating task '%s'\n", _name.data());
    _stack_region = stack_region;
    // coverity[cert_int31_c_violation] - valid cast
    TaskParameters_t td{
        .pvTaskCode = entrypoint,
        .pcName     = _name.data(),
        // coverity[cert_int31_c_violation] - valid cast
        .usStackDepth = static_cast<uint16_t>(stack_region.size() / sizeof(StackType_t)),
        .pvParameters = this,
        .uxPriority   = static_cast<UBaseType_t>(priority),

        // NOLINTNEXTLINE(*-reinterpret-cast)
        .puxStackBuffer = reinterpret_cast<StackType_t*>(stack_region.data()),
        .pxTaskBuffer   = &tcb(),

        // NOLINT(*-reinterpret-cast) .xRegions{}, .pxTaskBuffer   = &tcb(),
    };

    if ((id() >= TaskId::Privileged) && (id() < TaskId::EndPrivileged)) {
        td.uxPriority |= portPRIVILEGE_BIT;
    }

    // TODO: Remove access to peripheral region for unprivileged tasks
    setup_shared_region(td.xRegions[0]);
    setup_peripheral_region(td.xRegions[1]);
    setup_private_region(td.xRegions[2], private_region);

    if (xTaskCreateRestrictedStatic(&td, &handle()) != true) {
        nv::fatal("cannot create task %s\n", _name.data());
    }

    vTaskSetTaskNumber(handle(), static_cast<UBaseType_t>(id()) + 1);

    return true;
}

void Task::setup_shared_region(MemoryRegion_t& mr) const
{
    // coverity[cert_arr36_c_violation] - vakid
    mr.pvBaseAddress = &nv_linker_var_shared_ram_start;
    // coverity[cert_int31_c_violation] - valid address subtraction
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const auto end_address = reinterpret_cast<uint32_t>(&nv_linker_var_shared_ram_end);
    // coverity[cert_int31_c_violation] - valid address subtraction
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const auto start_address = reinterpret_cast<uint32_t>(&nv_linker_var_shared_ram_start);
    // coverity[cert_int30_c_violation] - will not wrap
    mr.ulLengthInBytes = end_address - start_address;
    mr.ulParameters    = tskMPU_REGION_READ_WRITE | tskMPU_REGION_NORMAL_MEMORY
                    | tskMPU_REGION_EXECUTE_NEVER;
}

void Task::setup_peripheral_region(MemoryRegion_t& mr) const
{
    // NOLINTNEXTLINE(*-reinterpret-cast)
    mr.pvBaseAddress   = reinterpret_cast<void*>(MpuRegion::PeripheralRegionAddr);
    mr.ulLengthInBytes = static_cast<uint32_t>(MpuRegion::PeripheralRegionSize);
    mr.ulParameters    = tskMPU_REGION_READ_WRITE | tskMPU_REGION_DEVICE_MEMORY
                    | tskMPU_REGION_EXECUTE_NEVER;
}

void Task::setup_private_region(MemoryRegion_t& mr, const std::span<uint8_t>& priv) const
{
    // NOLINTNEXTLINE(*-reinterpret-cast)
    mr.pvBaseAddress   = reinterpret_cast<void*>(priv.data());
    mr.ulLengthInBytes = static_cast<uint32_t>(priv.size());
    mr.ulParameters    = tskMPU_REGION_READ_WRITE | tskMPU_REGION_NORMAL_MEMORY;
}

void Task::setup_rom_api_region(MemoryRegion_t& mr) const
{
    // NOLINTNEXTLINE(*-reinterpret-cast)
    mr.pvBaseAddress   = reinterpret_cast<void*>(MpuRegion::RomApiRegionAddr);
    mr.ulLengthInBytes = static_cast<uint32_t>(MpuRegion::RomApiRegionSize);
    mr.ulParameters    = tskMPU_REGION_READ_ONLY | tskMPU_REGION_NORMAL_MEMORY;
}

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
    constexpr auto OneSecUs = 1'000'000;
    // coverity[cert_int32_c_violation] - will not overflow
    auto ticks = configTICK_RATE_HZ * us.count() / OneSecUs;
    // coverity[cert_int31_c_violation] - valid cast
    vTaskDelay(static_cast<TickType_t>(ticks));
}

Task::Status Task::status() const
{
    TaskStatus_t details{};
    vTaskGetInfo(handle(), &details, false, eInvalid);
    switch (details.eCurrentState) {
        case eDeleted:
        case eInvalid: return Status::Dead;
        default      : break;
    }
    return Status::Running;
}
