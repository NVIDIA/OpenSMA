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
#include <utility>

#include "nv/bootloader.h"
#include "nv/gpio/driver.h"
#include "nv/nv.h"
#include "nv/watchdog/runtime.h"
#include "sys/ipc/driver.h"
#include "sys/ipc/task.h"
#include "nv/logger/log.h"

using namespace nv::ssif;
using namespace std::chrono_literals;

namespace {

// Instance index to per-instance IPC identity. Instance N carries the N suffix, the same shape
// as the TaskMonitorIndex::I2cN to QueueId::I2cN switch in i2c::Task::wdt_notify. Every project
// that builds this module declares the full set and stubs the entries it does not use, as those
// projects already do for QueueId::I2c6 through I2c9. Kept here rather than in the header
// because these enumerators only exist in projects that build the SSIF module.
constexpr nv::ipc::EventId instance_event(size_t instance)
{
    switch (instance) {
        case 0 : return nv::ipc::EventId::Ssif0;
        case 1 : return nv::ipc::EventId::Ssif1;
        default: return nv::ipc::EventId::End;
    }
}

constexpr nv::ipc::QueueId instance_queue(size_t instance)
{
    switch (instance) {
        case 0 : return nv::ipc::QueueId::Ssif0;
        case 1 : return nv::ipc::QueueId::Ssif1;
        default: return nv::ipc::QueueId::End;
    }
}

template<size_t Instance, typename TaskId>
constexpr TaskId instance_task()
{
    if constexpr (Instance == 0) {
        return TaskId::Ssif0;
    }
    else {
        return TaskId::Ssif1;
    }
}

template<size_t Instance, typename BootedEventBits>
constexpr BootedEventBits instance_booted_bit()
{
    if constexpr (Instance == 0) {
        return BootedEventBits::Ssif0;
    }
    else {
        return BootedEventBits::Ssif1;
    }
}

template<size_t Instance>
constexpr nv::watchdog::TaskMonitorIndex instance_wdt_index()
{
    if constexpr (Instance == 0) {
        return nv::watchdog::TaskMonitorIndex::Ssif0;
    }
    else {
        return nv::watchdog::TaskMonitorIndex::Ssif1;
    }
}

template<size_t Instance>
constexpr std::string_view instance_name()
{
    if constexpr (Instance == 0) {
        return "SSIF0";
    }
    else {
        return "SSIF1";
    }
}

// The Ssif* monitor indices form one contiguous run, which the static_assert in task.h keeps
// true, so the instance is simple arithmetic rather than a search.
constexpr size_t instance_for_wdt_index(nv::watchdog::TaskMonitorIndex index)
{
    constexpr auto First = static_cast<size_t>(nv::watchdog::TaskMonitorIndex::Ssif0);
    const auto     raw   = static_cast<size_t>(index);
    if ((raw < First) || (raw >= First + MaxInstances)) {
        return InvalidInstance;
    }
    return raw - First;
}

template<size_t Channel>
void make_configured_instance()
{
    constexpr auto& entry = nv::lstp::LstpChannels[Channel];
    if constexpr (entry.info.type == nv::lstp::LstpChannelType::Ipmi) {
        constexpr auto  instance = entry.info.id;
        constexpr auto& config   = std::get<nv::lstp::LstpIpmiChannelConfig>(entry.config);

        static_assert(instance < MaxInstances, "SSIF instance id exceeds MaxInstances");
        static_assert(nv::lstp::GetChannelId(nv::lstp::LstpChannels,
                                             nv::lstp::LstpChannelType::Ipmi,
                                             config.i2c_port)
                          == Channel,
                      "SSIF I2C port does not map to a unique LSTP IPMI channel");

        // Set SSIF alert pin to high (inactive)
        nv::gpio::Driver::init_pin(config.alert_port,
                                   config.alert_pin,
                                   nv::gpio::Direction::Output,
                                   nv::gpio::GpioState::High);

        Task::Config task_config{instance_task<instance, nv::ipc::TaskId>(),
                                 instance_name<instance>(),
                                 instance_event(instance),
                                 config.i2c_port,
                                 config.alert_port,
                                 config.alert_pin,
                                 instance_queue(instance),
                                 instance_booted_bit<instance, nv::ipc::BootedEventBits>(),
                                 instance_wdt_index<instance>()};

        if (sys::ipc::task::Driver::can_direct_access_on_current_core(task_config.task_id)) {
            Task::make<instance>(task_config);
        }
    }
}

template<size_t... Channels>
void make_configured_instances(std::index_sequence<Channels...> /*channels*/)
{
    (make_configured_instance<Channels>(), ...);
}

}  // namespace

Task::Config::Config(nv::ipc::TaskId  task_id_,
                     std::string_view task_name_,
                     nv::ipc::EventId event_,
                     nv::i2c::Port    i2c_bus_,
                     gpio::GpioPort   alert_port_id_,
                     gpio::GpioPin    alert_pin_id_)
: Config(task_id_,
         task_name_,
         event_,
         i2c_bus_,
         alert_port_id_,
         alert_pin_id_,
         nv::ipc::QueueId::Ssif0,
         nv::ipc::BootedEventBits::Ssif0,
         watchdog::TaskMonitorIndex::Ssif0)
{}

Task::Config::Config(nv::ipc::TaskId            task_id_,
                     std::string_view           task_name_,
                     nv::ipc::EventId           event_,
                     nv::i2c::Port              i2c_bus_,
                     gpio::GpioPort             alert_port_id_,
                     gpio::GpioPin              alert_pin_id_,
                     nv::ipc::QueueId           queue_id_,
                     nv::ipc::BootedEventBits   booted_bit_,
                     watchdog::TaskMonitorIndex wdt_index_)
