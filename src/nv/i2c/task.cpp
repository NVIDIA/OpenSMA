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
#include <chrono>
#include <climits>
#include <cstring>

#include "nv/i2c/common.h"
#include "nv/i2c/eeprom_cache.h"
#include "nv/i2c/error_injection.h"
#include "nv/i2c/helper.h"
#include "nv/i2c/recovery.h"
#include "nv/i2c/sensor.h"
#include "nv/i3c/task.h"
#include "nv/logger/log.h"
#include "nv/lstp/lstp_router.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/selftest.h"
#include "nv/nv.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/usb/task.h"
#include "nv/volt_mon/mcu_internal_temp.h"
#include "nv/watchdog/runtime.h"
#include "sys/i2c/loopback.h"
#include "sys/i2c/utils.h"
#include "sys/sensor/sensor.h"

#include NV_IPC_CONFIG_H
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-dcl37-c,cert-dcl51-cpp)
extern nv::mctp::SelfTest::I2cLoopbackTestResultStruct loopback_result;

extern bool             projectIsApStatusTimer(nv::ipc::Timer& timer);
extern nv::ipc::QueueId projectGetApStatusQueueId(nv::ipc::Timer& timer);
extern void             project_stop_polling_timers();

using namespace nv::i2c;

namespace {
// Can be evaluated at compile time when given a constexpr client,
// but is also fine for normal runtime use.
constexpr bool is_target_client(nv::mctp::Client client)
{
    if (client == nv::mctp::Client::UsI2c) {
        return true;
    }

    for (const auto& ds : nv::ipc::DownStreamInfos) {
        if (ds.client == client) {
            return true;
        }
    }

    return false;
}

constexpr bool is_retryable(I2cStatus status)
{
    return status == I2cStatus::Busy || status == I2cStatus::Nak || status == I2cStatus::ArbLost
        || status == I2cStatus::FifoError;
}

}  // namespace

