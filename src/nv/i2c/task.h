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
#include <optional>
#include <functional>

#include "nv/i2c/port.h"
#include "nv/i2c/common.h"
#include "nv/i2c/eeprom_cache.h"
#include "nv/i2c/recovery.h"
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
    static constexpr size_t  DataSize                = nv::i2c::I2cBufferSize + 12;
    static constexpr uint8_t DynAddr                 = 0;
    static constexpr uint8_t ByteMask                = 0xFF;
    static constexpr uint8_t MaxSMBusBlockReadLength = 33;

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
        nv::ipc::TimerId         repeated_start_timer_id;
        constexpr Config(nv::ipc::TaskId          task_id_,
                         std::string_view         task_name_,
                         nv::mctp::Client         client_,
                         nv::ipc::EventId         event_id_,
                         nv::ipc::QueueId         queue_id_,
                         Port                     port_id_,
                         uint8_t                  target_addr_,
                         nv::ipc::BootedEventBits boot_event_,
                         nv::ipc::TimerId         timer_id_        = nv::ipc::TimerId::End,
                         nv::ipchandler::Id       ipchandler_id_   = nv::ipchandler::Id::Unuse,
                         nv::ipc::TimerId repeated_start_timer_id_ = nv::ipc::TimerId::End)
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
        , repeated_start_timer_id(repeated_start_timer_id_)
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

    static constexpr auto CtrlWaitEvents = Event::CtrlDone | Event::CtrlBusBusy | Event::CtrlNak
                                         | Event::CtrlArbLost | Event::CtrlError;

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
        I2cRecovery,
        CheckApStatus,
        I2cLoopbackTest,
        EepromUpdate,
        CheckTargetTimeout
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
    static void         refresh_smbus_cache(nv::ipc::Timer& timer);
    static void         eeprom_update(nv::ipc::Timer& timer);
    static Task::Status wdt_notify(nv::watchdog::TaskMonitorIndex taskId);
    static Task::Status send_recovery_request(RecoveryCmd cmd, mctp::Client client);
    static void         check_ap_status(nv::ipc::Timer& timer);
    static void         check_target_timeout(nv::ipc::Timer& timer);
    static void         repeated_start_timeout(nv::ipc::Timer& timer);
    static void         request_i2c_loopback_test();
    static void         stop_polling_timers();
    static bool         to_i2c(ipchandler::Id    src_id,
                               uint8_t           address,
                               ipchandler::Id    i2c_ipchandler_id,
                               uint16_t          write_length,
                               uint16_t          read_length,
                               I2cFlags          flags,
                               ipc::Queue::Item& item);

    static bool to_i2c_from_isr(ipchandler::Id    src_id,
                                uint8_t           address,
                                ipchandler::Id    i2c_ipchandler_id,
                                uint8_t           write_length,
                                uint16_t          read_length,
                                ipc::Queue::Item& item);

    Task(Config config) noexcept;
    [[noreturn]] void                 start();
    void                              rx(std::span<uint8_t> buffer);
    void                              rx_error();
    Task::Status                      set_event(Event event);
    EepromCache<nv::ipc::EepromSize>& eeprom_cache()
    {
        NV_ASSERT(_cache.has_value());
        return _cache.value().get();
    }
    void set_cache(EepromCache<nv::ipc::EepromSize>& cache) { _cache = cache; }

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

    RoutingTable                        _routing_table{};
    nv::mctp::Client                    _client;
    nv::ipc::Event&                     _event;
    nv::ipc::Queue&                     _queue;
    Port                                _port;
    sys::i2c::Driver                    _driver;
    std::array<uint8_t, UINT8_MAX + 1U> _eid_addr_map;
    uint8_t                             _target_addr;
    nv::ipc::BootedEventBits            _boot_event;
    nv::ipc::Timer                      _timer;
    nv::ipc::Timer                      _repeated_start_timer;
    nv::ipc::Timer                      _target_timeout_timer;
    nv::ipchandler::Id                  _ipchandler_id;
    APStatus                            _ap_status{APStatus::Querying};

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
    bool handle_eeprom_cache_request(const I2cRequest&  request,
                                     std::span<uint8_t> read_buffer,
                                     size_t             write_len,
                                     size_t             read_len);
    void handle_i2c_response(std::span<uint8_t> buffer);
    void handle_i2c_recovery(std::span<uint8_t> buffer);
    void handle_i2c_loopback_test();

    I2cStatus i2c_read_recvlen_wrapper(uint8_t            address,
                                       std::span<uint8_t> read_buffer,
                                       I2cFlags           flags,
                                       size_t&            read_len);
    I2cStatus i2c_cmd(uint8_t            address,
                      std::span<uint8_t> write_buffer,
                      std::span<uint8_t> read_buffer,
                      I2cFlags           flags,
                      size_t&            read_len);
    I2cStatus wait_for_i2c_completion();
    void      handle_ap_status(APStatus set_status);
    void      handle_check_target_timeout();
    void      handle_eeprom_update();

    // OOB Bus error telemetry
    nv::perf_mon::OobBus                                                    _oob_bus;
    std::optional<std::reference_wrapper<EepromCache<nv::ipc::EepromSize>>> _cache;
};

template<uint16_t CacheSize>
struct TaskWithCache
{
    EepromCache<CacheSize> cache;
    Task                   task;

    explicit TaskWithCache(Task::Config config) : cache(), task(config)
    {
        cache.init();
        if constexpr (CacheSize == nv::ipc::EepromSize) {
            task.set_cache(cache);
        }
    }
};

// Template function to create an I2C task for a specific port
template<Port PortId>
static void make_task_by_port(Task::Config config)
{
    constexpr auto i2c_task_stack_size = nv::lstp::EnableI2c ? 640 : 480;
    constexpr auto StackSize = std::max(i2c_task_stack_size, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    if constexpr (nv::ipc::EnableEepromBridge) {
        constexpr uint16_t CacheSize = (PortId == nv::ipc::EepromDstPort) ? nv::ipc::EepromSize
                                                                          : MinEepromCacheSize;
        NV_TASK_DATA static TaskWithCache<CacheSize> data(config);
        // NOLINTNEXTLINE(*-reinterpret-cast)
        const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&data), sizeof(data));
        data.task.setup(stack.span(), priv, Task::Priority::I2c, Task::entrypoint);
    }
    else {
        NV_TASK_DATA static Task task(config);
        // NOLINTNEXTLINE(*-reinterpret-cast)
        const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&task), sizeof(task));
        task.setup(stack.span(), priv, Task::Priority::I2c, Task::entrypoint);
    }
}

/// Map MCTP I2C client to the IPC queue for that downstream / US I2C task.
constexpr inline nv::ipc::QueueId client_to_i2c_queue_id(nv::mctp::Client client)
{
    using namespace nv::ipc;
    switch (client) {
        case nv::mctp::Client::UsI2c : return QueueId::I2c0;
        case nv::mctp::Client::DsI2c0: return QueueId::I2c1;
        case nv::mctp::Client::DsI2c1: return QueueId::I2c2;
        case nv::mctp::Client::DsI2c2: return QueueId::I2c3;
        case nv::mctp::Client::DsI2c3: return QueueId::I2c4;
        case nv::mctp::Client::DsI2c4: return QueueId::I2c5;
        case nv::mctp::Client::DsI2c5: return QueueId::I2c6;
        case nv::mctp::Client::DsI2c6: return QueueId::I2c7;
        case nv::mctp::Client::DsI2c7: return QueueId::I2c8;
        default                      : return QueueId::End;
    }
}

}  // namespace nv::i2c
