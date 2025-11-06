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
#include "nv/spi/task.h"

#include "nv/bootloader.h"
#include "nv/nv.h"
#include "sys/ipc/supervisor.h"
#include "nv/ctimer/ctimer.h"
#include "nv/mctp/driver.h"
#include "nv/usb/task.h"
#include "nv/logger/log.h"

#include <cstring>

using namespace nv::spi;

void Task::make(Config config)
{
    // TODO : decide the stack size for spi task
    // TODO : decide the priority of spi
    constexpr auto StackSize = std::max(384, int(configMINIMAL_STACK_SIZE));

    switch (config.port_id) {
        case Port::Zero: {
            NV_TASK_DATA static Task                       task0(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack0;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv0(reinterpret_cast<uint8_t*>(&task0), sizeof(Task));
            task0.setup(stack0.span(), Priv0, Priority::Norm, Task::entrypoint);
            break;
        }
        case Port::One: {
            NV_TASK_DATA static Task                       task1(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack1;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv1(reinterpret_cast<uint8_t*>(&task1), sizeof(Task));
            task1.setup(stack1.span(), Priv1, Priority::Norm, Task::entrypoint);
            break;
        }
        case Port::Two: {
            NV_TASK_DATA static Task                       task2(config);
            NV_STACK static sys::ipc::TaskStack<StackSize> stack2;

            // NOLINTNEXTLINE(*-reinterpret-cast)
            const std::span<uint8_t> Priv2(reinterpret_cast<uint8_t*>(&task2), sizeof(Task));
            task2.setup(stack2.span(), Priv2, Priority::Norm, Task::entrypoint);
            break;
        }
        default: nv::info("Flexcomm for SPI not defined (%d)\n", config.port_id);
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
, _driver()
, _client(config.client_id)
, _event(nv::ipc::Event::make(config.event_id))
, _queue(nv::ipc::Queue::make(config.queue_id))
, _boot_event(config.boot_event)
, _master_device(config.master_device)
, _slave_device(config.slave_device)
, _spb(_driver, _event, _client)
{
    if (_master_device == MasterDevice::Spi0 || _master_device == MasterDevice::Spi1
        || _master_device == MasterDevice::Spi2) {
        if (_slave_device == SlaveDevice::Glacier) {
            _spb._driver.bind(config.flexcomm_id,
                              config.cs_port_id,
                              config.cs_pin_id,
                              config.overwrite_freq,
                              config.freq,
                              this);
            _spb._driver.init();
        }
    }
}

void Task::start()
{
    using namespace ipc;

    Buffer request{};
    auto   item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));

    bootloader::Driver::set_task_booted(_boot_event);

    while (true) {
        auto wait = EventBits::MbxEvent | EventBits::TxEvent | EventBits::RxEvent
                  | EventBits::RoutingTableEvent;

        auto event = _event.wait(wait, true, false, 5s);

        // If TxEvent set, read tx from queue
        if (event.value() & EventBits::TxEvent) {
            auto status = _queue.recv(item);
            if (status != Queue::Status::Ok) {
                logger::error(logger::Event::SpiQueueFail,
                              {static_cast<uint8_t>(static_cast<uint16_t>(_client) & UINT8_MAX),
                               static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                               static_cast<uint8_t>(status)});
            }
            else {
                handle_tx(request);
            }
        }
        // Trigger by interrupt indicated mailbox is written
        if (event.value() & EventBits::MbxEvent) {
            // Read mailbox
            uint32_t ec2spimb = 0;
            auto     status   = _spb.mailbox_read(ec2spimb);
            if (status != SpbStatus::Ok) {
                error_log(SpbStatus::MbxReadFailInTask, _client);
                continue;
            }
            // Should only get EC_MSG_AVAILABLE since currently no tx, rx
            if (ec2spimb & EC_MSG_AVAILABLE) {
                handle_rx();
            }
        }

        if (event.value() & EventBits::RxEvent) {
            handle_rx();
        }

        if (event.value() & EventBits::RoutingTableEvent) {
            handle_update_routing_table();
        }

        // Set TxEvent if tx queue not empty
        if (_queue.size() != 0) {
            (void)_event.set(EventBits::TxEvent);
        }
    }
}

Task::Status Task::tx(const mctp::Packet& packet)
{
    using namespace ipc;
    ipc::QueueId queue_id{};
    ipc::EventId event_id{};
    switch (packet.priv.packet_interface) {
        case static_cast<uint8_t>(mctp::Client::Spi0):
            queue_id = QueueId::Spi0;
            event_id = EventId::Spi0Event;
            break;
        case static_cast<uint8_t>(mctp::Client::Spi1):
            queue_id = QueueId::Spi1;
            event_id = EventId::Spi1Event;
            break;
        case static_cast<uint8_t>(mctp::Client::Spi2):
            queue_id = QueueId::Spi2;
            event_id = EventId::Spi2Event;
            break;
        default: return Task::Status::InvalidInterface;
    }

    // Sending tx to queue
    Buffer request{};

    memcpy(request.data(), &packet, sizeof(packet));
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = Queue::make(queue_id).send(item);
    if (queue_status != Queue::Status::Ok) {
        // 2 bytes for client
        logger::error(logger::Event::SpiQueueFail,
                      {packet.priv.packet_interface, 0, static_cast<uint8_t>(queue_status)});
        return Task::Status::FailToPutTxPacket;
    }

    // Set TxEvent
    auto event_status = Event::make(event_id).set(EventBits::TxEvent);
    if (event_status != Event::Status::Ok) {
        return Task::Status::FailToSetTxEvent;
    }

    return Task::Status::Ok;
}

void Task::handle_tx(Buffer& buffer)
{
    auto status = _spb.spi_mctp_send(buffer);
    if (status != SpbStatus::Ok) {
        error_log(status, _client);
    }
}

void Task::handle_rx()
{
    Buffer buffer{};
    auto   status = _spb.spi_mctp_recv(buffer);
    if (status != SpbStatus::Ok) {
        error_log(status, _client);
    }
    else {
        forward(buffer);
    }
}

void Task::forward(Buffer& buffer)
{
    auto item      = ipc::Queue::Item(buffer);
    bool is_routed = false;

    auto& mctp_packet = mctp::Packet::from(buffer);

    for (auto& entry : _routing_table) {
        if (entry.is_enumerated == true) {
            if (entry.assigned_eid == mctp_packet.hdr.dst_eid) {
                if (entry.client == mctp::Client::UsUsb) {
                    auto status = usb::Task::usb_tx(item);
                    if (status != usb::Status::Ok) {
                        logger::error(
                            logger::Event::SpiCannotSendToUsb,
                            {static_cast<uint8_t>(static_cast<uint16_t>(_client) & UINT8_MAX),
                             static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                             static_cast<uint8_t>(status)});
                    }
                    is_routed = true;
                }
            }
        }
    }

    mctp::Status status{};
    if (is_routed == false) {
        status = mctp::Driver::mctp_send(item, _client);
        if (status != mctp::Status::Ok) {
            logger::error(logger::Event::SpiCannotSendToMctp,
                          {static_cast<uint8_t>(static_cast<uint16_t>(_client) & UINT8_MAX),
                           static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                           static_cast<uint8_t>(status)});
        }
    }
}

Task::Status Task::update_routing_table(mctp::Client client)
{
    using namespace ipc;
    ipc::EventId event_id{};
    switch (client) {
        case mctp::Client::Spi0: event_id = EventId::Spi0Event; break;
        case mctp::Client::Spi1: event_id = EventId::Spi1Event; break;
        case mctp::Client::Spi2: event_id = EventId::Spi2Event; break;
        default                : return Task::Status::InvalidInterface;
    }
    // Set TxEvent
    auto event_status = Event::make(event_id).set(EventBits::RoutingTableEvent);
    if (event_status != Event::Status::Ok) {
        return Task::Status::FailToSetRoutingTableEvent;
    }

    return Task::Status::Ok;
}

void Task::handle_update_routing_table()
{
    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&_routing_table), sizeof(_routing_table));
    auto             router_queue = ipc::Queue::make(ipc::QueueId::RoutingTable);
    auto             queue_status = router_queue.recv(item, 100ms);

    if (queue_status != ipc::Queue::Status::Ok) {
        logger::error(logger::Event::SpiQueueFail,
                      {static_cast<uint8_t>(static_cast<uint16_t>(_client) & UINT8_MAX),
                       static_cast<uint8_t>(static_cast<uint16_t>(_client) >> 8U),
                       static_cast<uint8_t>(queue_status)});
    }
}

void Task::error_log(SpbStatus status, mctp::Client client)
{
    if (EnableMctpOverSpiDebugLog) {
        logger::error(logger::Event::SpiError,
                      {static_cast<uint8_t>(static_cast<uint16_t>(client) & UINT8_MAX),
                       static_cast<uint8_t>(static_cast<uint16_t>(client) >> 8U),
                       static_cast<uint8_t>(status)});
    }
}

void Task::timeout_log(Timeout timeout)
{
    if (EnableMctpOverSpiDebugLog) {
        logger::error(logger::Event::SpiTimeout, {static_cast<uint8_t>(timeout)});
    }
}

void Task::middle_error_log(SpbStatus status)
{
    if (EnableMctpOverSpiDebugLog) {
        logger::error(logger::Event::SpiMidError, {static_cast<uint8_t>(status)});
    }
}

void Task::handle_mbx_interrupt(ipc::EventId event_id, mctp::Client client)
{
    auto status = nv::ipc::Event::make(event_id).set(nv::spi::EventBits::MbxEvent);
    if (status != nv::ipc::Event::Status::Ok) {
        error_log(SpbStatus::InterruptHandleError, client);
    }
}