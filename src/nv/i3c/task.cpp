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
#include "nv/i3c/task.h"

#include <algorithm>
#include <bit>
#include <cstring>

#include "nv/bootloader.h"
#include "nv/gpio/driver.h"
#include "nv/i2c/helper.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"
#include "nv/usb/task.h"
#include "nv/gpio/driver.h"
#include "nv/i2c/task.h"
#include "nv/watchdog/runtime.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/ctimer/ctimer.h"
#include "nv/flash/flash.h"
#include "nv/i3c/topology_info.h"
#include "sys/i2c/utils.h"

using namespace nv::i3c;

uint8_t __attribute__((weak))
nv::i3c::initial_endpoint_status([[maybe_unused]] nv::mctp::Client client)
{
    return 1;
}

void Task::make(Config config)
{
    constexpr auto StackSize = std::max(1024, int(configMINIMAL_STACK_SIZE));
    switch (config.port_id) {
        case Driver::Port::Zero: {
            NV_TASK_DATA static Task                       task0(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack0;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv0(reinterpret_cast<uint8_t*>(&task0), sizeof(Task));
            task0.setup(stack0.span(), Priv0, Priority::I2c, Task::entrypoint);
            break;
        }
        case Driver::Port::One: {
            NV_TASK_DATA static Task                       task1(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack1;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv1(reinterpret_cast<uint8_t*>(&task1), sizeof(Task));
            task1.setup(stack1.span(), Priv1, Priority::I2c, Task::entrypoint);
            break;
        }
        default: nv::info("I3C port %d does not support\n", config.port_id);
    }
}

void nv::i3c::Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

nv::i3c::Task::Status nv::i3c::Task::tx(const nv::mctp::Packet& packet)
{
    using namespace nv::ipc;
    using namespace nv::mctp;
    using namespace std::chrono_literals;
    // coverity[assigned_value] need to assign a value to pass lint
    QueueId queue_id = QueueId::End;
    switch (packet.priv.packet_interface) {
        case static_cast<uint8_t>(Client::DsI3c0): queue_id = QueueId::I3c0; break;
        case static_cast<uint8_t>(Client::DsI3c1): queue_id = QueueId::I3c1; break;
        default                                  : return Status::InvalidInterface;
    }
    Request request{.type = RequestType::Transmit};
    auto    length = packet.priv.packet_length;
    if (length > request.buffer.size()) {
        return Task::Status::InvalidArgument;
    }
    request.length = static_cast<uint16_t>(length);
    /// prepare I3C request
    auto mctp_raw = packet.to_span().subspan(sizeof(packet.priv));
    std::copy(mctp_raw.begin(), mctp_raw.end(), request.buffer.begin());
    /// send to I3C task
    auto   item     = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    Queue& queue    = Queue::make(queue_id);
    bool   need_log = true;
    while (queue.size() >= I3cQueueMaxTxSize) {
        if (need_log) {
            need_log = false;
            nv::logger::info(nv::logger::Event::I3CTxOverThreshold,
                             {static_cast<uint8_t>(queue_id)});
        }
        auto& task = Supervisor::inst().current_task();
        task.delay(5ms);
    }

    auto queue_status = queue.send(item);
    if (queue_status != Queue::Status::Ok) {
        nv::info("fail to put tx packet (%d)\n", queue_status);
        return Task::Status::FailToPutTxPacket;
    }
    return Task::Status::Ok;
}

nv::i3c::Task::Status nv::i3c::Task::update_routing_table(nv::mctp::Client client)
{
    using namespace nv::ipc;
    using namespace nv::mctp;
    using namespace std::chrono_literals;
    // coverity[assigned_value] need to assign a value to pass lint
    QueueId queue_id = QueueId::End;
    switch (client) {
        case Client::DsI3c0: queue_id = QueueId::I3c0; break;
        case Client::DsI3c1: queue_id = QueueId::I3c1; break;
        default            : return Status::InvalidInterface;
    }
    /// prepare I3C request
    const Request RequestPkt{.type = RequestType::UpdateRoutingTable};
    /// send to I3C task
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&RequestPkt), sizeof(RequestPkt));
    auto queue_status = Queue::make(queue_id).send(item, 10ms);
    if (queue_status != Queue::Status::Ok) {
        nv::info("fail to put tx packet (%d)\n", queue_status);
        return Task::Status::FailToPutTxPacket;
    }
    return Task::Status::Ok;
}

nv::i3c::Task::Status nv::i3c::Task::wdt_notify(watchdog::TaskMonitorIndex taskId)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    // coverity[assigned_value] need to assign a value to pass lint
    QueueId queue_id = QueueId::End;
    switch (taskId) {
        case watchdog::TaskMonitorIndex::I3c0: queue_id = QueueId::I3c0; break;
        case watchdog::TaskMonitorIndex::I3c1: queue_id = QueueId::I3c1; break;
        default                              : return Status::InvalidInterface;
    }
    /// prepare I3C request
    const Request RequestPkt{.type = RequestType::WdtEvent};
    /// send to I3C task
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&RequestPkt), sizeof(RequestPkt));
    auto queue_status = Queue::make(queue_id).send(item, 0s);
    if (queue_status != Queue::Status::Ok) {
        nv::info("fail to put tx packet (%d)\n", queue_status);
        logger::error_no_wait(
            logger::Event::I3CQueueDropV2,
            {static_cast<uint8_t>(queue_id), static_cast<uint8_t>(queue_status)});
        return Task::Status::FailToPutTxPacket;
    }
    return Task::Status::Ok;
}

