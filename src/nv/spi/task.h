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

#include "nv/gpio/common.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/spi/common.h"
#include "nv/spi/port.h"
#include "nv/spi/spb.h"
#include "sys/spi/spi.h"

using namespace std::chrono_literals;

namespace nv::spi {
class Task : public nv::ipc::Task
{
public:
    struct Config
    {
        nv::ipc::TaskId          task_id;
        std::string_view         task_name;
        MasterDevice             master_device;
        SlaveDevice              slave_device;
        nv::spi::Port            port_id;
        nv::spi::Flexcomm        flexcomm_id;
        gpio::GpioPort           cs_port_id;
        gpio::GpioPin            cs_pin_id;
        nv::mctp::Client         client_id;
        nv::ipc::EventId         event_id;
        nv::ipc::QueueId         queue_id;
        nv::ipc::BootedEventBits boot_event;
        bool                     overwrite_freq;
        uint8_t                  freq;
        Config(nv::ipc::TaskId          task_id_,
               std::string_view         task_name_,
               MasterDevice             master_device_,
               SlaveDevice              slave_device_,
               nv::spi::Port            port_id_,
               nv::spi::Flexcomm        flexcomm_id_,
               gpio::GpioPort           cs_port_id_,
               gpio::GpioPin            cs_pin_id_,
               nv::mctp::Client         client_id_,
               nv::ipc::EventId         event_id_,
               nv::ipc::QueueId         queue_id_,
               nv::ipc::BootedEventBits boot_event_,
               bool                     overwrite_freq_,
               uint8_t                  freq_)
        : task_id(task_id_)
        , task_name(task_name_)
        , master_device(master_device_)
        , slave_device(slave_device_)
        , port_id(port_id_)
        , flexcomm_id(flexcomm_id_)
        , cs_port_id(cs_port_id_)
        , cs_pin_id(cs_pin_id_)
        , client_id(client_id_)
        , event_id(event_id_)
        , queue_id(queue_id_)
        , boot_event(boot_event_)
        , overwrite_freq(overwrite_freq_)
        , freq(freq_)
        {}
    };

    enum class Status
    {
        Ok,
        InvalidInterface,
        FailToPutTxPacket,
        FailToSetTxEvent,
        FailToSetRoutingTableEvent,
    };

    static void         make(Config config);
    static void         entrypoint(void* params);
    static Task::Status tx(const mctp::Packet& packet);
    static Task::Status update_routing_table(mctp::Client client);
    static void         error_log(SpbStatus status, mctp::Client client);
    static void         timeout_log(Timeout timeout);
    static void         middle_error_log(SpbStatus status);
    static void handle_mbx_interrupt(ipc::EventId event_id, mctp::Client client);  // only
                                                                                   // called by
                                                                                   // interrupt
                                                                                   // handler

    Task(Config config) noexcept;
    [[noreturn]] void start();

private:
    using RoutingTable = std::array<mctp::ShardRoutingTable, ipc::RoutingTableSize>;
    RoutingTable             _routing_table{};
    sys::spi::Driver         _driver;
    nv::mctp::Client         _client;
    nv::ipc::Event&          _event;
    nv::ipc::Queue&          _queue;
    nv::ipc::BootedEventBits _boot_event;
    MasterDevice             _master_device;
    SlaveDevice              _slave_device;
    Spb                      _spb;
    nv::perf_mon::OobBus     _oob_bus;

    constexpr static bool EnableMctpOverSpiDebugLog = false;

    void handle_rx();
    void handle_tx(Buffer& buffer);
    void handle_update_routing_table();

    void forward(Buffer& buffer);
};
}  // namespace nv::spi
