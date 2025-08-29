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
#include "nv/i2c/task.h"

#include <bit>
#include <climits>
#include <cstdint>
#include <cstring>

#include "nv/i2c/common.h"
#include "nv/i2c/helper.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"
#include "nv/usb/task.h"
#include "nv/i2c/sensor.h"
#include "sys/sensor/sensor.h"
#include "nv/i3c/task.h"
#include "nv/watchdog/runtime.h"
#include "nv/perf_mon/perf_mon.h"
#include "sys/i2c/utils.h"

#include NV_IPC_CONFIG_H

using namespace nv::i2c;

void Task::make(Config config)
{
    /// TODO need a way to create object and stack by projects
    constexpr auto StackSize = std::max(384, int(configMINIMAL_STACK_SIZE));
    switch (config.port_id) {
        case Port::Zero: {
            NV_TASK_DATA static Task                       task0(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack0;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv0(reinterpret_cast<uint8_t*>(&task0), sizeof(Task));
            task0.setup(stack0.span(), Priv0, Priority::I2c, Task::entrypoint);
            break;
        }
        case Port::One: {
            NV_TASK_DATA static Task                       task1(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack1;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv1(reinterpret_cast<uint8_t*>(&task1), sizeof(Task));
            task1.setup(stack1.span(), Priv1, Priority::I2c, Task::entrypoint);
            break;
        }
        case Port::Two: {
            NV_TASK_DATA static Task                       task2(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack2;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv2(reinterpret_cast<uint8_t*>(&task2), sizeof(Task));
            task2.setup(stack2.span(), Priv2, Priority::I2c, Task::entrypoint);
            break;
        }
        case Port::Three: {
            NV_TASK_DATA static Task                       task3(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack3;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv3(reinterpret_cast<uint8_t*>(&task3), sizeof(Task));
            task3.setup(stack3.span(), Priv3, Priority::I2c, Task::entrypoint);
            break;
        }
        case Port::Four: {
            NV_TASK_DATA static Task                       task4(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack4;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv4(reinterpret_cast<uint8_t*>(&task4), sizeof(Task));
            task4.setup(stack4.span(), Priv4, Priority::I2c, Task::entrypoint);
            break;
        }
        case Port::Five: {
            NV_TASK_DATA static Task                       task5(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack5;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv5(reinterpret_cast<uint8_t*>(&task5), sizeof(Task));
            task5.setup(stack5.span(), Priv5, Priority::I2c, Task::entrypoint);
            break;
        }
        case Port::Six: {
            NV_TASK_DATA static Task                       task6(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack6;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv6(reinterpret_cast<uint8_t*>(&task6), sizeof(Task));
            task6.setup(stack6.span(), Priv6, Priority::I2c, Task::entrypoint);
            break;
        }
        default: nv::info("I2C port %d does not support\n", config.port_id);
    }
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
, _client(config.client)
, _event(nv::ipc::Event::make(config.event_id))
, _queue(nv::ipc::Queue::make(config.queue_id))
, _port(config.port_id)
, _driver()
, _eid_addr_map()
, _target_addr(config.target_addr)
, _boot_event(config.boot_event)
, _ipchandler_id(config.ipchandler_id)
, _oob_bus(nv::perf_mon::OobBus::End)
{
    std::fill(_eid_addr_map.begin(), _eid_addr_map.end(), 0);
    _driver.bind(config.port_id, this);
    _driver.init();

    switch (config.client) {
        case mctp::Client::UsI2c : _oob_bus = nv::perf_mon::OobBus::UsI2c; break;
        case mctp::Client::DsI2c0: _oob_bus = nv::perf_mon::OobBus::DsI2c0; break;
        case mctp::Client::DsI2c1: _oob_bus = nv::perf_mon::OobBus::DsI2c1; break;
        case mctp::Client::DsI2c2: _oob_bus = nv::perf_mon::OobBus::DsI2c2; break;
        case mctp::Client::DsI2c3: _oob_bus = nv::perf_mon::OobBus::DsI2c3; break;
        default                  : break;
    }
    nv::perf_mon::Driver::set_oob_bus_valid(_oob_bus);

    // upstream I2C handles to get sensor data
    if (_port == i2c::Port::Zero && ipc::SensorUpdateMs) {
        _timer = ipc::Timer::make(nv::ipc::TimerId::SmbSensor,
                                  std::chrono::microseconds(ipc::SensorUpdateMs),
                                  update_sensor);
    }
    else if (ipc::UseI2cApStatusTimer && config.timer_id != ipc::TimerId::End) {
        _timer = ipc::Timer::make(config.timer_id,
                                  std::chrono::microseconds(ipc::CheckApStatusTimerUs),
                                  check_ap_status);
    }
}

Task::Status Task::tx(const nv::mctp::Packet& packet, uint16_t additional_info)
{
    /// TODO need a way to use queue by projects
    using namespace nv::ipc;
    auto queue_id = QueueId::End;
    switch (packet.priv.packet_interface) {
        case static_cast<uint8_t>(nv::mctp::Client::UsI2c) : queue_id = QueueId::I2c0; break;
        case static_cast<uint8_t>(nv::mctp::Client::DsI2c0): queue_id = QueueId::I2c1; break;
        case static_cast<uint8_t>(nv::mctp::Client::DsI2c1): queue_id = QueueId::I2c2; break;
        case static_cast<uint8_t>(nv::mctp::Client::DsI2c2): queue_id = QueueId::I2c3; break;
        case static_cast<uint8_t>(nv::mctp::Client::DsI2c3): queue_id = QueueId::I2c4; break;
    }
    if (queue_id == QueueId::End) {
        nv::info("invalid interface (%d)\n", packet.priv.packet_interface);
        return Task::Status::InvalidInterface;
    }
    Request request{};
    if constexpr (ipc::I2cTransparent) {
        request.type            = RequestType::MctpTx;
        request.additional_info = additional_info;
    }
    else {
        request.type = RequestType::MctpTx;
    }
    const size_t CopySize = std::min(sizeof(request.data), sizeof(packet));
    std::memcpy(static_cast<void*>(request.data), std::bit_cast<uint8_t*>(&packet), CopySize);
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = Queue::make(queue_id).send(item);
    if (queue_status != Queue::Status::Ok) {
        logger::error(
            logger::Event::I2CQueueFail,
            {packet.priv.packet_interface, 0, static_cast<uint8_t>(I2cQueueError::PutTxFail)});
        return Task::Status::FailToPutTxPacket;
    }
    return Task::Status::Ok;
}

Task::Status Task::update_routing_table(mctp::Client client)
{
    using namespace nv::ipc;
    ipc::QueueId queue_id{};
    switch (client) {
        case nv::mctp::Client::UsI2c : queue_id = QueueId::I2c0; break;
        case nv::mctp::Client::DsI2c0: queue_id = QueueId::I2c1; break;
        case nv::mctp::Client::DsI2c1: queue_id = QueueId::I2c2; break;
        case nv::mctp::Client::DsI2c2: queue_id = QueueId::I2c3; break;
        case nv::mctp::Client::DsI2c3: queue_id = QueueId::I2c4; break;
        default                      : return Task::Status::InvalidInterface;
    }
    const Request RequestPkt{.type = RequestType::MctpUpdateRoutingTable};
    auto          item = Queue::Item(std::bit_cast<uint8_t*>(&RequestPkt), sizeof(RequestPkt));
    auto          queue_status = Queue::make(queue_id).send(item);
    if (queue_status != Queue::Status::Ok) {
        logger::error(logger::Event::I2CQueueFail,
                      {static_cast<uint8_t>(static_cast<uint16_t>(client) & ByteMask),
                       static_cast<uint8_t>(static_cast<uint16_t>(client) >> 8U),
                       static_cast<uint8_t>(I2cQueueError::PutRoutingTableFail)});
        return Task::Status::FailToPutTxPacket;
    }
    return Task::Status::Ok;
}

Task::Status Task::wdt_notify(watchdog::TaskMonitorIndex taskId)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    ipc::QueueId queue_id{};
    switch (taskId) {
        case watchdog::TaskMonitorIndex::I2c0: queue_id = QueueId::I2c0; break;
        case watchdog::TaskMonitorIndex::I2c1: queue_id = QueueId::I2c1; break;
        case watchdog::TaskMonitorIndex::I2c2: queue_id = QueueId::I2c2; break;
        case watchdog::TaskMonitorIndex::I2c3: queue_id = QueueId::I2c3; break;
        case watchdog::TaskMonitorIndex::I2c4: queue_id = QueueId::I2c4; break;
        default                              : return Task::Status::InvalidInterface;
    }
    const Request RequestPkt{.type = RequestType::WdtEvent};
    auto          item = Queue::Item(std::bit_cast<uint8_t*>(&RequestPkt), sizeof(RequestPkt));
    auto          queue_status = Queue::make(queue_id).send(item, 0s);
    if (queue_status != Queue::Status::Ok) {
        logger::error(logger::Event::I2CQueueFail,
                      {static_cast<uint8_t>(static_cast<uint16_t>(taskId) & ByteMask),
                       static_cast<uint8_t>(static_cast<uint16_t>(taskId) >> 8U),
                       static_cast<uint8_t>(I2cQueueError::PutWdtNotifyFail)});
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
        case TimerId::SmbSensor: queue_id = QueueId::I2c0; break;
        default                : return;
    }
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = Queue::make(queue_id).send(item, 0s);
    if (queue_status != Queue::Status::Ok) {};
}

void Task::start()
{
    using namespace nv::ipc;
    _driver.start();
    Request request{};
    auto    item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    if (_port == i2c::Port::Zero && _timer.id() != ipc::TimerId::End) {
        set_tmp_sensor();
        _timer.start();
    }
    nv::bootloader::Driver::set_task_booted(_boot_event);

    if (EnableFpgaI2cReEnumeration && _port == i2c::Port::Zero) {
        attempt_i2c_recovery();
    }

    while (true) {
        auto status = _queue.recv(item);
        if (status != Queue::Status::Ok) {
            logger::error(logger::Event::I2CQueueFail,
                          {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                           static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                           static_cast<uint8_t>(I2cQueueError::GetReqFail)});
            continue;
        }
        switch (request.type) {
            case RequestType::WdtEvent              : handle_wdt_event(); break;
            case RequestType::MctpRx                : handle_rx(request.data); break;
            case RequestType::MctpTx                : handle_tx(request.data, request.additional_info); break;
            case RequestType::UpdateSensor          : handle_update_sensor(request.data); break;
            case RequestType::MctpUpdateRoutingTable: handle_update_routing_table(); break;
            case RequestType::MctpRxError           : handle_error(Error::TargetRxError); break;
            case RequestType::I2cRequest            : handle_i2c_request(request.data); break;
            case RequestType::I2cResponse           : handle_i2c_response(request.data); break;
            case RequestType::CheckApStatus         : handle_ap_status(APStatus::Querying); break;
        }
    }
}

void Task::attempt_i2c_recovery()
{
    constexpr uint8_t MaxRetries       = 10;
    bool              recovery_success = false;

    for (uint8_t retry = 0; retry < MaxRetries; retry++) {
        if (_driver.send_i2c_recovery()) {
            recovery_success = true;
            nv::logger::info(
                nv::logger::Event::I2CRecoverySuccess,
                {static_cast<uint8_t>(retry + 1), static_cast<uint8_t>(I2cRecovery::Success)});
            break;
        }
        // Small delay between retries
        using namespace std::chrono_literals;
        delay(1ms);
    }

    if (!recovery_success) {
        nv::logger::error(
            nv::logger::Event::I2CRecoveryFail,
            {static_cast<uint8_t>(MaxRetries), static_cast<uint8_t>(I2cRecovery::Fail)});
    }
}

void Task::handle_wdt_event()
{
    watchdog::TaskMonitorIndex index{};

    switch (_client) {
        case mctp::Client::UsI2c : index = watchdog::TaskMonitorIndex::I2c0; break;
        case mctp::Client::DsI2c0: index = watchdog::TaskMonitorIndex::I2c1; break;
        case mctp::Client::DsI2c1: index = watchdog::TaskMonitorIndex::I2c2; break;
        case mctp::Client::DsI2c2: index = watchdog::TaskMonitorIndex::I2c3; break;
        case mctp::Client::DsI2c3: index = watchdog::TaskMonitorIndex::I2c4; break;
        default                  : return;
    }

    watchdog::Runtime::mark_task_alive(index);
}

void Task::set_tmp_sensor()
{
    const uint8_t Limit  = 115;
    uint8_t       value  = 0;
    I2cStatus     status = I2cStatus::Ok;
    // coverity[DEADCODE] the table is not 0 on other projects
    for (auto& [port, offset, item, gpio] : nv::ipc::ModuleTempSensorList) {
        // If a GPIO filter is associated with this temp sensor, don't set temp sensor limit
        // if the GPIO is low
        auto [gpio_port, gpio_pin] = gpio;
        if (gpio_port != nv::gpio::InvalidGpioPort) {
            uint8_t gpio_value  = 0;
            auto    gpio_status = nv::gpio::Driver::read(gpio_port, gpio_pin, gpio_value);
            if (gpio_status != nv::gpio::Status::Ok
                || gpio_value == static_cast<uint8_t>(nv::gpio::GpioState::Low)) {
                continue;
            }
        }

        switch (item) {
            case nv::telemetry::TelemId::ModuleTemp1:
            case nv::telemetry::TelemId::ModuleTemp2:
                // Remote temp
                status = TempSensor(port, offset, item)
                             .write_reg(TempSensor::Register::RemoteTempLimit, Limit);
                nv::logger::info(nv::logger::Event::I2CTempSensor,
                                 {static_cast<uint8_t>(TempSensor::Status::SetRegister),
                                  offset,
                                  static_cast<uint8_t>(status),
                                  Limit});
                status = TempSensor(port, offset, item)
                             .read_reg(TempSensor::Register::RemoteTempLimit, value);
                nv::logger::info(nv::logger::Event::I2CTempSensor,
                                 {static_cast<uint8_t>(TempSensor::Status::RemoteLimit),
                                  offset,
                                  static_cast<uint8_t>(status),
                                  value});
                // local temp
                status = TempSensor(port, offset, item)
                             .write_reg(TempSensor::Register::LocalTempLimit, Limit);
                nv::logger::info(nv::logger::Event::I2CTempSensor,
                                 {static_cast<uint8_t>(TempSensor::Status::SetRegister),
                                  offset,
                                  static_cast<uint8_t>(status),
                                  Limit});
                status = TempSensor(port, offset, item)
                             .read_reg(TempSensor::Register::LocalTempLimit, value);
                nv::logger::info(nv::logger::Event::I2CTempSensor,
                                 {static_cast<uint8_t>(TempSensor::Status::LocalLimit),
                                  offset,
                                  static_cast<uint8_t>(status),
                                  value});
                break;
            default: break;
        }
    }
}

void Task::rx(std::span<uint8_t> buffer)
{
    using namespace nv::ipc;
    Request request{};
    request.type          = RequestType::MctpRx;
    const size_t CopySize = std::min(buffer.size(), sizeof(request.data));
    std::memcpy(static_cast<void*>(request.data), buffer.data(), CopySize);
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = _queue.send_isr(item);
    if (queue_status != Queue::Status::Ok) {
        logger::Logger::add_from_isr(
            logger::Event::I2CQueueFail.unique_id,
            logger::Level::Error,
            {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
             static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
             static_cast<uint8_t>(I2cQueueError::PutRxFail)});
        return;
    }
}

void Task::rx_error()
{
    using namespace nv::ipc;
    // NOLINTNEXTLINE(misc-const-correctness)
    Request request{.type = RequestType::MctpRxError};
    auto    item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto    queue_status = _queue.send_isr(item);
    if (queue_status != Queue::Status::Ok) {
        logger::Logger::add_from_isr(
            logger::Event::I2CQueueFail.unique_id,
            logger::Level::Error,
            {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
             static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
             static_cast<uint8_t>(I2cQueueError::PutRxFail)});
        return;
    }
}

void Task::handle_rx(std::span<uint8_t> buffer)
{
    nv::mctp::Packet packet{};
    uint8_t          command_code{};
    auto             result = process_rx_packet(buffer, packet, command_code);
    if (result) {
        // reset status timer since AP proved connectivity with successful receive
        if (_timer.id() == ipc::TimerId::Ap1Status || _timer.id() == ipc::TimerId::Ap2Status) {
            _timer.reset();
        }
        forward(packet, command_code);
    }
    else {
        logger::error(logger::Event::I2CPktDrop,
                      {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                       static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                       static_cast<uint8_t>(I2cPktDrop::Rx)});
    }
    nv::perf_mon::Driver::log_pkt_meas(static_cast<uint32_t>(_client),
                                       static_cast<uint32_t>(buffer.size() & UINT8_MAX),
                                       true,
                                       !result);
}

void Task::handle_tx(std::span<uint8_t> buffer, uint16_t additional_info)
{
    using namespace std::chrono_literals;
    I2cPacket packet{};
    if (process_tx_packet(buffer, packet, additional_info)) {
        auto result = transmit(packet);
        if (result) {
            // reset status timer since AP proved connectivity with successful transmit
            if (_timer.id() == ipc::TimerId::Ap1Status
                || _timer.id() == ipc::TimerId::Ap2Status) {
                _timer.reset();
            }
        }
    }
    else {
        logger::error(logger::Event::I2CPktDrop,
                      {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                       static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                       static_cast<uint8_t>(I2cPktDrop::Tx)});
    }
}

void Task::handle_update_sensor([[maybe_unused]] std::span<uint8_t> buffer)
{
    using namespace nv::telemetry;
    using namespace sys;
    // coverity[DEADCODE] the table is not 0 on other projects
    for (auto& [port, offset, item, gpio] : nv::ipc::ModuleTempSensorList) {
        // Check if a GPIO filter is associated with this temp sensor
        auto [gpio_port, gpio_pin] = gpio;
        if (gpio_port != nv::gpio::InvalidGpioPort) {
            uint8_t gpio_value  = 0;
            auto    gpio_status = nv::gpio::Driver::read(gpio_port, gpio_pin, gpio_value);
            if (gpio_status != nv::gpio::Status::Ok
                || gpio_value == static_cast<uint8_t>(nv::gpio::GpioState::Low)) {
                Cache::inst().set_cache(item, Cache::InvalidItem);
                continue;
            }
        }
        TempSensor(port, offset, item).update_cache();
    }
    // TODO move to Telemetry task
    // InternalTemp
    float internal_temp_raw = 0;
    auto  result            = sensor::Driver::get_current_temperature(internal_temp_raw);
    if (result) {
        Cache::inst().set_cache(TelemId::InternalTemp,
                                static_cast<uint32_t>(internal_temp_raw));
    }
    else {
        Cache::inst().set_cache(TelemId::InternalTemp, Cache::InvalidItem);
    }
}

void Task::handle_update_routing_table()
{
    using namespace std::chrono_literals;

    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&_routing_table), sizeof(_routing_table));
    auto             router_queue = ipc::Queue::make(ipc::QueueId::RoutingTable);
    auto             queue_status = router_queue.recv(item, 100ms);

    if (queue_status != ipc::Queue::Status::Ok) {
        logger::error(logger::Event::I2CPktDrop,
                      {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                       static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                       static_cast<uint8_t>(I2cQueueError::GetRoutingTableFail)});
    }
    else if (ipc::CheckApStatusTimerUs) {
        // routing table events come from upstream enumueration, update status if checking for
        // AP status; update status if I2C is not running the timer or if AP is up and not
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

I2cStatus
Task::i2c_cmd(uint8_t address, std::span<uint8_t> write_buffer, std::span<uint8_t> read_buffer)
{
    //  If both write and read are requested, use write-read operation
    if (write_buffer.size() > 0 && read_buffer.size() > 0) {
        return sys::i2c::i2c_write_read(_port, address, write_buffer, read_buffer);
    }

    // Write only operation
    if (write_buffer.size() > 0) {
        return sys::i2c::i2c_write(_port, address, write_buffer);
    }

    // Read only operation
    if (read_buffer.size() > 0) {
        return sys::i2c::i2c_read(_port, address, read_buffer);
    }

    return I2cStatus::Ok;
}

void Task::handle_i2c_request(std::span<uint8_t> buffer)
{
    nv::i2c::I2cHidBuffer read_buffer = {0};
    nv::i2c::I2cRequest   request     = {0};
    const size_t          RequestSize = std::min(sizeof(nv::i2c::I2cRequest), buffer.size());
    std::memcpy(&request, buffer.data(), RequestSize);
    const size_t WriteLength = std::min(static_cast<size_t>(request.write_length),
                                        request.write_buffer.size());
    const size_t ReadLength  = std::min(static_cast<size_t>(request.read_length),
                                       read_buffer.size());
    auto write_buffer_span   = std::span<uint8_t>(request.write_buffer.data(), WriteLength);
    auto read_buffer_span    = std::span<uint8_t>(read_buffer.data(), ReadLength);
    auto result              = i2c_cmd(request.address, write_buffer_span, read_buffer_span);
    if (request.src_id == static_cast<uint8_t>(ipchandler::Id::Usb)) {
        std::span<uint8_t> item(read_buffer.data(), read_buffer.size());
        usb::Task::to_usb(this->_ipchandler_id, ReadLength, item, result);
    }
}

void Task::handle_i2c_response(std::span<uint8_t> buffer)
{
    [[maybe_unused]] auto response = std::bit_cast<nv::i2c::I2cResponse*>(buffer.data());
}

Task::Status Task::set_event(Event event, bool isr)
{
    auto status = _event.set(common::to_underlying(event), isr);
    if (status != nv::ipc::Event::Status::Ok) {
        if (!isr) {
            logger::error(logger::Event::I2CSetEventFail,
                          {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                           static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U)});
        }
        else {
            logger::Logger::add_from_isr(
                logger::Event::I2CSetEventFail.unique_id,
                logger::Level::Error,
                {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                 static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U)});
        }
        return Task::Status::FailToSetEvent;
    }
    return Task::Status::Ok;
}

bool Task::process_rx_packet(std::span<uint8_t> buffer,
                             nv::mctp::Packet&  mctp_packet,
                             uint8_t&           command_code)
{
    constexpr uint8_t CommandCode = 0x0F;
    constexpr uint8_t MinCount    = 5;
    constexpr uint8_t MaxCount    = 73;
    constexpr uint8_t I2cStart    = 0x08;
    constexpr uint8_t I2cEnd      = 0x77;
    /// check PEC
    uint8_t length = 0;
    if (buffer[0] >= 1) {
        constexpr uint8_t ByteMask = 0xFF;
        length                     = static_cast<uint8_t>((buffer[0] - 1U) & ByteMask);
    }
    auto payload = buffer.subspan(1, length);
    auto pec     = buffer[buffer[0]];
    if (crc8(payload) != pec) {
        nv::info("pec is invalid\n");
        return false;
    }
    /// check I2C header
    auto i2c_packet = std::bit_cast<I2cPacket*>(payload.data());
    if constexpr (ipc::I2cTransparent) {
        if (std::find(nv::i2c::TransparentDsIndexCcodes.begin(),
                      nv::i2c::TransparentDsIndexCcodes.end(),
                      i2c_packet->i2c_hdr.cmd_code)
                == nv::i2c::TransparentDsIndexCcodes.end()
            && i2c_packet->i2c_hdr.cmd_code != CommandCode) {
            nv::info("command code is not valid\n");
            return false;
        }
    }
    else {
        if (i2c_packet->i2c_hdr.cmd_code != CommandCode) {
            nv::info("command code is not valid\n");
            return false;
        }
    }
    if (i2c_packet->i2c_hdr.byte_cnt < MinCount || i2c_packet->i2c_hdr.byte_cnt > MaxCount) {
        nv::info("packet length is invalid\n");
        return false;
    }
    if (i2c_packet->i2c_hdr.src_addr < I2cStart || i2c_packet->i2c_hdr.src_addr > I2cEnd) {
        nv::info("source address is incorrect\n");
        return false;
    }
    if (_target_addr == DynAddr) {
        /// update EID-I2C table
        _eid_addr_map.at(i2c_packet->mctp_hdr.src_eid) = i2c_packet->i2c_hdr.src_addr;
    }
    /// convert to internal MCTP packet
    /// subtract is because of 'Source Slave Address' following 'byte count'
    mctp_packet.priv.packet_length    = i2c_packet->i2c_hdr.byte_cnt - 1U;
    mctp_packet.priv.packet_interface = static_cast<uint16_t>(_client) > UINT8_MAX
                                          ? 0
                                          : nv::common::to_underlying(_client);
    mctp_packet.hdr                   = i2c_packet->mctp_hdr;

    if constexpr (ipc::I2cTransparent) {
        auto it = std::find(nv::i2c::TransparentDsIndexCcodes.begin(),
                            nv::i2c::TransparentDsIndexCcodes.end(),
                            i2c_packet->i2c_hdr.cmd_code);
        // If the command code is a transparent DS index, set the packet interface to the client
        // of the DS index
        if (it != nv::i2c::TransparentDsIndexCcodes.end()) {
            auto ds_index = std::distance(nv::i2c::TransparentDsIndexCcodes.begin(), it);
            if (ds_index < ipc::DownStreamNum) {
                auto ds                           = ipc::DownStreamInfos.at(ds_index);
                mctp_packet.priv.packet_interface = static_cast<uint8_t>(
                    static_cast<uint16_t>(ds.client) & UINT8_MAX);
            }
        }
        command_code = i2c_packet->i2c_hdr.cmd_code;
    }

    const size_t CopySize = std::min(mctp_packet.msg.size(), sizeof(i2c_packet->msg));
    std::memcpy(
        static_cast<void*>(&mctp_packet.msg[0]), static_cast<void*>(i2c_packet->msg), CopySize);
    return true;
}

bool Task::process_tx_packet(std::span<uint8_t> buffer,
                             I2cPacket&         i2c_packet,
                             uint16_t           additional_info)
{
    constexpr uint8_t CommandCode = 0x0F;
    auto              mctp_packet = nv::mctp::Packet::from(buffer);
    auto              dst_addr    = _target_addr;
    if (_target_addr == DynAddr) {
        /// lookup EID-I2C table
        dst_addr = _eid_addr_map.at(mctp_packet.hdr.dst_eid);
        if (dst_addr == 0) {
            nv::info("no I2C address for EID %d\n", mctp_packet.hdr.dst_eid);
            return false;
        }
    }
    i2c_packet.i2c_hdr.dst_addr = dst_addr;
    if constexpr (ipc::I2cTransparent) {
        if (mctp_packet.priv.packet_interface
            == nv::common::to_underlying(nv::mctp::Client::UsI2c)) {
            if (std::find(nv::i2c::TransparentDsIndexCcodes.begin(),
                          nv::i2c::TransparentDsIndexCcodes.end(),
                          additional_info)
                != nv::i2c::TransparentDsIndexCcodes.end()) {
                i2c_packet.i2c_hdr.cmd_code = static_cast<uint8_t>(additional_info & UINT8_MAX);
            }
            else {
                i2c_packet.i2c_hdr.cmd_code = CommandCode;
            }
        }
        else {
            i2c_packet.i2c_hdr.cmd_code = CommandCode;
        }
    }
    else {
        i2c_packet.i2c_hdr.cmd_code = CommandCode;
    }
    /// add 1 byte for "byte count" field
    auto byte_cnt               = mctp_packet.priv.packet_length + 1U;
    i2c_packet.i2c_hdr.byte_cnt = byte_cnt > UINT8_MAX ? UINT8_MAX
                                                       : static_cast<uint8_t>(byte_cnt);
    i2c_packet.i2c_hdr.ipmi_src = 0b1;
    i2c_packet.i2c_hdr.src_addr = _driver.address();
    i2c_packet.mctp_hdr         = mctp_packet.hdr;
    const size_t CopySize       = std::min(mctp_packet.msg.size(), sizeof(i2c_packet.msg));
    std::memcpy(
        static_cast<void*>(i2c_packet.msg), static_cast<void*>(&mctp_packet.msg[0]), CopySize);
    auto i2c_buffer = std::bit_cast<uint8_t*>(&i2c_packet);
    /// add 3 bytes for Address, CMD and Count fields
    auto size = i2c_packet.i2c_hdr.byte_cnt + 3U;
    /// need 1 byte to fill PEC, the payload size should not equal the struct size
    if (size >= sizeof(i2c_packet)) {
        nv::info("byte count field is invalid\n", mctp_packet.hdr.dst_eid);
        return false;
    }
    i2c_buffer[size] = crc8(std::span(i2c_buffer, i2c_buffer + size));
    return true;
}

void Task::forward(nv::mctp::Packet& packet, uint8_t command_code)
{
    auto item             = packet.to_span();
    bool is_routed        = false;
    bool is_usi2c_eid_set = false;

    for (auto& entry : _routing_table) {
        if (entry.is_enumerated == true) {
            if (entry.assigned_eid == packet.hdr.dst_eid) {
                if (entry.client == mctp::Client::UsUsb) {
                    auto status = usb::Task::usb_tx(item);
                    if (status != usb::Status::Ok) {
                        logger::error(
                            logger::Event::I2CForwardFail,
                            {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                             static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U)});
                    }
                    is_routed = true;
                }
                if constexpr (ipc::I2cTransparent) {
                    if (entry.client == mctp::Client::UsI2c) {
                        uint16_t additional_info = 0;
                        auto     ds_index = nv::mctp::Control::get_routing_index(_client);
                        if (ds_index < TransparentDsIndexCcodes.size()) {
                            additional_info = TransparentDsIndexCcodes.at(ds_index);
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
        if (!is_routed && is_usi2c_eid_set && _client != mctp::Client::UsI2c) {
            if (std::find(nv::i2c::TransparentSkippedEids.begin(),
                          nv::i2c::TransparentSkippedEids.end(),
                          packet.hdr.dst_eid)
                == nv::i2c::TransparentSkippedEids.end()) {
                uint16_t additional_info = 0;
                auto     ds_index        = nv::mctp::Control::get_routing_index(_client);
                if (ds_index < TransparentDsIndexCcodes.size()) {
                    additional_info = TransparentDsIndexCcodes.at(ds_index);
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

    // packet was sent upstream, return
    if (is_routed) {
        return;
    }

    mctp::Status status{};

    if constexpr (nv::ipc::I2cTransparent) {
        auto it = std::find(nv::i2c::TransparentDsIndexCcodes.begin(),
                            nv::i2c::TransparentDsIndexCcodes.end(),
                            command_code);
        // is command code a transparent bridge command code?
        if (it != nv::i2c::TransparentDsIndexCcodes.end()) {
            auto ds_index = std::distance(nv::i2c::TransparentDsIndexCcodes.begin(), it);
            //  if upstream i2c is not ready or if command code not supported for project,
            //  return
            if (!is_usi2c_eid_set || ds_index >= ipc::DownStreamNum) {
                return;
            }
            auto ds_client = ipc::DownStreamInfos.at(ds_index).client;

            // log control commands from upstream i2c
            if (_client == mctp::Client::UsI2c && !mctp::Driver::is_allow_bridge(packet)) {
                logger::info(
                    logger::Event::MctpMcuActAsBridgePacketNotify,
                    {
                        static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                        static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                        packet.priv.packet_interface,
                        packet.msg.at(2),  // command_code
                        packet.msg.at(4)   // eid to set
                    });
            }
            // packet interface already assigned, send to downstream task
            switch (ds_client) {
                case nv::mctp::Client::DsI3c0:
                case nv::mctp::Client::DsI3c1: {
                    auto status = nv::i3c::Task::tx(packet);
                    if (status != nv::i3c::Task::Status::Ok) {
                        logger::error(
                            logger::Event::I2CForwardFail,
                            {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                             static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U)});
                        return;
                    }
                    is_routed = true;
                    break;
                }
                case nv::mctp::Client::DsI2c0:
                case nv::mctp::Client::DsI2c1:
                case nv::mctp::Client::DsI2c2:
                case nv::mctp::Client::DsI2c3: {
                    auto status = nv::i2c::Task::tx(packet);
                    if (status != nv::i2c::Task::Status::Ok) {
                        logger::error(
                            logger::Event::I2CForwardFail,
                            {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                             static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U)});
                        return;
                    }
                    is_routed = true;
                    break;
                }
                default:
                    logger::error(
                        logger::Event::I2CForwardFail,
                        {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                         static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U)});
                    return;
            }
        }
    }

    if (is_routed == false) {
        if (_client == mctp::Client::UsI2c) {
            status = mctp::Driver::mctp_send_from_us_i2c_0(item);
        }
        else if (_client == mctp::Client::DsI2c0) {
            status = mctp::Driver::mctp_send_from_ds_i2c_0(item);
        }
        else if (_client == mctp::Client::DsI2c1) {
            status = mctp::Driver::mctp_send_from_ds_i2c_1(item);
        }
        else if (_client == mctp::Client::DsI2c2) {
            status = mctp::Driver::mctp_send_from_ds_i2c_2(item);
        }
        else if (_client == mctp::Client::DsI2c3) {
            status = mctp::Driver::mctp_send_from_ds_i2c_3(item);
        }
        else {
            return;
        }
        if (status != nv::mctp::Status::Ok) {
            logger::error(logger::Event::I2CForwardFail,
                          {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                           static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U)});
        }
    }
}

bool Task::transmit(const I2cPacket& packet)
{
    using namespace std::chrono;
    constexpr auto Retry = 5;
    /// add 3 byte for Address, CMD, count fields and add 1 byte for "PEC" field
    auto size   = packet.i2c_hdr.byte_cnt + 4U;
    auto buffer = std::span<uint8_t>(std::bit_cast<uint8_t*>(&packet), size);
    auto wait   = Task::Event::CtrlDone | Task::Event::CtrlBusBusy | Task::Event::CtrlNak
              | Task::Event::CtrlArbLost | Task::Event::CtrlError;
    auto error = Error::Ok;
    _event.clear(wait);
    for (auto i = 0; i < Retry; i++) {
        if (!_driver.write(buffer)) {
            set_event(Task::Event::CtrlBusBusy);
        }
        auto event = _event.wait(wait, true, false, 500ms);
        if (event.value() & Task::Event::CtrlDone) {
            error = Error::Ok;
            break;
        }
        else if (event.value() & Task::Event::CtrlBusBusy) {
            error = Error::CtrlBusBusy;
            delay(10ms);
        }
        else if (event.value() & Task::Event::CtrlNak) {
            error = Error::CtrlNak;
            delay(10ms);
        }
        else if (event.value() & Task::Event::CtrlArbLost) {
            error = Error::CtrlArbLost;
            delay(10ms);
        }
        else if (event.value() & Task::Event::CtrlError) {
            error = Error::CtrlError;
            break;
        }
        else {
            // TODO driver hung, should we reset it?
            error = Error::CtrlTimeout;
            break;
        }
    }
    if (error != Error::Ok) {
        handle_error(error);
    }
    perf_mon::Driver::log_pkt_meas(
        0, static_cast<uint32_t>(buffer.size() & UINT8_MAX), false, error != Error::Ok);

    return (error == Error::Ok);
}

void Task::handle_error(Error reason)
{
    const uint8_t ByteMask = 0xFF;
    // NOLINTNEXTLINE(misc-const-correctness)
    nv::logger::EventData data{static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                               static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                               static_cast<uint8_t>(reason)};
    nv::logger::error(nv::logger::Event::I2CError, data);

    nv::perf_mon::Driver::set_transaction_error(_oob_bus, static_cast<uint8_t>(reason));
}

bool Task::to_i2c(ipchandler::Id    src_id,
                  uint8_t           address,
                  ipchandler::Id    i2c_ipchandler_id,
                  uint8_t           write_length,
                  uint16_t          read_length,
                  ipc::Queue::Item& item)
{
    // Create request
    Task::Request request{};
    request.type = Task::RequestType::I2cRequest;

    // Set up I2C request data
    auto* i2c_request         = std::bit_cast<I2cRequest*>(static_cast<void*>(request.data));
    i2c_request->address      = address;
    i2c_request->write_length = write_length;
    i2c_request->read_length  = read_length;
    i2c_request->src_id       = static_cast<uint8_t>(src_id);

    // Copy write data if any
    if (write_length > 0) {
        if (write_length > i2c_request->write_buffer.size()) {
            nv::info("Write length %d exceeds buffer size %d\n",
                     write_length,
                     i2c_request->write_buffer.size());
            return false;
        }
        std::copy_n(item.begin(), write_length, i2c_request->write_buffer.begin());
    }

    // Create queue item with the full request size
    auto request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));

    // Send to IPC handler
    auto status = ipchandler::Driver::send(
        src_id, i2c_ipchandler_id, request_item, sizeof(request), true);

    if (status != ipchandler::Status::Success) {
        nv::info("Failed to send I2C request (%d)\n", status);
        return false;
    }

    return true;
}

void Task::check_ap_status(nv::ipc::Timer& timer)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    QueueId       queue_id = QueueId::End;
    const Request request{.type = RequestType::CheckApStatus};
    switch (timer.id()) {
        // these QueueIds are not tightly coupled to the TimerId, double check for future
        // projects
        case TimerId::Ap1Status: queue_id = QueueId::I2c1; break;
        case TimerId::Ap2Status: queue_id = QueueId::I2c2; break;
        default                : return;
    }
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = Queue::make(queue_id).send(item, 0s);
    if (queue_status != Queue::Status::Ok) {
        /// drop event because task is busy
    }
}

void Task::handle_ap_status(APStatus set_status)
{
    bool     result = false;
    APStatus status = _ap_status;

    if (set_status == APStatus::Querying) {
        // if link is up, ping for status
        if (_target_addr != 0) {
            // perform an i2cdetect type operation to verify it's still alive
            result = _driver.get_status(_target_addr);
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
        logger::info(logger::Event::I2CApStatusUpdate,
                     {static_cast<uint8_t>(myep),
                      static_cast<uint8_t>(_ap_status),
                      static_cast<uint8_t>(status)});

        // update mctp driver with status for enumeration
        if (_timer.id() == ipc::TimerId::Ap1Status || _timer.id() == ipc::TimerId::Ap2Status) {
            if (status == APStatus::ApUpNotEnumerated || status == APStatus::ApDown) {
                nv::mctp::Driver::endpoint_status_change(
                    myep, (status == APStatus::ApUpNotEnumerated));
            }
        }
        _ap_status = status;
    }
}