void Task::update_sensor(nv::ipc::Timer& timer)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    QueueId       queue_id = QueueId::End;
    const Request request{.type = RequestType::UpdateSensor};
    switch (timer.id()) {
        case TimerId::Gpu1Seneor: queue_id = QueueId::I3c0; break;
        case TimerId::Gpu2Seneor: queue_id = QueueId::I3c1; break;
        default                 : return;
    }
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = Queue::make(queue_id).send(item, 0s);
    if (queue_status != Queue::Status::Ok) {
        /// drop event because task is busy
    }
}

void Task::check_ap_status(nv::ipc::Timer& timer)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    QueueId       queue_id = QueueId::End;
    const Request request{.type = RequestType::CheckApStatus};
    switch (timer.id()) {
        case TimerId::Ap1Status: queue_id = QueueId::I3c0; break;
        case TimerId::Ap2Status: queue_id = QueueId::I3c1; break;
        default                : return;
    }
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = Queue::make(queue_id).send(item, 0s);
    if (queue_status != Queue::Status::Ok) {
        /// drop event because task is busy
    }
}

nv::i3c::Task::Status nv::i3c::Task::init_bus(nv::mctp::Client client, bool init)
{
    using namespace nv::ipc;
    using namespace nv::mctp;
    using namespace std::chrono_literals;
    // coverity[assigned_value] need to assign a value to pass lint
    QueueId queue_id = QueueId::End;
    switch (client) {
        case Client::DsI3c0: queue_id = QueueId::I3c0; break;
        case Client::DsI3c1: queue_id = QueueId::I3c1; break;
        default            : return Status::InvalidInterface;
    }
    /// prepare I3C request
    const Request RequestPkt{
        .type = RequestType::Init, .length = 1, .buffer = {static_cast<uint8_t>(init ? 1 : 0)}};
    /// send to I3C task
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&RequestPkt), sizeof(RequestPkt));
    auto queue_status = Queue::make(queue_id).send(item, 10ms);
    if (queue_status != Queue::Status::Ok) {
        return Task::Status::FailToPutRequest;
    }
    return Task::Status::Ok;
}

nv::i3c::Task::Status nv::i3c::Task::update_ocp_address(nv::mctp::Client      client,
                                                        nv::i3c::Gpu::I2cAddr addr)
{
    using namespace nv::ipc;
    using namespace nv::mctp;
    // coverity[assigned_value] need to assign a value to pass lint
    QueueId queue_id = QueueId::End;
    switch (client) {
        case Client::DsI3c0: queue_id = QueueId::I3c0; break;
        case Client::DsI3c1: queue_id = QueueId::I3c1; break;
        default            : return Status::InvalidInterface;
    }
    /// prepare I3C request
    Request request_pkt{.type = RequestType::OcpAddr, .length = sizeof(addr)};
    std::memcpy(request_pkt.buffer.data(), &addr, sizeof(addr));
    /// send to I3C task
    auto item = Queue::Item(std::bit_cast<uint8_t*>(&request_pkt), sizeof(request_pkt));
    Queue::Status queue_status{};
    if (Supervisor::is_in_isr()) {
        queue_status = Queue::make(queue_id).send_isr(item);
    }
    else {
        queue_status = Queue::make(queue_id).send(item);
    }
    if (queue_status != Queue::Status::Ok) {
        return Task::Status::FailToPutRequest;
    }
    return Task::Status::Ok;
}

nv::i3c::Task::Status nv::i3c::Task::sync_cbc_sn(nv::mctp::Client client)
{
    using namespace nv::ipc;
    using namespace nv::mctp;
    // coverity[assigned_value] need to assign a value to pass lint
    QueueId queue_id = QueueId::End;
    switch (client) {
        case Client::DsI3c0: queue_id = QueueId::I3c0; break;
        case Client::DsI3c1: queue_id = QueueId::I3c1; break;
        default            : return Status::InvalidInterface;
    }
    /// prepare I3C request
    const Request RequestPkt{.type = RequestType::SyncCBCSn};
    /// send to I3C task
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&RequestPkt), sizeof(RequestPkt));
    auto queue_status = Queue::make(queue_id).send_isr(item);
    if (queue_status != Queue::Status::Ok) {
        return Task::Status::FailToPutTxPacket;
    }
    return Task::Status::Ok;
}

Task::Task(Config config) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, _client(config.client)
, _queue(nv::ipc::Queue::make(config.queue_id))
, _driver(config.port_id, config.freq, config.is_gpu, this, config.event_id, config.clock)
, _boot_event(config.boot_event)
, _is_gpu(config.is_gpu)
, _gpu_recovery_addr(config.gpu_recovery_addr)
, _gpu_smbpbi_addr(config.gpu_smbpbi_addr)
, _sensor_state(SensorState::Idle)
, _temp_sensor(config.temp_sensor)
, _gpu_error(config.gpu_error)
, _ipchandler_id(config.ipchandler_id)
, _oob_bus(nv::perf_mon::OobBus::End)
, _fru_i2c_info(config.fru_i2c_info)
, _platform_info(config.platform_info)
{
    nv::info("%s bind to I3C port %d\n", config.task_name.data(), config.port_id);
    logger::info(logger::Event::I3CBind,
                 {static_cast<uint8_t>(config.task_id), static_cast<uint8_t>(config.port_id)});

    switch (config.client) {
        case mctp::Client::DsI3c0: {
            _oob_bus      = nv::perf_mon::OobBus::DsI3c0;
            _smbpbi_items = {telemetry::TelemId::Gpu1Temp, telemetry::TelemId::Gpu1Power};
            break;
        }
        case mctp::Client::DsI3c1: {
            _oob_bus      = nv::perf_mon::OobBus::DsI3c1;
            _smbpbi_items = {telemetry::TelemId::Gpu2Temp, telemetry::TelemId::Gpu2Power};
            break;
        }
        default: break;
    }

    nv::perf_mon::Driver::set_oob_bus_valid(_oob_bus);

    if (_is_gpu) {
        nv::logger::info(nv::logger::Event::NvlInfo,
                         {static_cast<uint8_t>(_client),
                          _platform_info.node_index,
                          _platform_info.module_id});
        if (ipc::I3cSensorUpdateMs > 0) {
            _timer = ipc::Timer::make(config.timer_id,
                                      std::chrono::microseconds(ipc::I3cSensorUpdateMs),
                                      update_sensor);
        }
    }
    else {
        if (!ipc::UseI2cApStatusTimer && config.timer_id != ipc::TimerId::End) {
            _timer = ipc::Timer::make(config.timer_id,
                                      std::chrono::microseconds(ipc::CheckApStatusTimerUs),
                                      check_ap_status);
        }
    }
}

