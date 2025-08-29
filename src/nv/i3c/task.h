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
#include <array>
#include <type_traits>

#include "nv/i3c/driver.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/ipchandler/ipchandler.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/router.h"
#include "nv/telemetry/cache.h"
#include "nv/gpio/common.h"
#include "nv/i3c/gpu.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/i3c/smbpbi.h"

namespace nv::i3c {

class Task : public nv::ipc::Task
{
public:
    enum class Status : uint8_t
    {
        Ok,
        InvalidArgument,
        FailToEnumerate,
        InvalidInterface,
        FailToPutTxPacket,
        FailToSetEvent,
        FailToPutRequest,
        Nack
    };

    enum class RequestType : uint8_t
    {
        MctpIbi,
        GpuSmbpbiIbi,
        Transmit,
        UpdateRoutingTable,
        I2cRequest,
        I2cResponse,
        UpdateSensor,
        WdtEvent,
        Init,
        CheckApStatus,
        OcpAddr
    };

    enum class GpuOobResetStatus : uint8_t
    {
        Enable,
        AlreadyEnable,
        FailToQuery,
        FailToEnable,
        FailToAssignAddr,
        GpuNotInI3cMode,
        FailToResetAddr,
        FailToSetInt
    };

    enum class APStatus : uint8_t
    {
        Querying,
        ApDown,
        ApUpNotEnumerated,
        ApUpEnumerated
    };

    enum class SensorState : uint8_t
    {
        Idle,
        Request,
    };

    struct sensor_info
    {
        uint8_t                offset;
        nv::telemetry::TelemId temp_item;
    };

    struct gpio_info
    {
        nv::gpio::GpioPort port;
        nv::gpio::GpioPin  pin;
    };

    struct Config
    {
        nv::ipc::TaskId          task_id;
        std::string_view         task_name;
        nv::mctp::Client         client;
        nv::ipc::EventId         event_id;
        nv::ipc::QueueId         queue_id;
        Driver::Port             port_id;
        nv::ipc::BootedEventBits boot_event;
        nv::i3c::Driver::Freq    freq;
        bool                     is_gpu;
        uint8_t                  gpu_recovery_addr;
        uint8_t                  gpu_smbpbi_addr;
        nv::ipc::TimerId         timer_id;
        sensor_info              temp_sensor;
        gpio_info                gpu_error;
        nv::ipchandler::Id       ipchandler_id;
        Config(nv::ipc::TaskId          task_id_,
               const std::string_view&  task_name_,
               nv::mctp::Client         client_,
               nv::ipc::EventId         event_id_,
               nv::ipc::QueueId         queue_id_,
               Driver::Port             port_id_,
               nv::ipc::BootedEventBits boot_event_,
               nv::i3c::Driver::Freq    freq_,
               bool                     is_gpu_,
               uint8_t                  gpu_recovery_addr_,
               uint8_t                  gpu_smbpbi_addr_,
               nv::ipc::TimerId         timer_id_,
               sensor_info              temp_sensor_,
               gpio_info                gpu_error_,
               nv::ipchandler::Id       ipchandler_id_ = nv::ipchandler::Id::Unuse)
        : task_id(task_id_)
        , task_name(task_name_)
        , client(client_)
        , event_id(event_id_)
        , queue_id(queue_id_)
        , port_id(port_id_)
        , boot_event(boot_event_)
        , freq(freq_)
        , is_gpu(is_gpu_)
        , gpu_recovery_addr(gpu_recovery_addr_)
        , gpu_smbpbi_addr(gpu_smbpbi_addr_)
        , timer_id(timer_id_)
        , temp_sensor(temp_sensor_)
        , gpu_error(gpu_error_)
        , ipchandler_id(ipchandler_id_)
        {}
    };

    struct [[gnu::packed]] Request
    {
        RequestType       type;
        uint8_t           length;
        Driver::I3cBuffer buffer;
    };

