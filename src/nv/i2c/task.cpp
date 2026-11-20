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
#include "nv/i2c/i2c_forward.h"
#include "nv/i2c/eeprom_cache.h"
#include "nv/i2c/error_injection.h"
#include "nv/i2c/helper.h"
#include "nv/i2c/recovery.h"
#include "nv/i2c/sensor.h"
#include "nv/i2c/vr_i2c_task.h"
#include "nv/i3c/task.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/log.h"
#include "nv/lstp/lstp_router.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/usb/task.h"
#include "nv/volt_mon/mcu_internal_temp.h"
#include "nv/watchdog/runtime.h"
#include "sys/i2c/utils.h"
#include "sys/sensor/sensor.h"

#include NV_IPC_CONFIG_H

extern bool             projectIsApStatusTimer(nv::ipc::Timer& timer);
extern nv::ipc::QueueId projectGetApStatusQueueId(nv::ipc::Timer& timer);

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

/// Inbox this task blocks on: client, else ipchandler, else task. Never `End`.
/// Boards may pin a task with `i2c_queue_proj_override(task_id, QueueId::End)`.
template<typename ConfigT = Task::Config>
constexpr nv::ipc::QueueId primary_queue(const ConfigT& config)
{
    const auto queue_id = [&config] {
        if constexpr (requires {
                          i2c_queue_proj_override(config.task_id, nv::ipc::QueueId::End);
                      }) {
            const auto pinned = i2c_queue_proj_override(config.task_id, nv::ipc::QueueId::End);
            if (pinned != nv::ipc::QueueId::End) {
                return pinned;
            }
        }
        if (config.client != nv::mctp::Client::End) {
            return client_to_i2c_queue_id(config.client);
        }
        if (config.ipchandler_id != nv::ipchandler::Id::Unuse) {
            return ipchandler_to_i2c_queue(config.ipchandler_id);
        }
        return task_to_i2c_queue(config.task_id);
    }();
    NV_ALWAYS_ASSERT(queue_id != nv::ipc::QueueId::End);
    return queue_id;
}