void Task::start()
{
    using namespace nv::ipc;
    Request request{};
    auto    item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    nv::bootloader::Driver::set_task_booted(_boot_event);
    std::array<uint8_t, 1> init = {static_cast<uint8_t>(true)};
    if (_is_gpu && _gpu_recovery_addr != 0) {
        auto item = _client == mctp::Client::DsI3c0
                      ? nv::flash::Key::NpdsI2cDynamicAddrForOcpDevice0
                      : nv::flash::Key::NpdsI2cDynamicAddrForOcpDevice1;
        (void)nv::flash::Flash::set_data(item, _gpu_recovery_addr);
    }
    // In GPU case, MCU monitor IROT ERROR signal
    if (!_is_gpu && initial_endpoint_status(_client) != 0U) {
        _driver.init();
        handle_init(init);
    }

    while (true) {
        auto status = _queue.recv(item);
        if (status != Queue::Status::Ok) {
            nv::info("fail to get request from queue (%d)\n", status);
            continue;
        }
        if (request.length > request.buffer.size()) {
            continue;
        }
        auto data = std::span<uint8_t>(request.buffer.data(), request.length);
        switch (request.type) {
            case RequestType::MctpIbi     : handle_mctp_ibi(data); break;
            case RequestType::GpuSmbpbiIbi: handle_gpu_ibi(data); break;
            case RequestType::WdtEvent    : handle_wdt_event(); break;
            case RequestType::Transmit:
                perf_mon::Driver::log_pkt_latency(
                    perf_mon::Driver::LatencyEvent::I3cTaskRecvTxPkt);
                handle_tx(data);
                break;
            case RequestType::UpdateRoutingTable: handle_update_routing_table(); break;
            case RequestType::I2cRequest        : handle_i2c_request(data); break;
            case RequestType::I2cResponse       : handle_i2c_response(data); break;
            case RequestType::UpdateSensor      : handle_update_sensor(data); break;
            case RequestType::Init              : handle_init(data); break;
            case RequestType::CheckApStatus     : handle_ap_status(APStatus::Querying); break;
            case RequestType::OcpAddr           : handle_ocp_addr(data); break;
            case RequestType::SyncCBCSn         : handle_sync_cbc_sn(data); break;
            default                             : break;
        }
    }
}

void Task::on_ibi(uint8_t address, std::span<volatile uint8_t> data)
{
    using namespace nv::ipc;
    constexpr uint8_t PendingReadId  = 0xAEU;
    constexpr uint8_t GpuSmbDirectId = 0x00;
    Request           request{};
    // dispatch interrupt
    if (data.size() == 1 && data[0] == PendingReadId) {
        perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::I3cTaskRecvRxIsr);
        request.type      = RequestType::MctpIbi;
        request.length    = 1;
        request.buffer[0] = address;
    }
    else if (data.size() == 5 && data[0] == GpuSmbDirectId) {
        request.type   = RequestType::GpuSmbpbiIbi;
        request.length = 5;
        // device address, byet 0, byet 1, byet 2, byet 3
        request.buffer[0] = address;
        std::copy(data.begin() + 1, data.end(), request.buffer.begin() + 1);
    }
    else {
        // nv::info("ibi does not support\n");
        constexpr uint8_t ByteMask = 0xFF;
        logger::Logger::add_from_isr(logger::Event::I3CUnknownIBIV2.unique_id,
                                     logger::Level::Info,
                                     {static_cast<uint8_t>(_driver.port()),
                                      static_cast<uint8_t>(data.size() & ByteMask),
                                      data[0]});
        return;
    }
    auto item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));

    if (request.type == RequestType::MctpIbi) {
        auto result = _queue.send_front_isr(item);
        if (result != Queue::Status::Ok) {
            logger::Logger::add_from_isr(
                logger::Event::I3CIbiDropV2.unique_id,
                logger::Level::Error,
                {static_cast<uint8_t>(_queue.id()), static_cast<uint8_t>(result)});
        }
    }
    else {
        auto result = _queue.send_isr(item);
        if (result != Queue::Status::Ok) {
            // Discard the packet
        }
    }
    // reset status timer since AP proved connectivity with IBI
    if (_timer.id() == ipc::TimerId::Ap1Status || _timer.id() == ipc::TimerId::Ap2Status) {
        _timer.reset();
    }
}

void Task::handle_mctp_ibi(std::span<uint8_t> buffer)
{
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::I3cTaskReadRxPkt);
    if (buffer.size() == 0) {
        return;
    }
    uint8_t           length = 0;
    Driver::I3cBuffer data{};
    data[0] = static_cast<unsigned char>(((buffer[0] << 1U) | 1U) & UINT8_MAX);  // 8 bit
                                                                                 // address
    auto result = _driver.read(
        data[0] >> 1, std::span<uint8_t>(data.data() + 1, data.size() - 1), length);

    if (result) {
        handle_rx(std::span<uint8_t>(data.data(), length + 1));
    }
    nv::perf_mon::Driver::log_pkt_meas(static_cast<uint32_t>(_client), length, true, !result);
}

