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
#pragma once
#include <algorithm>
#include <cstddef>
#include <string_view>

#include "nv/ipc/event.h"
#include "nv/ipc/task.h"
#include "nv/lstp/lstp_common.h"
#include "nv/ssif/ssif.h"
#include "nv/watchdog/notify_interface.h"

namespace nv::ssif {

// Timeout for waiting on event bits
static constexpr auto TaskEventWaitTimeout = nv::ipc::Event::Usecs(100000);

// Upper bound on the number of SSIF interfaces a single build may instantiate. Keeps the
// instance registry a fixed-size array and lets an out-of-range channel be rejected cheaply.
// This is a type-system ceiling, not a capacity claim: each live instance costs roughly 4 KB
// of RAM, so the practical limit on any given board is its RAM and Flexcomm count.
constexpr static size_t MaxInstances = nv::lstp::LstpIpmiMaxInstances;

// Every instance needs its own watchdog monitor index, so raising MaxInstances means adding
// matching TaskMonitorIndex::Ssif* values. Asserted rather than commented so the two files
// cannot drift apart silently.
static_assert(static_cast<size_t>(watchdog::TaskMonitorIndex::Ssif0) + MaxInstances - 1
                  == static_cast<size_t>(watchdog::TaskMonitorIndex::Ssif1),
              "MaxInstances must match the run of contiguous TaskMonitorIndex::Ssif* values");

constexpr size_t InvalidInstance = MaxInstances;

constexpr size_t instance_for_channel(uint8_t lstp_channel_id)
{
    if (lstp_channel_id >= nv::lstp::LstpChannels.size()
        || nv::lstp::LstpChannels[lstp_channel_id].info.type
               != nv::lstp::LstpChannelType::Ipmi) {
        return InvalidInstance;
    }

    return nv::lstp::LstpChannels[lstp_channel_id].info.id;
}

class Task : public nv::ipc::Task
{
public:
    struct Config
    {
        nv::ipc::TaskId            task_id;
        std::string_view           task_name;
        nv::ipc::EventId           event;
        nv::i2c::Port              i2c_bus;
        gpio::GpioPort             alert_port_id;
        gpio::GpioPin              alert_pin_id;
        nv::ipc::QueueId           queue_id;
        nv::ipc::BootedEventBits   booted_bit;
        watchdog::TaskMonitorIndex wdt_index;

        // Single-interface form, kept so existing projects construct exactly as before.
        // Defined out of line because the SSIF IPC enumerators only exist in projects that
        // build this module.
        Config(nv::ipc::TaskId  task_id_,
               std::string_view task_name_,
               nv::ipc::EventId event_,
               nv::i2c::Port    i2c_bus_,
               gpio::GpioPort   alert_port_id_,
               gpio::GpioPin    alert_pin_id_);

        // Multi-interface form: every per-instance IPC identity supplied explicitly.
        Config(nv::ipc::TaskId            task_id_,
               std::string_view           task_name_,
               nv::ipc::EventId           event_,
               nv::i2c::Port              i2c_bus_,
               gpio::GpioPort             alert_port_id_,
               gpio::GpioPin              alert_pin_id_,
               nv::ipc::QueueId           queue_id_,
               nv::ipc::BootedEventBits   booted_bit_,
               watchdog::TaskMonitorIndex wdt_index_);
    };

    // Instance selects which static task and stack storage this call uses.
    template<size_t Instance = 0>
    static void make(Config config);
    static void make_all();

    static void entrypoint(void* params);

    // The LSTP channel id is the routing key for all of these: it is what the host uses to
    // address one SSIF interface rather than another.
    static bool to_ssif(uint8_t lstp_channel_id, std::span<uint8_t>& msg);
    static void enable(uint8_t lstp_channel_id);
    static void peripheral_recovery(uint8_t lstp_channel_id);
    static bool is_default_enabled(uint8_t lstp_channel_id);

    // Keyed on the monitor index instead, to match the i2c / i3c convention.
    static void wdt_notify(watchdog::TaskMonitorIndex index);

    Task(Config config) noexcept;
    [[noreturn]] void start();

private:
    Config         _config;
    nv::ipc::Event _event;
    Ssif           _ssif;
};

// Owns one interface's task and stack. Internal linkage on purpose, mirroring
// nv::i2c::make_task_by_port: a member template would give these statics COMDAT linkage,
// which conflicts with the plain "task_data" / "stack" sections other modules place objects
// in within the same translation unit. Being a template also means a project only pays RAM
// for the instances it actually creates.
template<size_t Instance>
static void make_ssif_instance(Task::Config config)
{
    static_assert(Instance < MaxInstances, "SSIF instance index exceeds MaxInstances");

    // leave ~288 bytes of margin above the worst-case observed stack usage
    constexpr auto StackSize = std::max(320, int(configMINIMAL_STACK_SIZE));

    NV_TASK_DATA static Task                       task(config);
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), priv, Task::Priority::I2c, Task::entrypoint);
}

template<size_t Instance>
void Task::make(Config config)
{
    make_ssif_instance<Instance>(config);
}
}  // namespace nv::ssif
