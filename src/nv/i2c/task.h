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

#include "nv/i2c/port.h"
#include "nv/i2c/common.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/ipc/timer.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/router.h"
#include "sys/i2c/i2c.h"
#include "nv/perf_mon/perf_mon.h"
namespace nv::i2c {

class Task : public nv::ipc::Task
{
public:
    static constexpr size_t  DataSize = 76;
    static constexpr uint8_t DynAddr  = 0;
    static constexpr uint8_t ByteMask = 0xFF;

    struct Config
    {
        nv::ipc::TaskId          task_id;
        std::string_view         task_name;
        nv::mctp::Client         client;
        nv::ipc::EventId         event_id;
        nv::ipc::QueueId         queue_id;
        Port                     port_id;
        uint8_t                  target_addr;
        nv::ipc::BootedEventBits boot_event;
        nv::ipc::TimerId         timer_id;
        nv::ipchandler::Id       ipchandler_id;
        Config(nv::ipc::TaskId          task_id_,
               std::string_view         task_name_,
               nv::mctp::Client         client_,
               nv::ipc::EventId         event_id_,
               nv::ipc::QueueId         queue_id_,
               Port                     port_id_,
               uint8_t                  target_addr_,
               nv::ipc::BootedEventBits boot_event_,
               nv::ipc::TimerId         timer_id_      = nv::ipc::TimerId::End,
               nv::ipchandler::Id       ipchandler_id_ = nv::ipchandler::Id::Unuse)
        : task_id(task_id_)
        , task_name(task_name_)
        , client(client_)
        , event_id(event_id_)
        , queue_id(queue_id_)
        , port_id(port_id_)
        , target_addr(target_addr_)
        , boot_event(boot_event_)
        , timer_id(timer_id_)
        , ipchandler_id(ipchandler_id_)
        {}
    };

    enum Event : nv::ipc::Event::Bits
    {
        CtrlDone    = 1_bit,
        CtrlBusBusy = 2_bit,
        CtrlNak     = 3_bit,
        CtrlArbLost = 4_bit,
        CtrlError   = 5_bit,
    };

    enum class RequestType : uint16_t
    {
        MctpRx,
        MctpTx,
        MctpUpdateRoutingTable,
        MctpRxError,
        UpdateSensor,
        WdtEvent,
        I2cRequest,
        I2cResponse,
        CheckApStatus
    };

    enum class Status
    {
        Ok,
        InvalidInterface,
        FailToPutTxPacket,
        FailToSetEvent,
    };

    struct [[gnu::packed]] Request
    {
        RequestType type;
        uint16_t    additional_info;
        uint8_t     data[DataSize];
    };

    static void         make(Config config);
    static void         entrypoint(void* params);
    static Task::Status tx(const nv::mctp::Packet& packet, uint16_t additional_info = 0);
    static Task::Status update_routing_table(mctp::Client client);
    static void         update_sensor(nv::ipc::Timer& timer);
    static Task::Status wdt_notify(nv::watchdog::TaskMonitorIndex taskId);
    static void         check_ap_status(nv::ipc::Timer& timer);

    static bool to_i2c(ipchandler::Id    src_id,
                       uint8_t           address,
                       ipchandler::Id    i2c_ipchandler_id,
                       uint8_t           write_length,
                       uint16_t          read_length,
                       ipc::Queue::Item& item);

    Task(Config config) noexcept;
    [[noreturn]] void start();
    void              rx(std::span<uint8_t> buffer);
    void              rx_error();
    Task::Status      set_event(Event event, bool isr = false);

private:
    /// add 1 byte for PEC
    constexpr static size_t MctpI2cpayload = sizeof(nv::mctp::Packet::msg) + 1;
    using RoutingTable = std::array<mctp::ShardRoutingTable, ipc::RoutingTableSize>;

    struct [[gnu::packed]] I2cHeader
    {
        uint8_t rw_dst   : 1;
        uint8_t dst_addr : 7;
        uint8_t cmd_code;
        uint8_t byte_cnt;
        uint8_t ipmi_src : 1;
        uint8_t src_addr : 7;
    };

    struct [[gnu::packed]] I2cPacket
    {
        I2cHeader        i2c_hdr;
        nv::mctp::Header mctp_hdr;
        uint8_t          msg[MctpI2cpayload];
    };

    enum class Error : uint8_t
    {
        Ok,
        CtrlBusBusy,
        CtrlNak,
        CtrlArbLost,
        CtrlError,
        CtrlTimeout,
        TargetRxError,
    };

    enum class APStatus : uint8_t
    {
        Querying,
        ApDown,
        ApUpNotEnumerated,
        ApUpEnumerated
    };

    RoutingTable                   _routing_table{};
    nv::mctp::Client               _client;
    nv::ipc::Event&                _event;
    nv::ipc::Queue&                _queue;
    Port                           _port;
    sys::i2c::Driver               _driver;
    std::array<uint8_t, UINT8_MAX> _eid_addr_map;
    uint8_t                        _target_addr;
    nv::ipc::BootedEventBits       _boot_event;
    nv::ipc::Timer                 _timer;
    nv::ipchandler::Id             _ipchandler_id;
    APStatus                       _ap_status{APStatus::Querying};

    void handle_rx(std::span<uint8_t> buffer);
    void handle_tx(std::span<uint8_t> buffer, uint16_t additional_info);
    void handle_update_sensor(std::span<uint8_t> buffer);
    void handle_wdt_event();
    bool process_rx_packet(std::span<uint8_t> buffer,
                           nv::mctp::Packet&  packet,
                           uint8_t&           command_code);
    bool
    process_tx_packet(std::span<uint8_t> buffer, I2cPacket& packet, uint16_t additional_info);
    void handle_update_routing_table();
    void forward(nv::mctp::Packet& packet, uint8_t command_code);
    bool transmit(const I2cPacket& packet);
    void handle_error(Error reason);
    void set_tmp_sensor();
    void handle_i2c_request(std::span<uint8_t> buffer);
    void handle_i2c_response(std::span<uint8_t> buffer);
    I2cStatus
    i2c_cmd(uint8_t address, std::span<uint8_t> write_buffer, std::span<uint8_t> read_buffer);
    void handle_ap_status(APStatus set_status);
    void attempt_i2c_recovery();

    // OOB Bus error telemetry
    nv::perf_mon::OobBus _oob_bus;
};

}  // namespace nv::i2c