void Task::handle_gpu_ibi(std::span<uint8_t> buffer)
{
    using namespace std;
    using namespace nv::i2c;
    using namespace std::chrono_literals;
    if (smbpbi_state == smbpbi::State::ReadStatus) {
        bool ready = false;
        if ((buffer[smbpbi::StatusOffset] & smbpbi::StatusMask)
            == static_cast<uint8_t>(smbpbi::StatusCode::Success)) {
            const bool success = nv::i3c::smbpbi::query_smbpbi_data(_driver,
                                                                    _address_pool.at(0) << 1);
            if (success) {
                smbpbi_state = smbpbi::State::ReadData;
                ready        = true;
            }
        }

        if (!ready) {
            smbpbi_not_ready_count++;
            if (smbpbi_not_ready_count >= SmbpbiNotReadyCountThreshold) {
                update_smbpbi_items(nv::telemetry::Cache::InvalidItem);
            }
            else {
                const bool success = nv::i3c::smbpbi::query_smbpbi_status(
                    _driver, _address_pool.at(0) << 1);
                if (!success) {
                    update_smbpbi_items(nv::telemetry::Cache::InvalidItem);
                }
            }
        }
    }
    else if (smbpbi_state == smbpbi::State::ReadData) {
        using namespace nv::telemetry;
        const uint32_t ItemValue = buffer_to_uint32(buffer.subspan(1, 4));
        update_smbpbi_items(ItemValue);
    }
    else {
        return;
    }
}

void Task::handle_wdt_event()
{
    watchdog::TaskMonitorIndex index{};

    switch (_client) {
        case mctp::Client::DsI3c0: index = watchdog::TaskMonitorIndex::I3c0; break;
        case mctp::Client::DsI3c1: index = watchdog::TaskMonitorIndex::I3c1; break;
        default                  : return;
    }

    watchdog::Runtime::mark_task_alive(index);
}
void Task::handle_rx(std::span<uint8_t> buffer)
{
    constexpr size_t  BytesAddrPec = 2;
    constexpr uint8_t IgnorePec    = 0xFF;
    constexpr uint8_t MinSize      = BytesAddrPec + sizeof(nv::mctp::Packet::hdr);
    constexpr uint8_t MaxSize      = MinSize + sizeof(nv::mctp::Packet::msg);
    if (buffer.size() < MinSize || buffer.size() > MaxSize) {
        return;
    }
#if 0
    for (const auto& element : buffer) {
        nv::info("RX 0x%02x\n", element);
    }
#endif
    /// check PEC
    auto pec = nv::i2c::crc8(buffer.subspan(0, buffer.size() - 1));
    if (buffer.back() != pec && buffer.back() != IgnorePec) {
        nv::logger::info(nv::logger::Event::I3CPecInvalidV2,
                         {static_cast<uint8_t>(_driver.port()),
                          buffer.back(),
                          pec,
                          static_cast<uint8_t>(buffer.size() & UINT8_MAX),
                          buffer[0],
                          buffer[1],
                          buffer[2],
                          buffer[3]});
        // nv::info("PEC 0x%02x is invalid, drop RX\n", buffer.back());
        return;
    }
    /// create internal MCTP packet
    nv::mctp::Packet packet{};
    // coverity[cert_int31_c_violation] already check the data size
    packet.priv.packet_length = buffer.size() - BytesAddrPec;
    // coverity[cert_int31_c_violation] number of client < 256
    packet.priv.packet_interface = nv::common::to_underlying(_client);
    /// remove the target address and PEC
    std::memcpy(static_cast<void*>(&packet.hdr),
                static_cast<void*>(&buffer[1]),
                buffer.size() - BytesAddrPec);
    forward(packet);
}

void Task::handle_tx(std::span<uint8_t> buffer)
{
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::I3cTaskHandleTx);
    constexpr size_t   BytesAddrPec = 2;
    Driver::I3cBuffer  raw{0};
    constexpr uint8_t  MinSize = sizeof(nv::mctp::Packet::hdr);
    constexpr uint16_t MaxSize = raw.size() - BytesAddrPec;
    if (buffer.size() < MinSize || buffer.size() > MaxSize) {
        return;
    }
    auto data = std::span<uint8_t>(raw.data(), buffer.size() + BytesAddrPec);
    /// add the target address
    // coverity[cert_int34_c_violation] 7-bit address is safe to left shift 1
    // coverity[cert_int31_c_violation] 8-bit address is never larger than UINT8_MAX
    data.front() = static_cast<uint8_t>(_address_pool.front() << 1U) | 0U;
    /// add payload
    std::copy(buffer.begin(), buffer.end(), &data[1]);
    /// add PEC
    data.back() = nv::i2c::crc8(data.subspan(0, buffer.size() + 1));
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::I3cTaskHandleTxDone);
    transmit(data);
#if 0
    for (const auto& element : data) {
        nv::info("TX 0x%02x\n", element);
    }
#endif
}

void Task::handle_update_routing_table()
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    Queue::Item item(std::bit_cast<uint8_t*>(&_routing_table), sizeof(_routing_table));
    auto        router_queue = Queue::make(QueueId::RoutingTable);
    auto        queue_status = router_queue.recv(item, 100ms);
    if (queue_status != Queue::Status::Ok) {
        nv::info("fail to update routing table\n");
    }
    else if (ipc::CheckApStatusTimerUs) {
        // routing table events come from upstream enumueration, update status if checking for
        // AP status; update status if I3C is not running the timer or if AP is up and not
        // enumerated
        if ((_timer.id() != ipc::TimerId::Ap1Status && _timer.id() != ipc::TimerId::Ap2Status)
            || _ap_status == APStatus::ApUpNotEnumerated || _ap_status == APStatus::Querying) {
            auto  myep    = nv::mctp::Control::get_routing_index(_client);
            auto& myentry = _routing_table.at(myep);
            if (myentry.is_enumerated) {
                // Set EID success
                handle_ap_status(APStatus::ApUpEnumerated);
            }
            else {
                // Set EID fail
                handle_ap_status(APStatus::ApDown);
            }
        }
    }
}

