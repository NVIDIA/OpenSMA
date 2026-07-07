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
#include "nv/mctp/enums.h"
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

    ipc::Timer::make(ipc::TimerId::MctpEnumerate,
                     Driver::EnumeratePeriod,
                     mctp::Driver::on_enumeration_done_timer,
                     false);

    ipc::Timer::make(ipc::TimerId::MctpEnumerateStart,
                     ipc::EnumerateStartMs ? ipc::EnumerateStartMs * 1000us : 1s,
                     mctp::Driver::on_enumeration_start_timer,
                     false);

    static_assert(!nv::ipc::EnableEndpointStatusChangeDebounce
                      || nv::ipc::EndpointStatusChangePeriodMs != 0,
                  "EndpointStatusChangePeriodMs must not be 0 when "
                  "EndpointStatusChangeDebounce is enabled");

    if constexpr (nv::ipc::EnableEndpointStatusChangeDebounce) {
        for (uint8_t i = 0; i < ipc::DownStreamNum; ++i) {
            ipc::Timer::make(static_cast<ipc::TimerId>(
                                 static_cast<int>(ipc::TimerId::EndpointStatusChangeStart) + i),
                             Driver::EndpointStatusChangePeriod,
                             mctp::Driver::on_endpoint_status_change_timer,
                             false);
        }
    }
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);

    using namespace std::chrono_literals;
    auto& task = *static_cast<nv::mctp::Task*>(params);

    task.power_sensor_mgr().identify_power_sensors();
    Driver drv(task, task.uuid);
    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Mctp);
    drv.main();
}

nv::i2c::power::DeviceManager& get_power_sensor_mgr()
{
    auto& base_task = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Mctp);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto& mctp_task = static_cast<nv::mctp::Task&>(base_task);
    return mctp_task.power_sensor_mgr();
}

Task& get_task()
{
    auto& base_task = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Mctp);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    return static_cast<nv::mctp::Task&>(base_task);
}

bool Task::ep_stall_detected()
{
    auto bitmap = static_cast<uint8_t>(port_recovery_ei_bitmap_);
    return (bitmap
            & (1u << static_cast<uint8_t>(
                   PortRecoveryEIPayloadValues::PortRecoveryL1MCTPEPStall)))
        != 0;
}

bool Task::bridge_hang_detected()
{
    auto bitmap = static_cast<uint8_t>(port_recovery_ei_bitmap_ >> 8);
    return (bitmap
            & (1u << static_cast<uint8_t>(
                   PortRecoveryEIPayloadValues::PortRecoveryL2MCTPBridgeHang)))
        != 0;
}

bool Task::pldm_t5_hang_detected()
{
    auto bitmap = static_cast<uint8_t>(port_recovery_ei_bitmap_ >> 8);
    return (bitmap
            & (1u << static_cast<uint8_t>(
                   PortRecoveryEIPayloadValues::PortRecoveryL2PLDMT5Hang)))
        != 0;
}

}  // namespace nv::mctp