/// A port can own a hardware target without being an MCTP endpoint. A SlaveFunctionTable entry
/// is served entirely from the target ISR, so the target must be armed for those ports too even
/// when no MCTP client is bound to them. An emulation entry only counts when the associated
/// task actually serves its address.
bool has_slave_function(nv::i2c::Port port, const nv::i2c::VrI2cTask* emul_task)
{
    for (const auto& entry : nv::ipc::SlaveFunctionTable) {
        if (entry.port == port && entry.enabled
            && (entry.function != nv::i2c::SlaveFunction::Emulation
                || (emul_task != nullptr && emul_task->has_device(entry.address)))) {
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
, _ap_status_client(config.ap_status_client != nv::mctp::Client::End ? config.ap_status_client
                                                                     : config.client)
, _queue_id(primary_queue(config))
, _port(config.port_id)
, _driver()
, _eid_addr_map()
, _target_addr(config.target_addr)
, _boot_event(config.boot_event)
, _ipchandler_id(config.ipchandler_id)
, _plain_status_probe(config.plain_status_probe)
, _probe_ack_is_down(config.probe_ack_is_down)
, _ap_probe_addr(config.ap_probe_addr)
, _oob_bus(nv::perf_mon::Driver::flexcomm_port_to_oobBus(static_cast<uint8_t>(config.port_id)))
{
    (void)nv::ipc::Queue::make(_queue_id);

    std::fill(_eid_addr_map.begin(), _eid_addr_map.end(), 0);
    nv::i2c::VrI2cTask* emul_task = nullptr;
    if (config.emul_task_id != nv::ipc::TaskId::End) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        emul_task = &static_cast<nv::i2c::VrI2cTask&>(
            nv::ipc::Supervisor::inst().task(config.emul_task_id));
    }
    _driver.bind(config.port_id, this, emul_task);
    _driver.init();

    _has_slave_function = has_slave_function(config.port_id, emul_task);

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
    // Register the AP-status probe timer either when the project globally routes AP-status
    // probing through the I2C tasks (UseI2cApStatusTimer), or when this specific task owns a
    // dedicated liveness probe address (ap_probe_addr != 0). The latter lets a single task
    // (e.g. an EROT recovery probe) run its own probe even on projects that keep
    // UseI2cApStatusTimer disabled because another bus (e.g. UsI3c) owns the generic timer.
    else if ((ipc::UseI2cApStatusTimer || config.ap_probe_addr != 0)
             && config.timer_id != ipc::TimerId::End) {
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

nv::ipc::Queue::Status Task::send_request(nv::ipc::QueueId      queue_id,
                                          const Request&        request,
                                          nv::ipc::Queue::Usecs timeout,
                                          bool                  is_isr)
{
    const auto item = nv::ipc::Queue::ConstItem(std::bit_cast<const uint8_t*>(&request),
                                                sizeof(request));
    // ISR must not construct; supervisor/ctor create the queue first.
    auto& queue = is_isr ? nv::ipc::Queue::get(queue_id) : nv::ipc::Queue::make(queue_id);
    return is_isr ? queue.send_isr(item) : queue.send(item, timeout);
}

Task::Status Task::tx(const nv::mctp::Packet& packet, uint16_t additional_info)
{
    using namespace nv::ipc;
    const auto queue_id = client_to_i2c_queue_id(
        static_cast<nv::mctp::Client>(packet.priv.packet_interface));
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
    auto queue_status = send_request(queue_id, request);
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
    const auto queue_id = client_to_i2c_queue_id(client);
    if (queue_id == QueueId::End) {
        return Task::Status::InvalidInterface;
    }
    const Request RequestPkt{.type = RequestType::MctpUpdateRoutingTable};
    auto          queue_status = send_request(queue_id, RequestPkt);
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
    const auto queue_id = wdt_to_i2c_queue(taskId);
    if (queue_id == QueueId::End) {
        return Task::Status::InvalidInterface;
    }
    const Request RequestPkt{.type = RequestType::WdtEvent};
    auto          queue_status = send_request(queue_id, RequestPkt, 0s);
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

    const auto queue_id = client_to_i2c_queue_id(client);
    if (queue_id == QueueId::End) {
        return Task::Status::InvalidInterface;
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
    auto queue_status = send_request(queue_id, request, Queue::Usecs::max(), is_in_isr);

    if (queue_status != Queue::Status::Ok) {
        return Task::Status::FailToPutTxPacket;
    }

    return Task::Status::Ok;
}

Task::Status Task::send_vr_fault_dump(nv::ipc::QueueId queue_id)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;

    if (queue_id == QueueId::End) {
        return Task::Status::InvalidInterface;
    }

    const Request request{.type = RequestType::VrFaultDump};
    auto          queue_status = send_request(queue_id, request, 0s, Supervisor::is_in_isr());
    if (queue_status != Queue::Status::Ok) {
        return Task::Status::FailToPutTxPacket;
    }
    return Task::Status::Ok;
}

void Task::update_sensor(nv::ipc::Timer& timer)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;
    const auto queue_id = i2c_queue(0);
    if (timer.id() != TimerId::SmbSensor || queue_id == QueueId::End) {
        return;
    }
    const Request request{.type = RequestType::UpdateSensor};
    auto          queue_status = send_request(queue_id, request, 0s);
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
        recovery.src_id = static_cast<uint8_t>(ipchandler::Id::Unuse);
        std::memcpy(static_cast<void*>(request.data), &recovery, sizeof(I2cRecoveryRequest));

        auto queue_status = send_request(queue_id, request, 0s);
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
        (void)send_request(EepromI2cQueueId, request, 0s);
    }
}

void Task::start()
{
    const bool enable_target = is_target_client(_client) || _has_slave_function;
    _driver.start(enable_target);

    using namespace nv::ipc;
    Request request{};
    auto    item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    if (_timer.id() != ipc::TimerId::End) {
        if (_port == i2c::Port::Zero && _timer.id() == ipc::TimerId::SmbSensor) {
            set_tmp_sensor();
        }
        // Port Zero (SmbSensor) and AP-status timers on any port (e.g. Port One PN72f,
        // Port Six ERoT). Routing-table bootstrap cannot start the timer while
        // _ap_status is still Querying. is_erot_ap_probe() is a subset of this.
        if (_port == i2c::Port::Zero || projectIsApStatusTimer(_timer)) {
            _timer.start();
        }
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
    auto& queue = Queue::make(_queue_id);
    while (true) {
        auto status = queue.recv(item);
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
            case RequestType::EepromUpdate          : handle_eeprom_update(); break;
            case RequestType::CheckTargetTimeout    : handle_check_target_timeout(); break;
            case RequestType::VrFaultDump           : poll_vr_fault_dump(_port, _bus_idle); break;
        }
    }
}

void Task::handle_wdt_event()
{
    const auto index = i2c_queue_to_wdt(_queue_id);
    if (index == watchdog::TaskMonitorIndex::End) {
        return;
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
    auto queue_status = send_request(_queue_id, request, Queue::Usecs::max(), true);
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
    auto    queue_status = send_request(_queue_id, request, Queue::Usecs::max(), true);
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
            // Gate the AP-status endpoint, which may differ from this task's data-path
            // client (e.g. EROT: probe over I2C but the MCTP endpoint is on Spi0, so
            // _client == End and _ap_status_client == Spi0). Using _client here would look
            // up the wrong routing entry and never see the EROT's enumerated state.
            auto  myep    = nv::mctp::Control::get_routing_index(_ap_status_client);
            auto& myentry = _routing_table.at(myep);
            if (myentry.is_enumerated) {
                // Set EID success
                handle_ap_status(APStatus::ApUpEnumerated);
            }
            else if (_ap_status != APStatus::Querying && myentry.is_set_eid_sent) {
                // Stay Querying until the first ping. ApDown only after Set EID was
                // sent and this slot is still not enumerated. A slot held down on
                // purpose never sets is_set_eid_sent, so a routing-table broadcast
                // from another endpoint leaves its AP status alone.
                handle_ap_status(APStatus::ApDown);
            }
        }
    }
}

I2cStatus Task::wait_for_i2c_completion()
{
    using namespace std::chrono_literals;
    auto notification = nv::ipc::TaskNotify::wait(1000ms, true);
    if (!notification) {
        return notification.error() == nv::ipc::TaskNotify::Status::Timeout ? I2cStatus::Timeout
                                                                            : I2cStatus::Error;
    }

    switch (notification.value()) {
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

I2cStatus Task::hold_or_idle(I2cStatus status, I2cFlags flags)
{
    if (status != I2cStatus::Ok) {
        return status;
    }
    if (flags & nv::i2c::I2cFlags::NoStop) {
        if (_repeated_start_timer.id() != ipc::TimerId::End) {
            _repeated_start_timer.reset();
        }
    }
    else {
        _bus_idle = true;
    }
    return status;
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

    return hold_or_idle(status, flags);
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

        // A transaction is starting: the bus is no longer known idle. It is marked idle
        // again only on a STOP-terminated success below (or recovered to idle on error by
        // the caller), so the AP-status ping cannot intrude mid-transaction.
        _bus_idle = false;

        if (flags & nv::i2c::I2cFlags::RecvLen) {
            return smbus_block_read_wrapper(
                address, write_buffer, read_buffer, flags, read_len);
        }

        //  If both write and read are requested, use write-read operation
        if (write_buffer.size() > 0 && read_buffer.size() > 0) {
            auto status = _driver.i2c_write(address, write_buffer, nv::i2c::I2cFlags::NoStop);
            if (status != I2cStatus::Ok) {
                return status;
            }
            status = wait_for_i2c_completion();
            if (status != I2cStatus::Ok) {
                return status;
            }

            status = _driver.i2c_read(address, read_buffer, flags);
            if (status != I2cStatus::Ok) {
                return status;
            }
            return hold_or_idle(wait_for_i2c_completion(), flags);
        }

        // Write only operation
        if ((write_buffer.size() > 0) || (flags & nv::i2c::I2cFlags::QuickWrite)) {
            auto status = _driver.i2c_write(address, write_buffer, flags);
            if (status != I2cStatus::Ok) {
                return status;
            }
            return hold_or_idle(wait_for_i2c_completion(), flags);
        }

        // Read only operation
        if ((read_buffer.size() > 0) || (flags & nv::i2c::I2cFlags::QuickRead)) {
            auto status = _driver.i2c_read(address, read_buffer, flags);
            if (status != I2cStatus::Ok) {
                return status;
            }
            return hold_or_idle(wait_for_i2c_completion(), flags);
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

    // Do not let the MCU answer for its own I2C target address on LSTP-bridged
    // scans. These downstream buses run the same Flexcomm as both master (LSTP
    // bridge) and MCTP-over-SMBus target, so the controller self-ACKs its own
    // slave address and the MCU shows up in the BMC's i2cdetect
    if constexpr (nv::lstp::EnableI2c) {
        if (request.src_id == static_cast<uint8_t>(ipchandler::Id::Lstp)
            && is_target_client(_client) && request.address == _driver.address()) {
            std::span<uint8_t> item(read_buffer.data(), read_buffer.size());
            nv::lstp::LstpRouter::send_i2c(this->_ipchandler_id, 0, item, I2cStatus::Nak);
            return;
        }
    }

    // Normal I2C transaction
    auto result = i2c_cmd(
        request.address, write_buffer_span, read_buffer_span, request.flags, ReadLength);

    if (result != I2cStatus::Ok) {
        // For LSTP I2C, suppress NACK/BUSY to avoid spam during i2cdetect scans
        // (CP2112 typically uses manual NACK); still log genuine bus faults.
        const bool is_lstp    = request.src_id == static_cast<uint8_t>(ipchandler::Id::Lstp);
        const bool scan_noise = result == I2cStatus::Nak || result == I2cStatus::Busy;
        if (!is_lstp || !scan_noise) {
            handle_error(result, request.src_id);
        }
        // A NAK is STOP-terminated by the controller, so the bus is idle again. Any other
        // failure may have left a NoStop sequence held, so keep the bus marked busy (as
        // i2c_cmd() left it) and let a later transaction or an explicit RecoveryCmd settle
        // it. Do not issue a recovery transfer here: quick_recovery() is itself a NoStop
        // write-read that early-returns without a STOP, so on a bus that just failed it can
        // hold the bus instead of settling it, and it drives an extra addressed transaction
        // into a target that may be mid-protocol.
        if (result == I2cStatus::Nak) {
            _bus_idle = true;
        }
    }

    // EROT recovery runs as a stateful multi-transaction Glacier upload over LSTP to the EROT
    // (_target_addr, 0x62). The liveness probe targets the SAME physical EROT (it ACKs at its
    // recovery-mode address 0x63 while in recovery), so a probe issued in the gap between two
    // recovery command transactions disturbs the EROT's recovery state machine and the upload
    // fails ("CRC check failed on the command parameters"). Defer the periodic probe on every
    // EROT LSTP transaction so it only fires after the bus has been quiet for a full
    // CheckApStatusTimerUs period; liveness itself still comes solely from the 0x63 probe (no
    // liveness is derived from the LSTP result here). Restricted to EROT-addressed LSTP
    // traffic: other addresses on this bus (e.g. i2cdetect scans) do not defer it.
    //
    // Gated on _ap_probe_addr != 0 so this only applies to the decoupled recovery-mode probe
    // setup (EROT: probe address separate from the LSTP/device address). Products that share
    // one address for probe and LSTP/device (_ap_probe_addr == 0) keep their
    // original behavior and never defer the AP-status timer on LSTP traffic.
    if (request.src_id == static_cast<uint8_t>(ipchandler::Id::Lstp) && _target_addr != 0
        && request.address == _target_addr && _ap_probe_addr != 0
        && projectIsApStatusTimer(_timer)) {
        _timer.reset();
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
        case static_cast<uint8_t>(ipchandler::Id::I2cForward):
            /// this task owns the destination bus, so the shadow is its own
            forward::on_forward_result(_forward_state, result, request);
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

    I2cStatus result = I2cStatus::Ok;

    // Execute the recovery operation
    switch (request.cmd) {
        case nv::i2c::RecoveryCmd::QuickRecovery:
            nv::i2c::quick_recovery(_port, _target_addr);
            break;
        case nv::i2c::RecoveryCmd::BusRecovery: result = nv::i2c::bus_recovery(_port); break;
        case nv::i2c::RecoveryCmd::ControllerReinit:
            if constexpr (nv::i2c::EnableI2cPeripheralRecovery) {
                _driver.peripheral_recovery(is_target_client(_client));
            }
            break;
        case nv::i2c::RecoveryCmd::None:
        default                        : break;
    }

    // After any recovery the bus has been driven back to a known idle state (e.g. the
    // repeated-start timeout path issues BusRecovery); allow the liveness ping to resume.
    if (request.cmd != nv::i2c::RecoveryCmd::None) {
        _bus_idle = true;
        if (_repeated_start_timer.id() != ipc::TimerId::End) {
            _repeated_start_timer.stop();
        }
    }

    // On-demand LSTP bus recovery expects a status response; internal recoveries do not.
    if constexpr (nv::lstp::EnableI2c) {
        if (request.src_id == static_cast<uint8_t>(ipchandler::Id::Lstp)) {
            nv::i2c::I2cHidBuffer read_buffer{};
            std::span<uint8_t>    item(read_buffer.data(), read_buffer.size());
            nv::lstp::LstpRouter::send_i2c(this->_ipchandler_id, 0, item, result);
        }
    }
    (void)result;
}

Task::Status Task::set_event(Event event)
{
    auto       status = nv::ipc::TaskNotify::send(id(), common::to_underlying(event));
    const bool isr    = sys::ipc::is_in_isr();
    if (status != nv::ipc::TaskNotify::Status::Ok) {
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
        (void)nv::i2c::bus_recovery(_port);
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

bool Task::to_i2c_recovery(ipchandler::Id src_id, ipchandler::Id i2c_ipchandler_id)
{
    Task::Request request{};
    request.type = Task::RequestType::I2cRecovery;

    I2cRecoveryRequest recovery{};
    recovery.cmd    = RecoveryCmd::BusRecovery;
    recovery.src_id = static_cast<uint8_t>(src_id);
    static_assert(sizeof(I2cRecoveryRequest) <= DataSize,
                  "I2cRecoveryRequest size must not exceed Request::data size");
    std::memcpy(static_cast<void*>(request.data), &recovery, sizeof(I2cRecoveryRequest));

    auto       request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));
    const auto status       = ipchandler::Driver::send(
        src_id, i2c_ipchandler_id, request_item, sizeof(request), true);

    return status == ipchandler::Status::Success;
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

    // ISR-originated requests only target the downstream buses (I2c1..I2c9); the
    // upstream bus and the VR buses are not reachable from here.
    if (i2c_ipchandler_id < ipchandler::Id::I2c1 || i2c_ipchandler_id > ipchandler::Id::I2c9) {
        return false;
    }
    const nv::ipc::QueueId queue_id = ipchandler_to_i2c_queue(i2c_ipchandler_id);
    if (queue_id == nv::ipc::QueueId::End) {
        return false;
    }

    const auto status = send_request(queue_id, request, nv::ipc::Queue::Usecs::max(), true);

    if (status != nv::ipc::Queue::Status::Ok) {
        return false;
    }

    return true;
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
                auto queue_status = send_request(queue_id, request, 0s);
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
            _driver.check_target_timeout(is_target_client(_client) || _has_slave_function);
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
    auto queue_status = send_request(queue_id, request, 0s);
    if (queue_status != Queue::Status::Ok) {
        /// drop event because task is busy
    }
}

void Task::handle_ap_status(APStatus set_status)
{
    if (set_status == APStatus::Querying) {
        // Route the periodic liveness probe: the EROT uses a dedicated, bus-idle-gated
        // two-stage probe; every other product uses the original common single-address
        // probe. Keeping these paths separate ensures the EROT changes cannot alter the
        // behavior of the common AP-status check shared by all other products.
        if (is_erot_ap_probe()) {
            handle_erot_liveness();
        }
        else {
            handle_common_ap_status();
        }
        return;
    }

    // Forced status change (e.g. from a routing-table update): reset the status timer since
    // we just resolved the status out of band, then commit the transition.
    if (projectIsApStatusTimer(_timer)) {
        _timer.reset();
    }
    commit_ap_status(set_status);
}

void Task::handle_common_ap_status()
{
    // Original pre-EROT behavior: a single MCTP-over-SMBus GetEndpointID probe on the device
    // address with immediate up/down (down_threshold == 1, no bus-idle gating). Kept separate
    // from the EROT path so other products are completely unaffected by the EROT changes.
    if (_target_addr == 0) {
        return;
    }
    if constexpr (ipc::SuppressApStatusPingWhenDown) {
        // Stay down until erot_recovery or another explicit set.
        if (_ap_status == APStatus::ApDown) {
            return;
        }
    }
    const bool alive = _driver.get_status(_target_addr);
    apply_ap_liveness(alive, /*down_threshold=*/1, /*up_threshold=*/1);
}

void Task::handle_erot_liveness()
{
    // Skip this liveness probe unless the bus is known idle. An LSTP transaction in
    // progress, a held repeated-start (NoStop) window, or an error that left the bus
    // mid-sequence all clear _bus_idle; the armed repeated-start timer is an additional
    // guard for the BMC-driven NoStop window. The probe issues a full START/STOP cycle,
    // which would corrupt a half-finished LSTP block/repeated-start. Retry next tick.
    if (!_bus_idle
        || (_repeated_start_timer.id() != ipc::TimerId::End
            && _repeated_start_timer.enabled())) {
        return;
    }
    // Two-stage EROT liveness probe when a dedicated recovery-mode indicator address
    // (_ap_probe_addr, EROT 0x63) is configured alongside the device address
    // (_target_addr, EROT 0x62):
    //   1. Probe 0x63. ACK => EROT is in recovery mode; its MCTP-over-SPI data path is
    //      DOWN => remove the endpoint.
    //   2. NAK on 0x63 => not in recovery; probe the device address 0x62. ACK => the EROT
    //      is present and its SPI path should work => enumerate the endpoint. NAK => the
    //      EROT is held in reset (not responding at all) => remove the endpoint.
    // The endpoint is up only when 0x63 NAKs and 0x62 ACKs. The 0x62 probe fires only
    // when 0x63 NAKs (never during recovery), so it cannot disturb the Glacier recovery
    // sequence on 0x62.
    const uint8_t probe_addr = (_ap_probe_addr != 0) ? _ap_probe_addr : _target_addr;
    if (probe_addr == 0) {
        return;
    }
    const bool ack   = probe_once(probe_addr);
    bool       alive = false;
    if (_probe_ack_is_down) {
        if (ack) {
            // Recovery-mode address ACKed: EROT is in recovery -> down.
            alive = false;
        }
        else if (_target_addr != 0 && _target_addr != probe_addr) {
            // Not in recovery: confirm the EROT is present on its device address.
            // ACK -> up; NAK -> EROT held in reset -> down.
            alive = probe_once(_target_addr);
        }
        else {
            alive = true;
        }
    }
    else {
        alive = ack;
    }
    apply_ap_liveness(alive, ProbeDownThreshold, ProbeUpThreshold);
}

bool Task::probe_once(uint8_t address)
{
    // Payload-free i2cdetect ping for non-MCTP-over-SMBus targets (e.g. EROT whose MCTP path
    // is on SPI); otherwise the MCTP-over-SMBus GetEndpointID probe. AP-status transitions are
    // recorded by commit_ap_status (I2CApStatusUpdate); a per-probe log is intentionally
    // omitted since it fires every timer tick and floods the log.
    const bool ack = _plain_status_probe ? _driver.ping(address) : _driver.get_status(address);
    return ack;
}

void Task::apply_ap_liveness(bool alive, uint8_t down_threshold, uint8_t up_threshold)
{
    APStatus status = _ap_status;

    if (alive) {
        // Alive: reset the miss counter. If already enumerated, stay up. Otherwise move to
        // ApUpNotEnumerated after the caller-selected number of consecutive alive probes.
        _probe_miss_count = 0;
        if (status == APStatus::ApUpEnumerated) {
            _probe_hit_count = 0;
        }
        else if (++_probe_hit_count >= up_threshold) {
            _probe_hit_count = 0;
            status           = APStatus::ApUpNotEnumerated;
        }
        // else: keep current status and wait for more consecutive alive observations.
    }
    else {
        // A failed observation breaks the consecutive-alive sequence, even if the endpoint
        // is already down.
        _probe_hit_count = 0;
        if (status != APStatus::ApDown) {
            // Missed observation: declare down only after down_threshold consecutive misses.
            // Common products pass 1 (immediate); the EROT probe passes ProbeDownThreshold to
            // debounce, since its liveness bus (I2C) differs from the SPI data path.
            if (++_probe_miss_count >= down_threshold) {
                _probe_miss_count = 0;
                status            = APStatus::ApDown;
            }
            // else: keep current status and retry on the next observation.
        }
    }

    commit_ap_status(status);
}

void Task::commit_ap_status(APStatus status)
{
    // Only act on real status transitions. Retry of a failed enumeration is driven by the
    // ApDown <-> ApUpNotEnumerated state cycle (a failed enumeration is promoted back to
    // ApDown by handle_update_routing_table() once the enum-done window closes, and the
    // next alive probe re-triggers enumeration). This keeps re-enumeration from restarting
    // the driver's in-flight enum-done timer, which would otherwise never complete.
    if (status == _ap_status) {
        return;
    }

    // Gate the AP-status endpoint, which may differ from this task's data-path
    // client (e.g. probe over I2C, gate the EROT's SPI endpoint).
    auto myep = nv::mctp::Control::get_routing_index(_ap_status_client);
    logger::info(logger::Event::I2CApStatusUpdate,
                 {static_cast<uint8_t>(myep),
                  static_cast<uint8_t>(_ap_status),
                  static_cast<uint8_t>(status)});

    // update mctp driver with status for enumeration
    if (projectIsApStatusTimer(_timer)) {
        if (status == APStatus::ApUpNotEnumerated || status == APStatus::ApDown) {
            on_ap_status_endpoint_change(myep, (status == APStatus::ApUpNotEnumerated));
        }
    }
    _ap_status = status;
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
    return nv::i2c::ap_status_timer_to_i2c_queue(timer.id());
}

__attribute__((weak)) void nv::i2c::on_ap_status_endpoint_change(uint8_t endpoint_index,
                                                                 uint8_t status)
{
    nv::mctp::Driver::endpoint_status_change(endpoint_index, status);
}

__attribute__((weak)) void nv::i2c::poll_vr_fault_dump([[maybe_unused]] Port port,
                                                       [[maybe_unused]] bool bus_idle)
{}

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

    if (value == 0x00) {
        handle_ap_status(APStatus::ApDown);
    }
    else {
        handle_ap_status(APStatus::ApUpNotEnumerated);
    }
}