void Task::handle_i2c_request(std::span<uint8_t> buffer)
{
    nv::i2c::I2cHidBuffer read_buffer = {0};
    nv::i2c::I2cRequest   request     = {0};
    const size_t          RequestSize = std::min(sizeof(nv::i2c::I2cRequest), buffer.size());
    std::memcpy(&request, buffer.data(), RequestSize);
    const size_t WriteLength  = std::min(static_cast<size_t>(request.write_length),
                                        request.write_buffer.size());
    const size_t ReadLength   = std::min(static_cast<size_t>(request.read_length),
                                       read_buffer.size());
    auto write_buffer_span    = std::span<uint8_t>(request.write_buffer.data(), WriteLength);
    auto read_buffer_span     = std::span<uint8_t>(read_buffer.data(), ReadLength);
    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Ok;
    if (request.address == _fru_i2c_info.eeprom_addr) {
        if (write_buffer_span.size() > 0 && read_buffer_span.size() > 0) {
            result = sys::i2c::i2c_write_read(
                _fru_i2c_info.port, request.address, write_buffer_span, read_buffer_span);
        }
        else if (write_buffer_span.size() > 0) {
            result = sys::i2c::i2c_write(
                _fru_i2c_info.port, request.address, write_buffer_span);
        }
        else if (read_buffer_span.size() > 0) {
            result = sys::i2c::i2c_read(_fru_i2c_info.port, request.address, read_buffer_span);
        }
    }
    else {
        result = _driver.i2c(request.address, write_buffer_span, read_buffer_span);
    }
    if (request.src_id == static_cast<uint8_t>(ipchandler::Id::Usb)) {
        std::span<uint8_t> item(read_buffer.data(), read_buffer.size());
        usb::Task::to_usb(
            this->_ipchandler_id, static_cast<uint16_t>(ReadLength), item, result);
    }
}

void Task::handle_i2c_response(std::span<uint8_t> buffer)
{
    [[maybe_unused]] auto response = std::bit_cast<nv::i2c::I2cResponse*>(buffer.data());
}

void Task::handle_update_sensor([[maybe_unused]] std::span<uint8_t> buffer)
{
    using namespace nv::telemetry;
    // do not read temp if gpu is offline
    uint8_t gpu_error = 0;
    if (_gpu_error.port != nv::gpio::InvalidGpioPort) {
        auto gpio_status = nv::gpio::Driver::read(_gpu_error.port, _gpu_error.pin, gpu_error);
        if (gpio_status != nv::gpio::Status::Ok) {
            return;
        }
        if (gpu_error == static_cast<uint8_t>(nv::gpio::GpioState::Low)) {
            return;
        }
    }

    auto command_index = get_smbpbi_command_index();
    auto command = static_cast<smbpbi::CommandCode>(smbpbi::SmbpbiCommands.at(command_index));
    if (smbpbi_state == smbpbi::State::Init) {
        const bool success = nv::i3c::smbpbi::start_smbpbi(
            _driver, _address_pool.at(0) << 1, command);
        if (success) {
            smbpbi_state = smbpbi::State::ReadStatus;
        }
    }
}

void Task::handle_init(std::span<uint8_t> buffer)
{
    using namespace nv;
    const bool init   = static_cast<bool>(buffer[0]);
    bool       result = false;
    logger::info(logger::Event::I3CInit, {static_cast<uint8_t>(init)});
    if (init) {
        // init case
        if (_is_gpu) {
            _driver.init();
            if constexpr (nv::ipc::EnableForwardNvlInfo) {
                // MCU needs to prepare NVL topology info when GPU up
                prepare_nvl_topology_info();
            }
            result = handle_gpu_reset(_gpu_recovery_addr, _gpu_smbpbi_addr);
            if (result) {
                _timer.start();
            }
        }
        else {
            // start status timer to handle init or reset directly
            if (_timer.id() == ipc::TimerId::Ap1Status
                || _timer.id() == ipc::TimerId::Ap2Status) {
                _timer.start();
            }
            else {
                _driver.init();
                handle_cxx_reset(false);
            }
        }
    }
    else {
        // deinit case
        if (_is_gpu) {
            _timer.stop();
            if (!pull_up_status()) {
                _driver.deinit();
            }
        }
        else {
            _driver.deinit();
        }
    }
}

bool Task::pull_up_status()
{
    if constexpr (ipc::I3CPullUpCheck) {
        uint8_t data = 0;
        (void)nv::gpio::Driver::read(ipc::I3CPullUpPort, ipc::I3CPullUpPin, data);
        return data == 0 ? false : true;
    }
    else {
        return true;
    }
}

