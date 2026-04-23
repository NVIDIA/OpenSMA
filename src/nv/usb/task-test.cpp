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
#include <chrono>
#include <cstring>

#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/interface.h"
#include "nv/usb/task.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;
using namespace std::chrono_literals;

class Usb : public ut::Fixture
{
public:
    void setup() override
    {
        // suspend other tasks that are attached to Mctp
        auto& mctptask = nv::ipc::Supervisor::inst().task(ipc::TaskId::Mctp);
        mctptask.suspend();

        // clean up mctp cmd queue
        auto&                 mctp_cmd_queue = ipc::Queue::make(ipc::QueueId::MctpCmd);
        mctp::Driver::Command cmd{};
        auto cmd_item = ipc::Queue::Item(std::bit_cast<uint8_t*>(&cmd), sizeof(cmd));

        while (mctp_cmd_queue.size() != 0) {
            auto cmd_status = mctp_cmd_queue.recv(cmd_item, 100ms);
            if (cmd_status != ipc::Queue::Status::Ok) {
                break;
            }
        }
    }

    void teardown() override
    {
        // restart any other tasks
        auto& mctptask = nv::ipc::Supervisor::inst().task(ipc::TaskId::Mctp);
        mctptask.resume();
    }
};

TEST_F(Usb, MctpUsbRx)
{
    auto& task = nv::ipc::Supervisor::inst().task(ipc::TaskId::Usb);

    nv::usb::Task&        usbtask = *std::bit_cast<nv::usb::Task*>(&task);
    usb::Task::MctpPacket usb_pkt{};

    // prepare usb packet
    usb_pkt.usb_hdr.dmtf_id = usb::Task::UsbDmtfId;
    usb_pkt.usb_hdr.length  = sizeof(usb_pkt);

    for (uint32_t i = 0; i < usb_pkt.msg.size(); i++) {
        usb_pkt.msg[i] = i;
    }

    memcpy(usbtask.mctp_rx_buffer.data(), &usb_pkt, sizeof(usb_pkt));

    auto& mctp_queue     = ipc::Queue::make(ipc::QueueId::MctpDataRequest);
    auto& mctp_cmd_queue = ipc::Queue::make(ipc::QueueId::MctpCmd);

    // make sure mctp task queue and cmd queue is clear
    ensure::is_eq(mctp_queue.size(), 0x00);
    ensure::is_eq(mctp_cmd_queue.size(), 0x00);

    // send usb packet to usb task
    usb::Task::set_mctp_rx0_event();

    ensure::is_eq(mctp_queue.size(), 0x01);
    ensure::is_eq(mctp_cmd_queue.size(), 0x01);

    mctp::Driver::Command cmd{};
    auto cmd_item   = ipc::Queue::Item(std::bit_cast<uint8_t*>(&cmd), sizeof(cmd));
    auto cmd_status = mctp_cmd_queue.recv(cmd_item, 100ms);
    ensure::is_eq(cmd_status, ipc::Queue::Status::Ok);
    ensure::is_eq(cmd.cmd, static_cast<uint16_t>(mctp::Client::UsUsb));

    mctp::Driver::Buffer mctp_rx{};

    ipc::Queue::Item item(mctp_rx.begin(), mctp_rx.begin() + mctp_queue.item_size());
    auto             status = mctp_queue.recv(item, 100ms);
    ensure::is_eq(status, ipc::Queue::Status::Ok);

    auto& mctp_pkt = mctp::Driver::from(mctp_rx);

    // compare mctp pkt is the same
    ensure::is_eq(mctp_pkt.priv.packet_interface, static_cast<uint8_t>(mctp::Client::UsUsb));
    ensure::is_eq((uint32_t)mctp_pkt.priv.packet_length,
                  sizeof(usb_pkt) - sizeof(usb::Task::MctpHeader));

    auto is_same = memcmp(&usb_pkt.mctp_hdr, &mctp_pkt.hdr, sizeof(mctp::Header));
    ensure::is_eq(is_same, 0);

    for (uint32_t i = 0; i < usb_pkt.msg.size(); i++) {
        ensure::is_eq(usb_pkt.msg[i], mctp_pkt.msg[i]);
    }
};

TEST_F(Usb, MctpUsbTx)
{
    mctp::Driver::Buffer mctp_tx{};
    auto&                mctp_pkt  = mctp::Driver::from(mctp_tx);
    mctp_pkt.priv.packet_length    = mctp_pkt.msg.size() + sizeof(mctp::Header);
    mctp_pkt.priv.packet_interface = static_cast<uint8_t>(mctp::Client::UsUsb);

    for (uint32_t i = 0; i < mctp_pkt.msg.size(); i++) {
        mctp_pkt.msg[i] = i;
    }

    auto item   = ipc::Queue::Item(std::bit_cast<uint8_t*>(&mctp_tx),
                                 mctp::Constants::BufferSize);
    auto status = usb::Task::usb_tx(item);
    ensure::is_eq(status, usb::Status::Ok);

    auto&          task    = nv::ipc::Supervisor::inst().task(ipc::TaskId::Usb);
    nv::usb::Task& usbtask = *std::bit_cast<nv::usb::Task*>(&task);

    usb::Task::MctpPacket usb_pkt{};
    memcpy(&usb_pkt, usbtask.mctp_tx_buffer.data(), sizeof(usb_pkt));

    ensure::is_eq((uint32_t)usb_pkt.usb_hdr.dmtf_id, usb::Task::UsbDmtfId);
    ensure::is_eq((uint32_t)usb_pkt.usb_hdr.length, sizeof(usb_pkt));

    // compare mctp tx pkt and usb tx pkt
    for (uint32_t i = 0; i < usb_pkt.msg.size(); i++) {
        ensure::is_eq(usb_pkt.msg[i], mctp_pkt.msg[i]);
    }

    usb::Task::set_mctp_tx_done_event();

    auto& usb_event = ipc::Event::make(ipc::EventId::UsbTask);
    auto  bits      = usb_event.wait(0xFFFF, false, false, 0s);
    auto  event     = bits.value();
    ensure::is_eq(event, 0x00);
};