: task_id(task_id_)
, task_name(task_name_)
, event(event_)
, i2c_bus(i2c_bus_)
, alert_port_id(alert_port_id_)
, alert_pin_id(alert_pin_id_)
, queue_id(queue_id_)
, booted_bit(booted_bit_)
, wdt_index(wdt_index_)
{}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

Task::Task(Config config) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, _config(config)
, _event(nv::ipc::Event::make(config.event))
, _ssif(_event)
{
    const auto channel_id = nv::lstp::GetChannelId(
        nv::lstp::LstpChannels, nv::lstp::LstpChannelType::Ipmi, config.i2c_bus);
    NV_ALWAYS_ASSERT(channel_id < nv::lstp::LstpChannels.size());
    _ssif.bind(config.i2c_bus, config.alert_port_id, config.alert_pin_id, channel_id);
}

void Task::make_all()
{
    make_configured_instances(std::make_index_sequence<nv::lstp::LstpChannels.size()>{});
}

bool Task::to_ssif(uint8_t lstp_channel_id, std::span<uint8_t>& msg)
{
    if (msg.size() < sizeof(Ssif::Packet)) {
        return false;
    }

    const auto instance = instance_for_channel(lstp_channel_id);
    if (instance == InvalidInstance) {
        nv::logger::error(nv::logger::Event::SsifInvalidChannel, {lstp_channel_id});
        return false;
    }

    std::array<uint8_t, sizeof(Ssif::Packet)> tmp_buffer{};
    auto queue    = nv::ipc::Queue::make(instance_queue(instance));
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

    auto event = nv::ipc::Event::make(instance_event(instance));
    event.set(TxReady);

    return true;
}

bool Task::is_default_enabled(uint8_t lstp_channel_id)
{
    const auto channels = nv::lstp::project_lstp_channels();
    if (lstp_channel_id >= channels.size()) {
        return false;
    }

    const auto& entry = channels[lstp_channel_id];
    if (entry.info.type != nv::lstp::LstpChannelType::Ipmi
        || !std::holds_alternative<nv::lstp::LstpIpmiChannelConfig>(entry.config)) {
        return false;
    }

    return std::get<nv::lstp::LstpIpmiChannelConfig>(entry.config).default_enabled;
}

void Task::enable(uint8_t lstp_channel_id)
{
    const auto instance = instance_for_channel(lstp_channel_id);
    if (instance == InvalidInstance) {
        nv::logger::error(nv::logger::Event::SsifInvalidChannel, {lstp_channel_id});
        return;
    }

    auto event = nv::ipc::Event::make(instance_event(instance));
    event.set(SlaveEnable);
}

void Task::peripheral_recovery(uint8_t lstp_channel_id)
{
    const auto instance = instance_for_channel(lstp_channel_id);
    if (instance == InvalidInstance) {
        nv::logger::error(nv::logger::Event::SsifInvalidChannel, {lstp_channel_id});
        return;
    }

    auto event = nv::ipc::Event::make(instance_event(instance));
    event.set(PeripheralRecovery);
}

void Task::wdt_notify(watchdog::TaskMonitorIndex index)
{
    // Runtime::query_task_status() routes only TaskMonitorIndex::Ssif0 and Ssif1 here, and the
    // static_assert in task.h pins that pair to the whole MaxInstances run, so this lookup
    // cannot miss by construction. Unlike the lstp_channel_id the other entry points take,
    // the index is internal dispatch state rather than host input, so a miss would mean
    // the Ssif* monitor run and MaxInstances had drifted apart in a build. Assert so
    // that surfaces during bring-up, and still bail out in release rather than
    // deriving IPC ids from an out-of-range instance.
    const auto instance = instance_for_wdt_index(index);
    NV_ASSERT(instance != InvalidInstance);
    if (instance == InvalidInstance) {
        nv::logger::error(nv::logger::Event::SsifInvalidChannel, {static_cast<uint8_t>(index)});
        return;
    }

    // The caller has already set this index's alive bit and only the task clears it, by calling
    // Runtime::mark_task_alive() once it observes WdtNotify. A dropped notification therefore
    // strands the bit: Runtime::feed() is never reached and the hardware watchdog resets the
    // board, so a failed set has to leave a trace instead of being swallowed.
    auto event = nv::ipc::Event::make(instance_event(instance));
    auto sts   = event.set(WdtNotify);
    if (sts != nv::ipc::Event::Status::Ok) {
        nv::logger::error(nv::logger::Event::SsifWdtNotifyFailed,
                          {static_cast<uint8_t>(index), static_cast<uint8_t>(sts)});
    }
}

[[noreturn]] void Task::start()
{
    auto queue   = nv::ipc::Queue::make(_config.queue_id);
    auto tx_item = _ssif.get_tx_buffer();

    const auto channel_id = nv::lstp::GetChannelId(
        nv::lstp::LstpChannels, nv::lstp::LstpChannelType::Ipmi, _config.i2c_bus);
    if (is_default_enabled(channel_id)) {
        _ssif.start();
    }

    nv::bootloader::Driver::set_task_booted(_config.booted_bit);

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
            watchdog::Runtime::mark_task_alive(_config.wdt_index);
            _event.clear(WdtNotify);
        }

        auto elapsed = _ssif.get_target_timeout_elapsed();
        if (elapsed > MaxActivityTimeoutUs) {
            nv::logger::error(nv::logger::Event::SsifTargetTimeout, {});
            _ssif.peripheral_recovery();
        }
    }
}