void Task::handle_ap_status(APStatus set_status)
{
    bool     result = false;
    APStatus status = _ap_status;

    if (set_status == APStatus::Querying) {
        // Query for status
        if (status == APStatus::Querying || status == APStatus::ApDown) {
            if constexpr (ipc::I3CPullUpCheck) {
                if (!pull_up_status()) {
                    _driver.deinit();
                }
                else {
                    _driver.init();
                }
            }
            // if at startup or ping failed on last check, restart daa
            result = handle_cxx_reset((status == APStatus::ApDown));
            status = result ? APStatus::ApUpNotEnumerated : APStatus::ApDown;
        }
        else {
            // if link is up, ping for status
            if (_address_pool.size()) {
                // only support single AP on bus for now
                auto     addr    = _address_pool[0];
                uint16_t dstatus = 0;
                result           = _driver.get_status(addr, dstatus);
                switch (status) {
                    case APStatus::ApUpNotEnumerated:
                    case APStatus::ApDown:
                    default:
                        status = (result ? APStatus::ApUpNotEnumerated : APStatus::ApDown);
                        break;
                    case APStatus::ApUpEnumerated:
                        status = (result ? APStatus::ApUpEnumerated : APStatus::ApDown);
                        break;
                }
            }
        }
    }
    else {
        // Set status
        status = set_status;
        // reset status timer since we just forced a status change
        if (_timer.id() == ipc::TimerId::Ap1Status || _timer.id() == ipc::TimerId::Ap2Status) {
            _timer.reset();
        }
    }

    // update and log on status change
    if (status != _ap_status) {
        auto myep = nv::mctp::Control::get_routing_index(_client);
        logger::info(logger::Event::I3CApStatusUpdate,
                     {static_cast<uint8_t>(myep),
                      static_cast<uint8_t>(_ap_status),
                      static_cast<uint8_t>(status)});

        // update mctp driver with status for enumeration if I3C is running the timer
        if (_timer.id() == ipc::TimerId::Ap1Status || _timer.id() == ipc::TimerId::Ap2Status) {
            if (status == APStatus::ApUpNotEnumerated || status == APStatus::ApDown) {
                nv::mctp::Driver::endpoint_status_change(
                    myep, (status == APStatus::ApUpNotEnumerated));
                // If I2C downstream is enabled, update I2C status as well
                if (ipc::DownStreamNum > 3) {
                    constexpr uint8_t ByteMask = 0xFF;
                    nv::mctp::Driver::endpoint_status_change(
                        (myep + 1) & ByteMask, (status == APStatus::ApUpNotEnumerated));
                }
            }
        }
        _ap_status = status;
    }
}

void Task::handle_ocp_addr(std::span<uint8_t> buffer)
{
    auto addr          = *std::bit_cast<Gpu::I2cAddr*>(buffer.data());
    _gpu_recovery_addr = addr.ocp_recovery;
    _gpu_smbpbi_addr   = addr.temp_address;
    logger::info(logger::Event::I3CGpuI2cAddr,
                 {static_cast<uint8_t>(Gpu::I2cAddrStatus::Ok), _gpu_recovery_addr});
    auto item = _client == mctp::Client::DsI3c0
                  ? nv::flash::Key::NpdsI2cDynamicAddrForOcpDevice0
                  : nv::flash::Key::NpdsI2cDynamicAddrForOcpDevice1;
    (void)nv::flash::Flash::set_data(item, _gpu_recovery_addr);
}

void Task::forward(nv::mctp::Packet& packet)
{
    using namespace nv;
    auto item             = packet.to_span();
    bool is_routed        = false;
    bool is_usi2c_eid_set = false;

    for (auto& entry : _routing_table) {
        if (entry.is_enumerated == true) {
            if (entry.assigned_eid == packet.hdr.dst_eid) {
                if (entry.client == mctp::Client::UsUsb) {
                    perf_mon::Driver::log_pkt_latency(
                        perf_mon::Driver::LatencyEvent::I3cTaskSendToUsb);
                    auto status = usb::Task::usb_tx(item);
                    if (status != usb::Status::Ok) {
                        nv::info("usb_tx fail 0x%x\n", status);
                    }
                    is_routed = true;
                }
                if constexpr (ipc::I2cTransparent) {
                    if (entry.client == mctp::Client::UsI2c) {
                        uint16_t additional_info = 0;
                        auto     ds_index = nv::mctp::Control::get_routing_index(_client);
                        if (ds_index < nv::i2c::TransparentDsIndexCcodes.size()) {
                            additional_info = nv::i2c::TransparentDsIndexCcodes.at(ds_index);
                        }
                        packet.priv.packet_interface = static_cast<uint8_t>(entry.client);
                        auto status                  = i2c::Task::tx(packet, additional_info);
                        if (status != i2c::Task::Status::Ok) {
                            nv::info("i2c_tx fail 0x%x\n", status);
                        }
                        is_routed = true;
                    }
                }
            }
            if constexpr (nv::ipc::I2cTransparent) {
                if (entry.client == mctp::Client::UsI2c) {
                    is_usi2c_eid_set = true;
                }
            }
        }
    }

    // forward unknown EIDs from downstream to upstream i2c
    if constexpr (ipc::I2cTransparent) {
        if (!is_routed && is_usi2c_eid_set) {
            if (std::find(nv::i2c::TransparentSkippedEids.begin(),
                          nv::i2c::TransparentSkippedEids.end(),
                          packet.hdr.dst_eid)
                == nv::i2c::TransparentSkippedEids.end()) {
                uint16_t additional_info = 0;
                auto     ds_index        = nv::mctp::Control::get_routing_index(_client);
                if (ds_index < nv::i2c::TransparentDsIndexCcodes.size()) {
                    additional_info = nv::i2c::TransparentDsIndexCcodes.at(ds_index);
                }
                packet.priv.packet_interface = static_cast<uint8_t>(mctp::Client::UsI2c);
                auto status                  = i2c::Task::tx(packet, additional_info);
                if (status != i2c::Task::Status::Ok) {
                    nv::info("i2c_tx fail 0x%x\n", status);
                }
                is_routed = true;
            }
        }
    }

    mctp::Status status{};
    if (is_routed == false) {
        status = mctp::Driver::mctp_send(item, _client);
        if (status != nv::mctp::Status::Ok) {
            nv::info("fail to forward packet (%d)\n", status);
        }
    }
}