    static void   make(Config config);
    static void   entrypoint(void* params);
    static Status tx(const nv::mctp::Packet& packet);
    static Status update_routing_table(nv::mctp::Client client);
    static Status wdt_notify(nv::watchdog::TaskMonitorIndex taskId);
    static void   update_sensor(nv::ipc::Timer& timer);
    static Status init_bus(nv::mctp::Client client, bool init);
    static void   check_ap_status(nv::ipc::Timer& timer);
    static Status update_ocp_address(nv::mctp::Client client, nv::i3c::Gpu::I2cAddr addr);

    template<nv::ipchandler::Id EnumVal>
    static constexpr nv::ipc::QueueId GetQueueFromIpchandlerId()
    {
        static_assert(EnumVal > nv::ipchandler::Id::I3cStart
                          && EnumVal < nv::ipchandler::Id::I3cEnd,
                      "The Input ipchandler id is not valid");

        for (const auto& I2cAddrMappingItem : nv::ipc::I2cVirtualAddressMappingTable) {
            if (I2cAddrMappingItem.ipchandler_id == EnumVal) {
                return I2cAddrMappingItem.queue_id;
            }
        }
        // not found any match
        return nv::ipc::QueueId::I3c0;
    }

    Task(Config config) noexcept;
    void        start();
    void        on_ibi(uint8_t address, std::span<uint8_t> data);
    static bool to_i2c(ipchandler::Id     src_id,
                       uint8_t            address,
                       nv::ipchandler::Id i2c_ipchandler_id,
                       uint8_t            write_length,
                       uint16_t           read_length,
                       ipc::Queue::Item&  item);

    void record_error(uint8_t error_type) const;

private:
    using RoutingTable = std::array<mctp::ShardRoutingTable, ipc::RoutingTableSize>;

    nv::mctp::Client _client;
    nv::ipc::Queue&  _queue;
    Driver           _driver;
    // NOLINTNEXTLINE the I3C address is number
    std::array<uint8_t, 1> _address_pool{
        0x31,
    };
    RoutingTable             _routing_table{};
    nv::ipc::BootedEventBits _boot_event;
    // GPU specific variable
    bool               _is_gpu;
    uint8_t            _gpu_recovery_addr;
    uint8_t            _gpu_smbpbi_addr;
    nv::ipc::Timer     _timer;
    SensorState        _sensor_state;
    sensor_info        _temp_sensor;
    gpio_info          _gpu_error;
    nv::ipchandler::Id _ipchandler_id;
    APStatus           _ap_status = APStatus::Querying;

    void    handle_mctp_ibi(std::span<uint8_t> buffer);
    void    handle_gpu_ibi(std::span<uint8_t> buffer);
    void    handle_wdt_event();
    void    handle_rx(std::span<uint8_t> buffer);
    void    handle_tx(std::span<uint8_t> buffer);
    void    handle_update_routing_table();
    void    handle_i2c_request(std::span<uint8_t> buffer);
    void    handle_i2c_response(std::span<uint8_t> buffer);
    void    handle_update_sensor(std::span<uint8_t> buffer);
    void    handle_init(std::span<uint8_t> buffer);
    void    handle_ocp_addr(std::span<uint8_t> buffer);
    void    forward(nv::mctp::Packet& packet);
    void    transmit(std::span<uint8_t> packet);
    bool    handle_gpu_reset(uint8_t ocp_addr, uint8_t therm_addr);
    bool    handle_cxx_reset(bool ping);
    void    read_gpu_sensor(uint8_t reg);
    uint8_t get_smbpbi_command_index();
    void    update_smbpbi_command_index();
    void    update_smbpbi_items(uint32_t value);

    constexpr static uint32_t ThresholdTick = 10000;  // 10 ms
    void                      handle_ap_status(APStatus set_status);

    // OOB Bus error telemetry
    nv::perf_mon::OobBus _oob_bus;

    // smbpbi
    smbpbi::State            smbpbi_state{};
    uint8_t                  smbpbi_command_index         = 0;
    uint8_t                  smbpbi_not_ready_count       = 0;
    constexpr static uint8_t SmbpbiNotReadyCountThreshold = 3;
    std::array<nv::telemetry::TelemId, smbpbi::NumSmbPbiCommandToRead> _smbpbi_items{};
};

}  // namespace nv::i3c
