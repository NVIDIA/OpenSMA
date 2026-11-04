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
#include "nv/thermal/task.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>

#include "nv/common/debug.h"
#include "nv/common/preproc.h"
#include "nv/i2c/port.h"
#include "nv/i2c/tmp468.h"
#include "nv/ipc/event.h"
#include "nv/ipc/timer.h"
#include "nv/thermal/state.h"

#include NV_IPC_CONFIG_H

namespace nv::thermal {

using namespace std::chrono_literals;

namespace {

// TMP468 location on E4188: I2C address 0x48 on Port::Six (FLEXCOMM6, the bus
// owned by the I2c5 task). See the "Thermal sensor" entry in the e4188
// I2cVirtualAddressMappingTable and make_i2c_task() in main.cpp.
constexpr nv::i2c::Port Tmp468Port    = nv::i2c::Port::Six;
constexpr uint8_t       Tmp468Address = 0x48;

// "TMP468 remote temp 20/s" => one full read every 50 ms.
constexpr auto PollPeriod = 50ms;

// Whiteboard unit: temperatures are stored in 1/16 C ticks (16 ticks per degree).
constexpr float TicksPerCelsius = 16.0F;

/// Driver gives Celsius; the whiteboard stores 1/16 C ticks. The driver value
/// is always an exact multiple of 1/16 C, so the multiply is exact.
int16_t celsius_to_ticks(float celsius)
{
    return static_cast<int16_t>(celsius * TicksPerCelsius);
}

// Event bit the periodic timer sets to wake the thermal task.
constexpr uint32_t TickBit = 1U << 0U;

// Timer expiry runs in the timer-daemon context and must NOT block, so it only
// wakes the thermal task; the blocking TMP468 read happens in poll_once().
void on_timer_tick(nv::ipc::Timer& /*timer*/)
{
    (void)nv::ipc::Event::make(nv::ipc::EventId::ThermalEvent).set(TickBit);
}

}  // namespace

Task& Task::inst()
{
    NV_TASK_DATA static Task task;
    return task;
}

Task::Task() : ipc::Task(ipc::TaskId::Thermal, "Thermal") {}

void Task::make()
{
    auto&          task      = inst();
    constexpr auto StackSize = std::max(1024, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), priv, Priority::Norm, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.main();
}

[[noreturn]] void Task::main()
{
    // Timer-driven (not a delay loop): a reload timer fires every PollPeriod and
    // wakes us via ThermalEvent, so the cadence is fixed regardless of how long
    // a read takes. The blocking TMP468 read still runs here in task context.
    auto& timer = nv::ipc::Timer::make(
        nv::ipc::TimerId::ThermalTick, PollPeriod, on_timer_tick, true);
    (void)timer.start();
    auto& event = nv::ipc::Event::make(nv::ipc::EventId::ThermalEvent);

    while (true) {
        const auto bits = event.wait(TickBit, false, false, 100ms);
        if (!bits.has_value()) {
            continue;  // no tick this window; just re-wait
        }
        event.clear(TickBit);
        poll_once();
    }
}

void Task::poll_once()
{
    // Tmp468 reads via sys::i2c, which takes the per-port mutex internally
    // (same lock the I2c5 task uses), so no manual bus lock here.
    nv::i2c::Tmp468 sensor(Tmp468Port, Tmp468Address);
    ThermalBoard&   board = ThermalState::inst().writer_buffer();
    board.reset();  // every slot starts InvalidTemp; overwrite the ones that read OK

    float celsius = 0.0F;
    if (sensor.get_local_temp(celsius) == nv::i2c::I2cStatus::Ok) {
        board.local = celsius_to_ticks(celsius);
    }
    for (uint8_t channel = 0; channel < ThermalBoard::RemoteCount; ++channel) {
        if (sensor.get_remote_temp(channel, celsius) == nv::i2c::I2cStatus::Ok) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            board.remote[channel] = celsius_to_ticks(celsius);
        }
    }

    // Fold this cycle's fresh readings into the running min/max.
    ThermalState::inst().observe_minmax(board);

    // TODO(thermal): GPU Tsense (GA/GB) is a separate source - normal-mode I2CS
    // read or IST-mode SMBPBI. Fill gpu_tsense[] here once the access method is
    // confirmed.

    ThermalState::inst().publish();
}

}  // namespace nv::thermal