void Task::transmit(std::span<uint8_t> packet)
{
    if (packet.size() == 0) {
        return;
    }
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::I3cTaskDriverTx);
    auto result = _driver.write(packet.front() >> 1U, packet.subspan(1, packet.size() - 1));
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::I3cTaskDriverTxDone);
    nv::perf_mon::Driver::log_pkt_meas(static_cast<uint32_t>(_client),
                                       static_cast<uint32_t>((packet.size() - 1U) & UINT8_MAX),
                                       false,
                                       !result);
    if (result) {
        // reset status timer since AP proved connectivity with successful transmit
        if (_timer.id() == ipc::TimerId::Ap1Status || _timer.id() == ipc::TimerId::Ap2Status) {
            _timer.reset();
        }
    }
}

bool Task::handle_gpu_reset(uint8_t ocp_addr, uint8_t therm_addr)
{
    // 3.10.5.4.1 OOBHUB hardcode
    using namespace nv;
    using namespace std::chrono_literals;
    const auto port = static_cast<uint8_t>(_driver.port());
    if (_gpu_recovery_addr == 0) {
        logger::error_no_wait(logger::Event::I3COobResetV2,
                              {port, static_cast<uint8_t>(GpuOobResetStatus::FailToEnable)});
        return false;
    }
    // send OCP recover command to enable I3C interface
    constexpr uint8_t EnIfRetry        = 3;
    bool              enable_interface = false;
    for (uint8_t i = 0; i < EnIfRetry; i++) {
        delay(10ms);
        // enable interface mastering
        if (!_driver.ocp_enable_interface_mastering(ocp_addr)) {
            logger::error_no_wait(
                logger::Event::I3COobResetV2,
                {port, static_cast<uint8_t>(GpuOobResetStatus::FailToEnable)});
            continue;
        }
        delay(10ms);
        // query interface mastering
        if (!_driver.ocp_query_interface_mastering(ocp_addr, enable_interface)) {
            logger::error_no_wait(logger::Event::I3COobResetV2,
                                  {port, static_cast<uint8_t>(GpuOobResetStatus::FailToQuery)});
        }
    }
    if (!enable_interface) {
        return false;
    }
    delay(10ms);
    // configure interrupt
    if (!_driver.enec()) {
        logger::error_no_wait(logger::Event::I3COobResetV2,
                              {port, static_cast<uint8_t>(GpuOobResetStatus::FailToSetInt)});
        return false;
    }
    delay(10ms);
    // dynamic address assignment
    bool daa_result = false;
    for (uint8_t i = 0; i < DaaRetry; i++) {
        if (_driver.process_daa(_address_pool)) {
            daa_result = true;
            break;
        }
        delay(100ms);
    }
    if (!daa_result) {
        logger::error_no_wait(
            logger::Event::I3COobResetV2,
            {port, static_cast<uint8_t>(GpuOobResetStatus::FailToAssignAddr)});
        return false;
    }
    delay(10ms);
    // wait for GPU enter to I3C mode
    constexpr uint8_t Retry = 100;
    bool              i3c   = false;
    for (uint8_t i = 0; i < Retry; i++) {
        if (_driver.gpu_query_i3c_mode(therm_addr, i3c) && i3c) {
            logger::info(logger::Event::I3COobResetV2,
                         {port, static_cast<uint8_t>(GpuOobResetStatus::Enable)});
            return true;
        }
        delay(1ms);
    }
    logger::error_no_wait(logger::Event::I3COobResetV2,
                          {port, static_cast<uint8_t>(GpuOobResetStatus::GpuNotInI3cMode)});
    return false;
}

bool Task::handle_cxx_reset(bool ping)
{
    using namespace std::chrono_literals;

    const constexpr uint32_t DelayInit = 2000;
    bool                     result    = false;

    for (uint8_t i = 0; i < DaaRetry; i++) {
        // detect cxx
        result = _driver.reset_daa(ping);
        if (result) {
            // NBU team requests to add a delay bewteen RSTDAA and ENTDAA
            if constexpr (nv::ipc::EnableDelayInI3CInit) {
                nv::ctimer::Driver::delay_for_us(DelayInit);
            }
            result = _driver.process_daa(_address_pool);
            if (result) {
                // delay 100ms to allow time for MCTP thread to be ready for SetEID request
                delay(100ms);
                return result;
            }
        }
        delay(100ms);
    }
    return false;
}

bool Task::to_i2c(ipchandler::Id    src_id,
                  uint8_t           address,
                  ipchandler::Id    i2c_ipchandler_id,
                  uint16_t          write_length,
                  uint16_t          read_length,
                  ipc::Queue::Item& item)
{
    // Validate lengths against buffer limits
    if (write_length > nv::i2c::I2cBufferSize || read_length > nv::i2c::I2cHidSmbBufferSize) {
        return false;
    }

    using namespace nv::ipc;
    static_assert(sizeof(nv::i2c::I2cRequest) <= Driver::BufferSize);
    Request request{
        .type   = RequestType::I2cRequest,
        .length = sizeof(nv::i2c::I2cRequest),
    };
    auto i2c_request          = std::bit_cast<nv::i2c::I2cRequest*>(request.buffer.data());
    i2c_request->address      = address;
    i2c_request->write_length = write_length;
    i2c_request->read_length  = read_length;
    i2c_request->src_id       = static_cast<uint8_t>(src_id);

    // Copy write data if any
    if (write_length > 0) {
        std::copy_n(item.begin(), write_length, i2c_request->write_buffer.begin());
    }

    // Create queue item with the full request size
    auto request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));

    // Send to IPC handler
    auto status = ipchandler::Driver::send(
        src_id, i2c_ipchandler_id, request_item, sizeof(Request), true);
    if (status != ipchandler::Status::Success) {
        nv::info("fail to put i2c request (%d)\n", status);
        return false;
    }

    return true;
}

