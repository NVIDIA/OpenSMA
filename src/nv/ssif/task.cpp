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

#include "nv/ssif/task.h"

#include <chrono>
#include <cstdint>
#include <span>

#include "nv/bootloader.h"
#include "nv/nv.h"
#include "nv/watchdog/runtime.h"
#include "sys/ipc/task.h"
#include "nv/logger/log.h"

using namespace nv::ssif;
using namespace std::chrono_literals;

void Task::make(Config config)
{
    constexpr auto StackSize = std::max(128, int(configMINIMAL_STACK_SIZE));

    NV_TASK_DATA static Task                       task(config);
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), priv, Priority::I2c, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

Task::Task(Config config) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, _event(nv::ipc::Event::make(config.event))
, _ssif(_event)
{
    _ssif.bind(config.i2c_bus, config.alert_port_id, config.alert_pin_id);
}

bool Task::to_ssif(std::span<uint8_t>& msg)
{
    if (msg.size() < sizeof(Ssif::Packet)) {
        return false;
    }

    std::array<uint8_t, sizeof(Ssif::Packet)> tmp_buffer{};
    auto queue    = nv::ipc::Queue::make(nv::ipc::QueueId::Ssif);
    auto tmp_item = nv::ipc::Queue::Item(tmp_buffer);
    auto status   = queue.recv(tmp_item, 0us);
    if (status == nv::ipc::Queue::Status::Ok) {
        // Previous message was not consumed yet, override
        nv::logger::info(nv::logger::Event::SsifTxDataOverriden, {});
    }

    auto item = msg.subspan(0, sizeof(Ssif::Packet));
    status    = queue.send(item, 100ms);
    if (status != nv::ipc::Queue::Status::Ok) {
        nv::logger::error(nv::logger::Event::SsifTxQueueSendFailed,
                          {static_cast<uint8_t>(status)});
        return false;
    }

    auto event = nv::ipc::Event::make(nv::ipc::EventId::Ssif);
    event.set(TxReady);

    return true;
}

bool Task::is_default_enabled()
{
    // SSIF is disabled by default
    return false;
}

void Task::enable()
{
    auto event = nv::ipc::Event::make(nv::ipc::EventId::Ssif);
    event.set(SlaveEnable);
}

void Task::peripheral_recovery()
{
    auto event = nv::ipc::Event::make(nv::ipc::EventId::Ssif);
    event.set(PeripheralRecovery);
}

void Task::wdt_notify()
{
    auto event = nv::ipc::Event::make(nv::ipc::EventId::Ssif);
    auto sts   = event.set(WdtNotify);
    if (sts != nv::ipc::Event::Status::Ok) {
        // TBD
    }
}

[[noreturn]] void Task::start()
{
    auto queue   = nv::ipc::Queue::make(nv::ipc::QueueId::Ssif);
    auto tx_item = _ssif.get_tx_buffer();

    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Ssif);

    // This task is designed to run for the lifetime of the system
    // coverity[no_escape : FALSE]*/
    while (true) {
        // Due to SSIF protocol specifics (stateful SMBus Block Read operations, autoincrement
        // registers etc) SMBus layer logic needs to live in ISR context. This task handles
        // fully assembled IPMI messages on USB/LSTP layer.
        auto wait_bits  = SlaveEnable | RxReady | TxReady | PeripheralRecovery | WdtNotify;
        auto event      = _event.wait(wait_bits, false, false, TaskEventWaitTimeout);
        auto event_bits = event.value() & wait_bits;

        if (event_bits & SlaveEnable) {
            _ssif.start();
            _event.clear(SlaveEnable);
        }

        if (event_bits & RxReady) {
            _ssif.handle_rx();
            _event.clear(RxReady);
        }

        if (event_bits & TxReady) {
            // Responses received directly into TX buffer to avoid extra copy
            auto status = queue.recv(tx_item, 100ms);
            if (status == nv::ipc::Queue::Status::Ok) {
                _ssif.handle_tx();
            }
            else {
                nv::logger::error(nv::logger::Event::SsifTxQueueRecvFailed,
                                  {static_cast<uint8_t>(status)});
            }
            _event.clear(TxReady);
        }

        if (event_bits & PeripheralRecovery) {
            _ssif.peripheral_recovery();
            _event.clear(PeripheralRecovery);
        }

        if (event_bits & WdtNotify) {
            watchdog::Runtime::mark_task_alive(watchdog::TaskMonitorIndex::Ssif);
            _event.clear(WdtNotify);
        }

        auto elapsed = _ssif.get_target_timeout_elapsed();
        if (elapsed > MaxActivityTimeoutUs) {
            nv::logger::error(nv::logger::Event::SsifTargetTimeout, {});
            _ssif.peripheral_recovery();
        }
    }
}