void Task::make(Config config)
{
    switch (config.port_id) {
        case Port::Zero : make_task_by_port<Port::Zero>(config); break;
        case Port::One  : make_task_by_port<Port::One>(config); break;
        case Port::Two  : make_task_by_port<Port::Two>(config); break;
        case Port::Three: make_task_by_port<Port::Three>(config); break;
        case Port::Four : make_task_by_port<Port::Four>(config); break;
        case Port::Five : make_task_by_port<Port::Five>(config); break;
        case Port::Six  : make_task_by_port<Port::Six>(config); break;
        case Port::Seven: make_task_by_port<Port::Seven>(config); break;
        case Port::Eight: make_task_by_port<Port::Eight>(config); break;
        case Port::Nine : make_task_by_port<Port::Nine>(config); break;
        default         : nv::info("I2C port %d does not support\n", config.port_id);
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
, _oob_bus(nv::perf_mon::Driver::flexcomm_port_to_oobBus(static_cast<uint8_t>(config.port_id)))
{
    std::fill(_eid_addr_map.begin(), _eid_addr_map.end(), 0);
    _driver.bind(config.port_id, this);
    _driver.init();

    nv::perf_mon::Driver::set_oob_bus_valid(_oob_bus);
    nv::perf_mon::Driver::set_oob_bus_type(_oob_bus, nv::perf_mon::OobBusType::I2c);

    // SMBus Direct cache refresh timer (if configured)
    // New VR Telemetry cache refresh timer
    if (_port == i2c::SmbusDirectPort && i2c::SmbusCacheRefreshMs > 0) {
        _timer = ipc::Timer::make(nv::ipc::TimerId::SmbusCacheRefresh,
                                  std::chrono::microseconds(i2c::SmbusCacheRefreshMs),
                                  refresh_smbus_cache);
    }
    else if (_port == i2c::Port::Zero && ipc::SmbSensorUpdateMs) {
        _timer = ipc::Timer::make(nv::ipc::TimerId::SmbSensor,
                                  std::chrono::microseconds(ipc::SmbSensorUpdateMs),
                                  update_sensor);
    }
    else if constexpr (nv::ipc::EnableEepromBridge) {
        if (_port == nv::ipc::EepromDstPort && ipc::EepromUpdateTimerUs) {
            // EEPROM port: periodic timer to sync dirty cache pages
            _timer = ipc::Timer::make(nv::ipc::TimerId::EepromUpdate,
                                      std::chrono::microseconds(ipc::EepromUpdateTimerUs),
                                      eeprom_update);
        }
    }
    else if (ipc::UseI2cApStatusTimer && config.timer_id != ipc::TimerId::End) {
        _timer = ipc::Timer::make(config.timer_id,
                                  std::chrono::microseconds(ipc::CheckApStatusTimerUs),
                                  check_ap_status);
    }

    if (config.repeated_start_timer_id != ipc::TimerId::End) {
        _repeated_start_timer = ipc::Timer::make(config.repeated_start_timer_id,
                                                 RepeatedStartTimeoutMs,
                                                 repeated_start_timeout,
                                                 false);
    }

    if constexpr (nv::i2c::EnableI2cPeripheralRecovery) {
        if constexpr (nv::i2c::I2CTargetTimeoutCheckIntervalMs > 0) {
            if (nv::i2c::I2cTargetTimeoutTimerClient == config.client) {
                _target_timeout_timer = ipc::Timer::make(
                    nv::ipc::TimerId::I2CTargetTimeoutCheck,
                    std::chrono::milliseconds(nv::i2c::I2CTargetTimeoutCheckIntervalMs),
                    check_target_timeout);
            }
        }
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
        case static_cast<uint8_t>(nv::mctp::Client::DsI2c4): queue_id = QueueId::I2c5; break;
        case static_cast<uint8_t>(nv::mctp::Client::DsI2c5): queue_id = QueueId::I2c6; break;
        case static_cast<uint8_t>(nv::mctp::Client::DsI2c6): queue_id = QueueId::I2c7; break;
        case static_cast<uint8_t>(nv::mctp::Client::DsI2c7): queue_id = QueueId::I2c8; break;
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
        case nv::mctp::Client::DsI2c4: queue_id = QueueId::I2c5; break;
        case nv::mctp::Client::DsI2c5: queue_id = QueueId::I2c6; break;
        case nv::mctp::Client::DsI2c6: queue_id = QueueId::I2c7; break;
        case nv::mctp::Client::DsI2c7: queue_id = QueueId::I2c8; break;
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
        case watchdog::TaskMonitorIndex::I2c5: queue_id = QueueId::I2c5; break;
        case watchdog::TaskMonitorIndex::I2c6: queue_id = QueueId::I2c6; break;
        case watchdog::TaskMonitorIndex::I2c7: queue_id = QueueId::I2c7; break;
        case watchdog::TaskMonitorIndex::I2c8: queue_id = QueueId::I2c8; break;
        case watchdog::TaskMonitorIndex::I2c9: queue_id = QueueId::I2c9; break;
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

Task::Status Task::send_recovery_request(RecoveryCmd cmd, mctp::Client client)
{
    using namespace nv::ipc;

    const bool is_in_isr = Supervisor::is_in_isr();

    // Determine which I2C Task queue to send to based on client
    ipc::QueueId queue_id{};
    switch (client) {
        case nv::mctp::Client::UsI2c : queue_id = QueueId::I2c0; break;
        case nv::mctp::Client::DsI2c0: queue_id = QueueId::I2c1; break;
        case nv::mctp::Client::DsI2c1: queue_id = QueueId::I2c2; break;
        case nv::mctp::Client::DsI2c2: queue_id = QueueId::I2c3; break;
        case nv::mctp::Client::DsI2c3: queue_id = QueueId::I2c4; break;
        case nv::mctp::Client::DsI2c4: queue_id = QueueId::I2c5; break;
        case nv::mctp::Client::DsI2c5: queue_id = QueueId::I2c6; break;
        case nv::mctp::Client::DsI2c6: queue_id = QueueId::I2c7; break;
        case nv::mctp::Client::DsI2c7: queue_id = QueueId::I2c8; break;
        default                      : return Task::Status::InvalidInterface;
    }

    // Create recovery request
    Request request{};
    request.type = RequestType::I2cRecovery;

    // Set up recovery request data
    I2cRecoveryRequest recovery_request{};
    recovery_request.cmd    = cmd;
    recovery_request.src_id = static_cast<uint8_t>(nv::ipchandler::Id::Mctp);

    // Check if I2cRecoveryRequest size is less than or equal to Request::data size
    static_assert(sizeof(I2cRecoveryRequest) <= DataSize,
                  "I2cRecoveryRequest size must not exceed Request::data size");
    // Explicit cast to avoid array-to-pointer decay warning
    std::memcpy(
        static_cast<void*>(request.data), &recovery_request, sizeof(I2cRecoveryRequest));

    // Send the request
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = is_in_isr ? Queue::make(queue_id).send_isr(item)
                                  : Queue::make(queue_id).send(item);

    if (queue_status != Queue::Status::Ok) {
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

void Task::refresh_smbus_cache(nv::ipc::Timer& timer)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    // Wake up MCTP task to refresh SMBus Direct cache in MCTP context.
    // This avoids cross-task access to MCTP-owned objects (e.g. power sensor manager)
    // from the I2C task, which can cause races or crashes during enumeration.
    const nv::mctp::Driver::Command Cmd{
        .cmd   = static_cast<uint16_t>(nv::mctp::Driver::CmdCode::SmbusCacheRefresh),
        .data1 = 0,
        .data2 = 0,
        .data3 = {},
    };
    switch (timer.id()) {
        case TimerId::SmbusCacheRefresh: break;
        default                        : return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto item         = Queue::Item(const_cast<uint8_t*>(std::bit_cast<const uint8_t*>(&Cmd)),
                            sizeof(Cmd));
    auto queue_status = Queue::make(QueueId::MctpCmd).send(item, 0s);
    if (queue_status != Queue::Status::Ok) {};
}

void Task::repeated_start_timeout(nv::ipc::Timer& timer)
{
    if constexpr (nv::lstp::EnableI2c) {
        using namespace nv::ipc;
        using namespace std::chrono_literals;

        QueueId queue_id = QueueId::End;
        for (const auto& [tid, qid] : I2cTimerInfos) {
            if (tid == timer.id()) {
                queue_id = qid;
                break;
            }
        }
        if (queue_id == QueueId::End) {
            return;
        }

        Request request{};
        request.type = RequestType::I2cRecovery;

        I2cRecoveryRequest recovery{};
        recovery.cmd    = RecoveryCmd::BusRecovery;
        recovery.src_id = static_cast<uint8_t>(ipchandler::Id::Lstp);
        std::memcpy(static_cast<void*>(request.data), &recovery, sizeof(I2cRecoveryRequest));

        auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
        auto queue_status = Queue::make(queue_id).send(item, 0s);
        if (queue_status != Queue::Status::Ok) {}
    }
}

void Task::eeprom_update(nv::ipc::Timer& timer)
{
    if constexpr (nv::ipc::EnableEepromBridge) {
        using namespace nv::ipc;
        using namespace std::chrono_literals;

        if (timer.id() != TimerId::EepromUpdate) {
            return;
        }

        const Request request{.type = RequestType::EepromUpdate};
        auto          item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
        (void)Queue::make(EepromI2cQueueId).send(item, 0s);
    }
}

void Task::start()
{
    const bool enable_target = is_target_client(_client);
    _driver.start(enable_target);

    using namespace nv::ipc;
    Request request{};
    auto    item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    if (_port == i2c::Port::Zero && _timer.id() != ipc::TimerId::End) {
        if (_timer.id() == ipc::TimerId::SmbSensor) {
            set_tmp_sensor();
        }
        _timer.start();
    }

    if constexpr (nv::i2c::EnableI2cPeripheralRecovery) {
        if constexpr (nv::i2c::I2CTargetTimeoutCheckIntervalMs > 0) {
            if (nv::i2c::I2cTargetTimeoutTimerClient == _client) {
                _target_timeout_timer.start();
            }
        }
    }

    // Initialize EEPROM cache directly for downstream EEPROM port
    if constexpr (nv::ipc::EnableEepromBridge) {
        if (_port == nv::ipc::EepromDstPort) {
            eeprom_cache().load_from_eeprom();
            if (_timer.id() != ipc::TimerId::End) {
                _timer.start();
            }
        }
    }

    nv::bootloader::Driver::set_task_booted(_boot_event);
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
            case RequestType::MctpRxError           : handle_error(I2cStatus::TargetRxError); break;
            case RequestType::I2cRequest            : handle_i2c_request(request.data); break;
            case RequestType::I2cResponse           : handle_i2c_response(request.data); break;
            case RequestType::I2cRecovery           : handle_i2c_recovery(request.data); break;
            case RequestType::CheckApStatus         : handle_ap_status(APStatus::Querying); break;
            case RequestType::I2cLoopbackTest       : handle_i2c_loopback_test(); break;
            case RequestType::EepromUpdate          : handle_eeprom_update(); break;
            case RequestType::CheckTargetTimeout    : handle_check_target_timeout(); break;
        }
    }
}

void Task::stop_polling_timers()
{
    constexpr int32_t DefaultPollingTimeout = 10;
    nv::ipc::Timer::make(nv::ipc::TimerId::SmbSensor,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::SmbusCacheRefresh,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::Ap1Status,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::Ap2Status,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::Ap3Status,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::EepromUpdate,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    project_stop_polling_timers();
}

void Task::handle_i2c_loopback_test()
{
    // Execute I2C loopback test for this port
    // This is called from the I2C task context
    constexpr int MaxPort = 9;
    // Loopback test watchdog reset timeout
    constexpr uint32_t LoopbackWatchdogResetMs = 30000;
    // Stop I2C polling to avoid collisions with the loopback test
    stop_polling_timers();
    if constexpr (nv::ipc::EnableRuntimeWdt) {
        nv::watchdog::Runtime::update_timeout(LoopbackWatchdogResetMs);
    }
    for (int i = 0; i <= MaxPort; i++) {
        sys::i2c::LoopbackDriver loopback_driver(static_cast<nv::i2c::Port>(i));
        loopback_driver.start_test();
    }
    if constexpr (nv::ipc::EnableRuntimeWdt) {
        nv::watchdog::Runtime::update_timeout(nv::ipc::RuntimeWatchdogResetMs);
        nv::logger::info(nv::logger::Event::I2cLoopbackWdtUpdate,
                         nv::logger::data_from_two_u32(LoopbackWatchdogResetMs,
                                                       nv::ipc::RuntimeWatchdogResetMs));
    }
    loopback_result.IsRunning = 0;
}

void Task::handle_wdt_event()
{
    watchdog::TaskMonitorIndex index{};
    auto                       queue_id = _queue.id();

    switch (queue_id) {
        case nv::ipc::QueueId::I2c0: index = watchdog::TaskMonitorIndex::I2c0; break;
        case nv::ipc::QueueId::I2c1: index = watchdog::TaskMonitorIndex::I2c1; break;
        case nv::ipc::QueueId::I2c2: index = watchdog::TaskMonitorIndex::I2c2; break;
        case nv::ipc::QueueId::I2c3: index = watchdog::TaskMonitorIndex::I2c3; break;
        case nv::ipc::QueueId::I2c4: index = watchdog::TaskMonitorIndex::I2c4; break;
        case nv::ipc::QueueId::I2c5: index = watchdog::TaskMonitorIndex::I2c5; break;
        case nv::ipc::QueueId::I2c6: index = watchdog::TaskMonitorIndex::I2c6; break;
        case nv::ipc::QueueId::I2c7: index = watchdog::TaskMonitorIndex::I2c7; break;
        case nv::ipc::QueueId::I2c8: index = watchdog::TaskMonitorIndex::I2c8; break;
        case nv::ipc::QueueId::I2c9: index = watchdog::TaskMonitorIndex::I2c9; break;
        default                    : return;
    }

    watchdog::Runtime::mark_task_alive(index);
}

void Task::set_tmp_sensor()
{
    const uint8_t Limit = 115;
    uint8_t   value  = 0;  // NOLINT(misc-const-correctness) modified inside loop on MCU builds
    I2cStatus status = I2cStatus::Ok;  // NOLINT(misc-const-correctness) modified inside loop on
                                       // MCU builds
    // coverity[DEADCODE] the table is not 0 on other projects
    for (auto& [port, offset, item] : nv::ipc::ModuleTempSensorList) {
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
        if (projectIsApStatusTimer(_timer)) {
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
            if (projectIsApStatusTimer(_timer)) {
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
    for (auto& [port, offset, item] : nv::ipc::ModuleTempSensorList) {
        TempSensor(port, offset, item).update_cache();
    }
    // TODO move to Telemetry task
    // InternalTemp
    float internal_temp_raw = 0;
    constexpr auto
         mcuTempCfg = nv::ipc::voltage_monitor_config::mcu_internal_temp_get_sensor_config();
    bool result     = false;
    if constexpr (mcuTempCfg.sensor != nv::volt_mon::Sensor::Invalid) {
        result = (nv::mcu_internal_temp::McuInternalTemp::inst().get_temperature_celsius(
                      internal_temp_raw)
                  == nv::volt_mon::Status::Ok);
    }
    else {
        result = sys::sensor::Driver::get_current_temperature(internal_temp_raw);
    }

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
        const bool is_ap_status_timer = projectIsApStatusTimer(_timer);
        if (!is_ap_status_timer || _ap_status == APStatus::ApUpNotEnumerated
            || _ap_status == APStatus::Querying) {
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

I2cStatus Task::wait_for_i2c_completion()
{
    using namespace std::chrono_literals;
    auto event = _event.wait(CtrlWaitEvents, true, false, 1000ms);

    switch (event.value()) {
        case Task::Event::CtrlDone         : return I2cStatus::Ok;
        case Task::Event::CtrlBusBusy      : return I2cStatus::Busy;
        case Task::Event::CtrlNak          : return I2cStatus::Nak;
        case Task::Event::CtrlArbLost      : return I2cStatus::ArbLost;
        case Task::Event::CtrlFifoError    : return I2cStatus::FifoError;
        case Task::Event::CtrlBitError     : return I2cStatus::BitError;
        case Task::Event::CtrlPinLowTimeout: return I2cStatus::PinLowTimeout;
        case Task::Event::CtrlDmaError     : return I2cStatus::DmaError;
        case Task::Event::CtrlTimeout      : return I2cStatus::Timeout;
        case Task::Event::CtrlNoTransfer   : return I2cStatus::NoTransfer;
        default                            : return I2cStatus::Error;
    }
    return I2cStatus::Error;
}

I2cStatus Task::smbus_block_read_wrapper(uint8_t            address,
                                         std::span<uint8_t> write_buffer,
                                         std::span<uint8_t> read_buffer,
                                         I2cFlags           flags,
                                         size_t&            read_len)
{
    // WAR: Fetch the max for SMBus Block Read (33 bytes) until NACK
    // Returns the actual length read in read_len for the caller to trim
    const size_t extra_read_len = (flags & nv::i2c::I2cFlags::SmbusPec) ? 1U : 0U;
    const size_t smbus_read_len = MaxSMBusBlockReadLength + extra_read_len;
    if (read_buffer.size() < smbus_read_len) {
        nv::error("read buffer size is too small for SMBus Block Read\n");
        return I2cStatus::Error;
    }

    if (write_buffer.size() > 0) {
        auto status = _driver.i2c_write(address, write_buffer, nv::i2c::I2cFlags::NoStop);
        if (status != I2cStatus::Ok) {
            return status;
        }

        status = wait_for_i2c_completion();
        _event.clear(CtrlWaitEvents);
        if (status != I2cStatus::Ok) {
            return status;
        }
    }

    auto item   = read_buffer.subspan(0, smbus_read_len);
    auto status = _driver.i2c_read(
        address, item, (flags & ~(nv::i2c::I2cFlags::RecvLen | nv::i2c::I2cFlags::SmbusPec)));
    if (status != I2cStatus::Ok) {
        return status;
    }

    status = wait_for_i2c_completion();

    if (read_buffer[0] > SmbusBlockReadLength) {
        status = I2cStatus::Error;
    }

    if (status == I2cStatus::Ok) {
        read_len = read_buffer[0] + 1 + extra_read_len;
    }

    if (status == I2cStatus::Ok && (flags & nv::i2c::I2cFlags::NoStop)
        && _repeated_start_timer.id() != ipc::TimerId::End) {
        _repeated_start_timer.reset();
    }

    return status;
}

I2cStatus Task::i2c_cmd(uint8_t            address,
                        std::span<uint8_t> write_buffer,
                        std::span<uint8_t> read_buffer,
                        I2cFlags           flags,
                        size_t&            read_len)
{
    // Check if error injection is enabled for this port (I2C protocol)
    if constexpr (nv::ipc::EnableI2CErrorInjection) {
        if (nv::i2c::should_inject_error(
                _port, address, 0, static_cast<uint8_t>(nv::i2c::ProtocolType::I2c))) {
            return nv::i2c::get_injected_error_status(
                _port, static_cast<uint8_t>(nv::i2c::ProtocolType::I2c));
        }
    }

    if constexpr (nv::lstp::EnableI2c) {  // Non-blocking I2C implementation
        if (_repeated_start_timer.id() != ipc::TimerId::End) {
            _repeated_start_timer.stop();
        }

        _event.clear(CtrlWaitEvents);

        if (flags & nv::i2c::I2cFlags::RecvLen) {
            return smbus_block_read_wrapper(
                address, write_buffer, read_buffer, flags, read_len);
        }

        //  If both write and read are requested, use write-read operation
        if (write_buffer.size() > 0 && read_buffer.size() > 0) {
            // Note: There is significant delay for events to wake up task (around 100us)
            // Blocking approach has negligable delay
            // return sys::i2c::i2c_write_read(_port, address, write_buffer, read_buffer);
            auto status = _driver.i2c_write(address, write_buffer, nv::i2c::I2cFlags::NoStop);
            if (status != I2cStatus::Ok) {
                return status;
            }
            status = wait_for_i2c_completion();
            _event.clear(CtrlWaitEvents);
            if (status != I2cStatus::Ok) {
                return status;
            }

            status = _driver.i2c_read(address, read_buffer);
            if (status != I2cStatus::Ok) {
                return status;
            }
            return wait_for_i2c_completion();
        }

        // Write only operation
        if ((write_buffer.size() > 0) || (flags & nv::i2c::I2cFlags::QuickWrite)) {
            auto status = _driver.i2c_write(address, write_buffer, flags);
            if (status != I2cStatus::Ok) {
                return status;
            }
            status = wait_for_i2c_completion();
            if (status == I2cStatus::Ok && (flags & nv::i2c::I2cFlags::NoStop)
                && _repeated_start_timer.id() != ipc::TimerId::End) {
                _repeated_start_timer.reset();
            }
            return status;
        }

        // Read only operation
        if ((read_buffer.size() > 0) || (flags & nv::i2c::I2cFlags::QuickRead)) {
            auto status = _driver.i2c_read(address, read_buffer, flags);
            if (status != I2cStatus::Ok) {
                return status;
            }
            status = wait_for_i2c_completion();
            if (status == I2cStatus::Ok && (flags & nv::i2c::I2cFlags::NoStop)
                && _repeated_start_timer.id() != ipc::TimerId::End) {
                _repeated_start_timer.reset();
            }
            return status;
        }
    }
    else {  // Legacy Blocking I2C implementation
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
    }
    return I2cStatus::Ok;
}

void Task::handle_eeprom_update()
{
    if constexpr (nv::ipc::EnableEepromBridge) {
        if (_port == nv::ipc::EepromDstPort) {
            auto& cache = eeprom_cache();
            if (cache.has_dirty_pages()) {
                cache.sync_one_page();
            }
        }
    }
}

// Handle EEPROM request via cache
// Note: Only called when EnableEepromBridge is true and address matches EepromDstAddress
bool Task::handle_eeprom_cache_request(const I2cRequest&  request,
                                       std::span<uint8_t> read_buffer,
                                       size_t             write_len,
                                       size_t             read_len)
{
    if (_port != nv::ipc::EepromDstPort) {
        return false;
    }
    auto& cache = eeprom_cache();

    // Handle write data (address bytes + optional data)
    if (write_len > 0) {
        if (cache.use_2byte_addr()) {
            // 2-byte addressing
            if (write_len >= 2) {
                const uint16_t addr = (static_cast<uint16_t>(request.write_buffer[0]) << 8)
                                    | request.write_buffer[1];
                cache.set_addr_ptr(addr);
                // Write data bytes (after 2-byte address)
                for (size_t i = 2; i < write_len; i++) {
                    cache.write(cache.get_addr_ptr(), request.write_buffer.at(i));
                    cache.inc_addr_ptr();
                }
            }
            else if (write_len == 1) {
                cache.set_addr_ptr(request.write_buffer[0]);
            }
        }
        else {
            // 1-byte addressing
            cache.set_addr_ptr(request.write_buffer[0]);
            // Write data bytes (after 1-byte address)
            for (size_t i = 1; i < write_len; i++) {
                cache.write(cache.get_addr_ptr(), request.write_buffer.at(i));
                cache.inc_addr_ptr();
            }
        }
    }

    // Handle read data
    for (size_t i = 0; i < read_len; i++) {
        read_buffer[i] = cache.read(cache.get_addr_ptr());
        cache.inc_addr_ptr();
    }

    // Send response
    std::span<uint8_t> item(read_buffer.data(), read_buffer.size());
    if (request.src_id == static_cast<uint8_t>(ipchandler::Id::Usb)) {
        usb::Task::to_usb(this->_ipchandler_id, read_len, item, I2cStatus::Ok);
    }

    return true;
}
void Task::handle_i2c_request(std::span<uint8_t> buffer)
{
    nv::i2c::I2cHidBuffer read_buffer{};
    nv::i2c::I2cRequest   request{};
    const size_t          RequestSize = std::min(sizeof(nv::i2c::I2cRequest), buffer.size());
    std::memcpy(&request, buffer.data(), RequestSize);
    const size_t WriteLength = std::min(static_cast<size_t>(request.write_length),
                                        request.write_buffer.size());
    size_t ReadLength = std::min(static_cast<size_t>(request.read_length), read_buffer.size());
    auto   write_buffer_span = std::span<uint8_t>(request.write_buffer.data(), WriteLength);
    auto   read_buffer_span  = std::span<uint8_t>(read_buffer.data(), ReadLength);

    // Try EEPROM cache for USB requests only (sync requests must go to real EEPROM)
    if constexpr (nv::ipc::EnableEepromBridge) {
        if (request.address == nv::ipc::EepromDstAddress
            && request.src_id == static_cast<uint8_t>(ipchandler::Id::Usb)) {
            const std::span<uint8_t> read_span(read_buffer.data(), read_buffer.size());
            if (handle_eeprom_cache_request(request, read_span, WriteLength, ReadLength)) {
                return;
            }
        }
    }

    // Normal I2C transaction
    auto result = i2c_cmd(
        request.address, write_buffer_span, read_buffer_span, request.flags, ReadLength);

    if (result != I2cStatus::Ok) {
        // Don't log NAKs for LSTP I2C to avoid spam during i2cdetect
        // CP2112 typically uses manual NACK so we don't need the same
        if (request.src_id != static_cast<uint8_t>(ipchandler::Id::Lstp)) {
            handle_error(result);
        }
    }

    std::span<uint8_t> item(read_buffer.data(), read_buffer.size());

    if constexpr (nv::ipc::EnableI2CErrorInjection) {
        if (nv::i2c::should_inject_error(
                _port, request.address, 0, static_cast<uint8_t>(nv::i2c::ProtocolType::I2c))) {
            const auto index = nv::i2c::port_to_error_injection_index(
                _port, static_cast<uint8_t>(nv::i2c::ProtocolType::I2c));
            const auto& configs = nv::i2c::get_error_injection_configs();
            if (index < configs.size() && configs.at(index).enabled) {
                // Check if error type is UsbQueueFull or Timeout - both should drop the packet
                if (configs.at(index).error_type
                    == static_cast<uint8_t>(nv::i2c::ErrorInjectionType::UsbQueueFull)) {
                    nv::info("USB queue full simulation - skipping to_usb call p:%d\n",
                             (uint8_t)_port);
                    return;
                }
            }
        }
    }

    switch (request.src_id) {
        case static_cast<uint8_t>(ipchandler::Id::Usb):
            usb::Task::to_usb(this->_ipchandler_id, ReadLength, item, result);
            break;
        case static_cast<uint8_t>(ipchandler::Id::Lstp):
            if constexpr (nv::lstp::EnableI2c) {
                nv::lstp::LstpRouter::send_i2c(this->_ipchandler_id, ReadLength, item, result);
            }
            break;
        default: break;
    }
}

void Task::handle_i2c_response(std::span<uint8_t> buffer)
{
    [[maybe_unused]] auto response = std::bit_cast<nv::i2c::I2cResponse*>(buffer.data());
}

void Task::handle_i2c_recovery(std::span<uint8_t> buffer)
{
    nv::i2c::I2cRecoveryRequest request = {};
    std::memcpy(&request, buffer.data(), sizeof(I2cRecoveryRequest));

    // Execute the recovery operation
    switch (request.cmd) {
        case nv::i2c::RecoveryCmd::QuickRecovery:
            nv::i2c::quick_recovery(_port, _target_addr);
            break;
        case nv::i2c::RecoveryCmd::BusRecovery: nv::i2c::bus_recovery(_port); break;
        case nv::i2c::RecoveryCmd::ControllerReinit:
            if constexpr (nv::i2c::EnableI2cPeripheralRecovery) {
                _driver.peripheral_recovery(is_target_client(_client));
            }
            break;
        case nv::i2c::RecoveryCmd::None:
        default                        : break;
    }
}

Task::Status Task::set_event(Event event)
{
    auto       status = _event.set(common::to_underlying(event));
    const bool isr    = sys::ipc::is_in_isr();
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
    if (buffer[0] < 1 || buffer[0] >= buffer.size()) {
        return false;
    }
    constexpr uint8_t ByteMask = 0xFF;
    length                     = static_cast<uint8_t>((buffer[0] - 1U) & ByteMask);
    auto payload               = buffer.subspan(1, length);
    auto pec                   = buffer[buffer[0]];
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
        nv::info("byte count field is invalid (dst_eid=%d)\n", mctp_packet.hdr.dst_eid);
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
                auto& ctl_pkt = mctp::Control::PktReq::from(packet);
                logger::info(logger::Event::MctpMcuActAsBridgePacketNotify,
                             {static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                              static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                              packet.priv.packet_interface,
                              static_cast<uint8_t>(ctl_pkt.command_code),
                              ctl_pkt.data[1]});
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
                case nv::mctp::Client::DsI2c3:
                case nv::mctp::Client::DsI2c4:
                case nv::mctp::Client::DsI2c5:
                case nv::mctp::Client::DsI2c6:
                case nv::mctp::Client::DsI2c7: {
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
        status = mctp::Driver::mctp_send(item, _client);
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
    auto status = I2cStatus::Ok;
    _event.clear(CtrlWaitEvents);
    for (auto i = 0; i < Retry; i++) {
        if (!_driver.write(buffer)) {
            set_event(Task::Event::CtrlBusBusy);
        }
        // TODO driver hung, should we reset it?
        status = wait_for_i2c_completion();
        if (status == I2cStatus::Ok || !is_retryable(status)) {
            break;
        }
        delay(10ms);
    }
    if (status != I2cStatus::Ok) {
        handle_error(status);
    }
    perf_mon::Driver::log_pkt_meas(
        0, static_cast<uint32_t>(buffer.size() & UINT8_MAX), false, status != I2cStatus::Ok);

    return status == I2cStatus::Ok;
}

void Task::handle_error(I2cStatus status, uint8_t src_id)
{
    if (status == I2cStatus::Ok) {
        return;
    }

    if constexpr (nv::i2c::EnableI2cPeripheralRecovery) {
        if (status == I2cStatus::Busy || status == I2cStatus::ArbLost) {
            _driver.peripheral_recovery(is_target_client(_client));
        }
    }

    if (status == I2cStatus::PinLowTimeout) {
        nv::i2c::bus_recovery(_port);
    }

    const uint8_t ByteMask = 0xFF;
    // NOLINTNEXTLINE(misc-const-correctness)
    nv::logger::EventData data{static_cast<uint8_t>(static_cast<uint16_t>(_client) & ByteMask),
                               static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                               static_cast<uint8_t>(status),
                               src_id,
                               static_cast<uint8_t>(id()),
                               static_cast<uint8_t>(_port)};
    nv::logger::error(nv::logger::Event::I2CError, data);

    nv::perf_mon::Driver::set_transaction_error(_oob_bus, static_cast<uint8_t>(status));
}

bool Task::to_i2c(ipchandler::Id    src_id,
                  uint8_t           address,
                  ipchandler::Id    i2c_ipchandler_id,
                  uint16_t          write_length,
                  uint16_t          read_length,
                  I2cFlags          flags,
                  ipc::Queue::Item& item)
{
    // Validate lengths against buffer limits
    if (write_length > I2cBufferSize) {
        return false;
    }
    if (read_length > I2cBufferSize) {
        return false;
    }

    // Check for queue full error injection
    if constexpr (nv::ipc::EnableI2CErrorInjection) {
        if (nv::i2c::should_inject_error(i2c_ipchandler_id,
                                         address,
                                         0,
                                         static_cast<uint8_t>(nv::i2c::ProtocolType::I2c))) {
            const Port port  = nv::i2c::ipchandler_to_error_injection_port(i2c_ipchandler_id);
            const auto index = nv::i2c::port_to_error_injection_index(
                port, static_cast<uint8_t>(nv::i2c::ProtocolType::I2c));
            const auto& configs = nv::i2c::get_error_injection_configs();
            if (index < configs.size()
                && configs.at(index).error_type
                       == static_cast<uint8_t>(nv::i2c::ErrorInjectionType::QueueFull)) {
                nv::info("I2C queue full error injected for port %d\n", static_cast<int>(port));
                return false;  // Simulate queue full by returning false
            }
        }
    }

    // Create request
    Task::Request request{};
    request.type = Task::RequestType::I2cRequest;

    // Set up I2C request data
    auto* i2c_request         = std::bit_cast<I2cRequest*>(static_cast<void*>(request.data));
    i2c_request->address      = address;
    i2c_request->write_length = write_length;
    i2c_request->read_length  = read_length;
    i2c_request->src_id       = static_cast<uint8_t>(src_id);
    i2c_request->flags        = flags;

    // Copy write data if any
    if (write_length > 0) {
        std::copy_n(item.begin(), write_length, i2c_request->write_buffer.begin());
    }

    // Create queue item with the full request size
    auto request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));

    // Send to IPC handler
    auto status = ipchandler::Driver::send(
        src_id, i2c_ipchandler_id, request_item, sizeof(request), true);

    if (status != ipchandler::Status::Success) {
        return false;
    }

    return true;
}

bool Task::to_i2c_from_isr(ipchandler::Id    src_id,
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
            return false;
        }
        std::copy_n(item.begin(), write_length, i2c_request->write_buffer.begin());
    }

    // Map ipchandler_id to queue_id
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    nv::ipc::QueueId queue_id = nv::ipc::QueueId::End;
    switch (i2c_ipchandler_id) {
        case ipchandler::Id::I2c1: queue_id = nv::ipc::QueueId::I2c1; break;
        case ipchandler::Id::I2c2: queue_id = nv::ipc::QueueId::I2c2; break;
        case ipchandler::Id::I2c3: queue_id = nv::ipc::QueueId::I2c3; break;
        case ipchandler::Id::I2c4: queue_id = nv::ipc::QueueId::I2c4; break;
        case ipchandler::Id::I2c5: queue_id = nv::ipc::QueueId::I2c5; break;
        case ipchandler::Id::I2c6: queue_id = nv::ipc::QueueId::I2c6; break;
        case ipchandler::Id::I2c7: queue_id = nv::ipc::QueueId::I2c7; break;
        case ipchandler::Id::I2c8: queue_id = nv::ipc::QueueId::I2c8; break;
        case ipchandler::Id::I2c9: queue_id = nv::ipc::QueueId::I2c9; break;
        default                  : return false;
    }

    // Create queue item with the full request size
    auto request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));

    // Send to queue using ISR-safe API
    nv::ipc::Queue& queue  = nv::ipc::Queue::make(queue_id);
    auto            status = queue.send_isr(request_item);

    if (status != nv::ipc::Queue::Status::Ok) {
        return false;
    }

    return true;
}

void Task::request_i2c_loopback_test()
{
    const Task::Request request{.type = RequestType::I2cLoopbackTest};
    auto item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    (void)nv::ipc::Queue::make(nv::ipc::QueueId::I2c0).send(item, std::chrono::seconds(0));
}

void Task::check_target_timeout([[maybe_unused]] nv::ipc::Timer& timer)
{
    if constexpr (nv::i2c::EnableI2cPeripheralRecovery) {
        if constexpr (nv::i2c::I2CTargetTimeoutCheckIntervalMs > 0) {
            using namespace nv::ipc;
            using namespace std::chrono_literals;
            if (timer.id() != TimerId::I2CTargetTimeoutCheck) {
                return;
            }
            const Request request{.type = RequestType::CheckTargetTimeout};
            for (const auto client : nv::i2c::I2CTargetTimeoutCheckClients) {
                const auto queue_id = client_to_i2c_queue_id(client);
                if (queue_id == QueueId::End) {
                    continue;
                }
                auto item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
                auto queue_status = Queue::make(queue_id).send(item, 0s);
                if (queue_status != Queue::Status::Ok) {
                    /// drop event because task is busy
                }
            }
        }
    }
}

void Task::handle_check_target_timeout()
{
    if constexpr (nv::i2c::EnableI2cPeripheralRecovery) {
        if constexpr (nv::i2c::I2CTargetTimeoutCheckIntervalMs > 0) {
            _driver.check_target_timeout(is_target_client(_client));
        }
    }
}

void Task::check_ap_status(nv::ipc::Timer& timer)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    QueueId       queue_id = QueueId::End;
    const Request request{.type = RequestType::CheckApStatus};
    queue_id = projectGetApStatusQueueId(timer);
    if (queue_id == QueueId::End) {
        return;
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
        if (projectIsApStatusTimer(_timer)) {
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
        if (projectIsApStatusTimer(_timer)) {
            if (status == APStatus::ApUpNotEnumerated || status == APStatus::ApDown) {
                nv::mctp::Driver::endpoint_status_change(
                    myep, (status == APStatus::ApUpNotEnumerated));
            }
        }
        _ap_status = status;
    }
}

// Weak function definition for projectTryRunAdcTrigger (for testrunner project(esp. in
// simulation))
__attribute__((weak)) bool projectIsApStatusTimer(nv::ipc::Timer& timer)
{
    switch (timer.id()) {
        case nv::ipc::TimerId::Ap1Status: return true;
        case nv::ipc::TimerId::Ap2Status: return true;
        case nv::ipc::TimerId::Ap3Status: return true;
        default                         : return false;
    }
    return false;
}

// Weak function definition for projectTryRunAdcTrigger (for testrunner project(esp. in
// simulation))
__attribute__((weak)) nv::ipc::QueueId projectGetApStatusQueueId(nv::ipc::Timer& timer)
{
    switch (timer.id()) {
        case nv::ipc::TimerId::Ap1Status: return nv::ipc::QueueId::I2c1;
        case nv::ipc::TimerId::Ap2Status: return nv::ipc::QueueId::I2c2;
        case nv::ipc::TimerId::Ap3Status: return nv::ipc::QueueId::I2c3;
        default                         : return nv::ipc::QueueId::End;
    }
}

// Weak function definition for project_stop_polling_timers (for testrunner project(esp. in
// simulation))
__attribute__((weak)) void project_stop_polling_timers() {}

// EROT Recovery handler.
// Recovery sequence (per EROT firmware spec):
//   S <ErotAddr> Wr 0x00 0x00 P      (status -> down)
//   wait ~10 ms
//   S <ErotAddr> Wr 0x00 0x01 P      (status -> up)
void Task::handle_erot_recovery(uint8_t reg, uint8_t value)
{
    if (reg != 0x00) {
        return;
    }
    if (value != 0x00 && value != 0x01) {
        return;
    }

    size_t endpoint_index = nv::ipc::DownStreamInfos.size();
    for (size_t i = 0; i < nv::ipc::DownStreamInfos.size(); ++i) {
        if (nv::ipc::DownStreamInfos.at(i).client == _client) {
            endpoint_index = i;
            break;
        }
    }
    if (endpoint_index >= nv::ipc::DownStreamInfos.size()) {
        return;
    }

    nv::mctp::Driver::endpoint_status_change(static_cast<uint8_t>(endpoint_index), value);
}