void Task::read_gpu_sensor(uint8_t reg)
{
    using namespace std;
    using namespace nv::i2c;
    constexpr uint8_t GpuSmbReadReq = 0xCF;
    const uint8_t     address       = _address_pool.at(0) << 1;
    array<uint8_t, 4> payload{address, GpuSmbReadReq, reg};
    payload.at(3) = crc8(span<uint8_t>(payload).subspan(0, payload.size() - 1));
    auto result   = _driver.write(payload.at(0) >> 1,
                                span<uint8_t>(payload).subspan(1, payload.size() - 1));
    if (!result) {
        // Ignore this error for telemetry
    }
}

void Task::record_error(uint8_t error_type) const
{
    nv::perf_mon::Driver::set_transaction_error(_oob_bus, error_type);
}

uint8_t Task::get_smbpbi_command_index()
{
    return smbpbi_command_index;
}

void Task::update_smbpbi_command_index()
{
    smbpbi_command_index += 1;
    if (smbpbi_command_index >= smbpbi::NumSmbPbiCommandToRead) {
        smbpbi_command_index = 0;
    }
}

void Task::update_smbpbi_items(uint32_t value)
{
    const nv::telemetry::TelemId item = _smbpbi_items.at(get_smbpbi_command_index());
    // For read temperature, first byte is all zero
    if (item == nv::telemetry::TelemId::Gpu1Temp || item == nv::telemetry::TelemId::Gpu2Temp) {
        constexpr uint8_t ByteMask = 0xFF;
        constexpr uint8_t Shift    = 8;
        value                      = (value >> Shift) & ByteMask;
    }
    nv::telemetry::Cache::inst().set_cache(item, value);
    smbpbi_not_ready_count = 0;
    smbpbi_state           = smbpbi::State::Init;
    update_smbpbi_command_index();
}

void Task::prepare_nvl_topology_info()
{
    using namespace nv;
    using namespace sys::topology;
    using namespace std::chrono_literals;
    TopologyInfo::NVL_topology_info topology_info;
    auto                            result = TopologyInfo::get_topology_info(
        _fru_i2c_info.port, _fru_i2c_info.eeprom_addr, _platform_info, topology_info);
    if (!result) {
        logger::error(logger::Event::FruGetTopologyFail, {static_cast<uint8_t>(_client)});
    }
    // drive nvs present
    uint8_t state = 0;
    if (_fru_i2c_info.port == nv::i2c::Port::End) {
        // No FRU case
        state = _platform_info.nvs_present ? 0 : 1;
    }
    else {
        // FRU case
        state = (topology_info.TOPOLOGY_ID_TYPE & 0x80) != 0 ? 0 : 1;
    }
    logger::info(logger::Event::NvlNvsPresent,
                 {static_cast<uint8_t>(_client),
                  _platform_info.nvs_present_port,
                  _platform_info.nvs_present_pin,
                  state});
    (void)gpio::Driver::write(
        _platform_info.nvs_present_port, _platform_info.nvs_present_pin, state);

    // I2C path
    if (!_driver.gpu_configure_cms1(_gpu_recovery_addr)) {
        logger::error(logger::Event::NvlCms1Fail,
                      {static_cast<uint8_t>(_client),
                       static_cast<uint8_t>(NvlCms1Status::FailToConfigure)});
        return;
    }
    delay(10ms);
    if (!_driver.gpu_program_cms1(_gpu_recovery_addr, topology_info.to_span())) {
        logger::error(logger::Event::NvlCms1Fail,
                      {static_cast<uint8_t>(_client),
                       static_cast<uint8_t>(NvlCms1Status::FailToProgram)});
        return;
    }
    delay(10ms);
    topology_info = {};
    if (!_driver.gpu_read_cms1(_gpu_recovery_addr, topology_info.to_span())) {
        logger::error(
            logger::Event::NvlCms1Fail,
            {static_cast<uint8_t>(_client), static_cast<uint8_t>(NvlCms1Status::FailToRead)});
        return;
    }
    logger::info(
        logger::Event::NvlRaw,
        {
            static_cast<uint8_t>(_client),
            topology_info.TRAY_TYPE,
            topology_info.TOPOLOGY_ID_TYPE,
            topology_info.CHASSIS_PHYSICAL_SLOT_NUMBER,
            topology_info.COMPUTE_SLOT_INDEX,
            static_cast<uint8_t>(topology_info.NODE_INDEX | topology_info.DEVICE_INDEX << 4),
        });

    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    logger::info(logger::Event::FruBoardSerial,
                 {
                     static_cast<uint8_t>(_client),
                     topology_info.RACK_GUID.serial_number[0],
                     topology_info.RACK_GUID.serial_number[1],
                     topology_info.RACK_GUID.serial_number[2],
                     topology_info.RACK_GUID.serial_number[3],
                     topology_info.RACK_GUID.serial_number[4],
                     topology_info.RACK_GUID.serial_number[5],
                     topology_info.RACK_GUID.serial_number[6],
                 });

    logger::info(logger::Event::FruBoardSerial,
                 {
                     static_cast<uint8_t>(_client),
                     topology_info.RACK_GUID.serial_number[7],
                     topology_info.RACK_GUID.serial_number[8],
                     topology_info.RACK_GUID.serial_number[9],
                     topology_info.RACK_GUID.serial_number[10],
                     topology_info.RACK_GUID.serial_number[11],
                     topology_info.RACK_GUID.serial_number[12],
                 });
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

void Task::handle_sync_cbc_sn([[maybe_unused]] std::span<uint8_t> buffer)
{
    sys::topology::TopologyInfo::update_sn();
}
