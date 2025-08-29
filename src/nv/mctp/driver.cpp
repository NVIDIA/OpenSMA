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
#include "nv/mctp/driver.h"

#include <chrono>
#include <cstring>

#include "nv/common/system.h"
#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/ipc/timer.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include "nv/mctp/debug.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/nsm_type_5.h"
#include "nv/mctp/task.h"
#include "nv/mctp/vendor.h"
#include "nv/nv.h"
#include "nv/pldm/task.h"
#include "nv/spdm/task.h"
#include "nv/spi/task.h"
#include "nv/usb/task.h"
#include "nv/watchdog/runtime.h"

using namespace nv::mctp;
using namespace nv;
using namespace std::chrono_literals;

Driver::Driver(ipc::Task& task, common::Uuid& uuid)
: _vendor(_control)
, _nsm(_control)
, _validator(_control.router())
, _composer()
, _usb_us_queue(ipc::Queue::make(ipc::QueueId::MctpUsUsbRequest))
, _i2c_us_queue(ipc::Queue::make(ipc::QueueId::MctpUsI2c0Request))
, _i2c0_ds_queue(ipc::Queue::make(ipc::QueueId::MctpDsI2c0Request))
, _i2c1_ds_queue(ipc::Queue::make(ipc::QueueId::MctpDsI2c1Request))
, _i2c2_ds_queue(ipc::Queue::make(ipc::QueueId::MctpDsI2c2Request))
, _i2c3_ds_queue(ipc::Queue::make(ipc::QueueId::MctpDsI2c3Request))
, _i3c0_ds_queue(ipc::Queue::make(ipc::QueueId::MctpDsI3c0Request))
, _i3c1_ds_queue(ipc::Queue::make(ipc::QueueId::MctpDsI3c1Request))
, _spi0_queue(ipc::Queue::make(ipc::QueueId::MctpSpi0Request))
, _spi1_queue(ipc::Queue::make(ipc::QueueId::MctpSpi1Request))
, _spi2_queue(ipc::Queue::make(ipc::QueueId::MctpSpi2Request))
, _pldm_queue(ipc::Queue::make(ipc::QueueId::MctpPldmRequest))
, _spdm_queue(ipc::Queue::make(ipc::QueueId::MctpSpdmRequest))
, _cmd_queue(ipc::Queue::make(ipc::QueueId::MctpCmd))
, _fake_queue(ipc::Queue::make(ipc::QueueId::MctpFake))
, _router_queue(ipc::Queue::make(ipc::QueueId::RoutingTable))
, _enum_done_timer(
      ipc::Timer::make(ipc::TimerId::MctpEnumerate, 1s, on_enumeration_done_timer, false))
, _task(task)
{
    _control.router().ec.uuid = uuid;
}

void Driver::init()
{
    auto& uuid = _control.router().ec.uuid;
    /* logging uuid , no lint due to only for logging*/
    logger::info(
        logger::Event::MctpUuid,
        {uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7]});  // NOLINT

    logger::info(logger::Event::MctpUuid,
                 {uuid[8],     // NOLINT
                  uuid[9],     // NOLINT
                  uuid[10],    // NOLINT
                  uuid[11],    // NOLINT
                  uuid[12],    // NOLINT
                  uuid[13],    // NOLINT
                  uuid[14],    // NOLINT
                  uuid[15]});  // NOLINT

    /* init downstream to need enumerate */
    auto& map = _control.routing_map();
    for (auto& entry : map) {
        entry.is_need_enumerate = true;
    }

    // init all interface
    if (!ipc::DownStreamInfos.empty()) {
        for (auto& info : ipc::DownStreamInfos) {
            if (info.client == Client::DsI3c0 || info.client == Client::DsI3c1) {
                i3c::Task::init_bus(info.client, true);
            }
        }
    }
}

[[noreturn]] void Driver::main()
{
    init();

    auto status = nv::flash::Flash::set_data(nv::flash::Key::NpdsBootTimeFromFmcEndToMctpReady,
                                             sys::ipc::get_os_ticks()
                                                 * nv::common::System::inst().tick_time_ms());
    if (status != nv::flash::Status::Ok) {
        // TODO: shall NOT happen, avoid warning only
        nv::error("Failed to set boot time");
    }

    const Command Cmd{};
    auto          cmd_item = ipc::Queue::Item(std::bit_cast<uint8_t*>(&Cmd), sizeof(Cmd));

    while (true) {
        auto cmd_status = _cmd_queue.recv(cmd_item, 1s);

        if (cmd_status == ipc::Queue::Status::Ok) {
            switch (Cmd.cmd) {
                case static_cast<uint16_t>(CmdCode::WdtEvent): on_wdt_event(); break;
                case static_cast<uint16_t>(Client::UsI2c):
                case static_cast<uint16_t>(Client::UsUsb):
                case static_cast<uint16_t>(Client::DsI2c0):
                case static_cast<uint16_t>(Client::DsI2c1):
                case static_cast<uint16_t>(Client::DsI2c2):
                case static_cast<uint16_t>(Client::DsI2c3):
                case static_cast<uint16_t>(Client::DsI3c0):
                case static_cast<uint16_t>(Client::DsI3c1):
                case static_cast<uint16_t>(Client::Spi0):
                case static_cast<uint16_t>(Client::Spi1):
                case static_cast<uint16_t>(Client::Spi2):
                    on_receive(static_cast<Client>(Cmd.cmd));
                    break;
                case static_cast<uint16_t>(Client::Pldm): {
                    memset(&_mctp_tx_buf, 0, sizeof(_mctp_tx_buf));
                    nv::ipc::Queue::Item pldm_buf_item{
                        _mctp_tx_buf.begin(), _mctp_tx_buf.begin() + Constants::PldmRxBufSize};
                    on_receive_application(_pldm_queue, pldm_buf_item);
                    break;
                }
                case static_cast<uint16_t>(Client::Spdm): {
                    memset(&_mctp_tx_buf, 0, sizeof(_mctp_tx_buf));
                    nv::ipc::Queue::Item spdm_buf_item{
                        _mctp_tx_buf.begin(), _mctp_tx_buf.begin() + Constants::SpdmRxBufSize};
                    on_receive_application(_spdm_queue, spdm_buf_item);
                    break;
                }
                case static_cast<uint16_t>(CmdCode::Enumerate)    : on_enumerate(); break;
                case static_cast<uint16_t>(CmdCode::EnumerateDone): on_enumerate_done(); break;
                case static_cast<uint16_t>(CmdCode::DiscoveryNotify):
                    on_discovery_notify();
                    break;
                case static_cast<uint16_t>(CmdCode::RotStateInfoChange):
                    on_type6_event(NsmFwEvent::RotStateInformationChangeEvent);
                    break;
                case static_cast<uint16_t>(CmdCode::EndPointStatusChange):
                    on_endpint_status_change(Cmd.data1, Cmd.data2);
                    break;
                case static_cast<uint16_t>(CmdCode::NsmT5FatalFaultEI):
                    nsm_type5::on_nsm_t5_fatal_fault_ei(Cmd.data1);
                default: break;
            }
        }
    }
}

ipc::Queue& Driver::get_queue(Client client)
{
    switch (client) {
        case Client::UsI2c : return _i2c_us_queue; break;
        case Client::UsUsb : return _usb_us_queue; break;
        case Client::DsI2c0: return _i2c0_ds_queue; break;
        case Client::DsI2c1: return _i2c1_ds_queue; break;
        case Client::DsI2c2: return _i2c2_ds_queue; break;
        case Client::DsI2c3: return _i2c3_ds_queue; break;
        case Client::DsI3c0: return _i3c0_ds_queue; break;
        case Client::DsI3c1: return _i3c1_ds_queue; break;
        case Client::Spi0  : return _spi0_queue; break;
        case Client::Spi1  : return _spi1_queue; break;
        case Client::Spi2  : return _spi2_queue; break;
        default            : return _fake_queue;
    }
}

void Driver::on_receive(Client client)
{
    if (client >= Client::End) {
        return;
    }
    auto             queue = get_queue(client);
    ipc::Queue::Item item(_rx_buf.begin(), _rx_buf.begin() + queue.item_size());

    auto  status             = queue.recv(item, 100ms);
    auto& rx                 = from(_rx_buf);
    rx.priv.packet_interface = static_cast<uint8_t>(client);
    if (status != ipc::Queue::Status::Ok) {
        return;
    }

    if (_validator.validate(rx, client) == false) {
        // dump_packet(rx, mctp_buf_size);
        return;
    }

    auto     is_multi = false;
    uint16_t pkt_size{};
    auto&    multi_rx = from(_multi_pkt_buf);
    if (!(rx.hdr.som && rx.hdr.eom)) {
        // TODO: add other type multi command
        if (!_composer.recv_packet(pkt_size, rx, _multi_pkt_buf)) {
            return;
        }
        else {
            /* sub private header size */
            pkt_size = pkt_size > sizeof(PrivateHeader) ? pkt_size - sizeof(PrivateHeader)
                                                        : sizeof(PrivateHeader);
            multi_rx.priv.packet_length = pkt_size;
            is_multi                    = true;
        }
    }

    /* using large or standard buffer */
    auto&   mctp_rx  = is_multi ? multi_rx : rx;
    auto    msg_type = Control::PktReq::from(mctp_rx).msg_type;
    Packet& tx       = from(_mctp_tx_buf);

    switch (msg_type) {
        case MsgType::Control: {
            memset(_mctp_tx_buf.data(), 0, _mctp_tx_buf.size());
            _control.process(mctp_rx, tx);
            if (tx.priv.packet_length != 0) {
                nv::ipc::Queue::Item control_item{_mctp_tx_buf};
                on_forward_message(control_item);
            }
            break;
        }
        case MsgType::Pldm:
            forward(Client::Pldm, mctp_rx);
            if (is_multi == true) {
                _composer.clear();
            }
            break;
        case MsgType::Spdm:
            forward(Client::Spdm, mctp_rx);
            if (is_multi == true) {
                _composer.clear();
            }
            break;
        case MsgType::VendorPci: {
            memset(_mctp_tx_buf.data(), 0, _mctp_tx_buf.size());
            _nsm.process(mctp_rx, tx);
            nv::ipc::Queue::Item pci_item{_mctp_tx_buf};
            on_forward_message(pci_item);
            if (is_multi == true) {
                _composer.clear();
            }
            break;
        }
        case MsgType::VendorIani: {
            memset(_mctp_tx_buf.data(), 0, _mctp_tx_buf.size());
            _vendor.process(mctp_rx, tx);
            if (tx.priv.packet_length != 0) {
                nv::ipc::Queue::Item iana_item{_mctp_tx_buf};
                on_forward_message(iana_item);
                _vendor.action(mctp_rx, tx);
                if (is_multi == true) {
                    _composer.clear();
                }
            }
            break;
        }
        default: break;
    }
}

/*
 *  forward()
 */
void Driver::forward([[maybe_unused]] Client client, const Packet& tx)
{
    // dump_packet(tx, Constants::BufferSize);

    if (client == Client::UsI2c || client == Client::DsI2c0 || client == Client::DsI2c1
        || client == Client::DsI2c2 || client == Client::DsI2c3) {
        auto status = nv::i2c::Task::tx(tx);
        if (status != nv::i2c::Task::Status::Ok) {
            return;
        }
    }
    else if (client == Client::Spi0 || client == Client::Spi1 || client == Client::Spi2) {
        if constexpr (nv::ipc::Spi_Available) {
            auto status = nv::spi::Task::tx(tx);
            if (status != nv::spi::Task::Status::Ok) {
                return;
            }
        }
    }
    else if (client == Client::DsI3c0 || client == Client::DsI3c1) {
        auto status = nv::i3c::Task::tx(tx);
        if (status != nv::i3c::Task::Status::Ok) {
            return;
        }
    }
    else if (client == Client::UsUsb) {
        auto item   = ipc::Queue::Item(std::bit_cast<uint8_t*>(&tx), Constants::BufferSize);
        auto status = usb::Task::usb_tx(item);
        if (status != usb::Status::Ok) {
            return;
        }
    }
    else if (client == Client::Pldm) {
        auto status = nv::pldm::Task::pldm_tx(tx);
        if (status != nv::pldm::Status::Ok) {
            return;
        }
    }
    else if (client == Client::Spdm) {
        // send message from ap to spdm library.
        auto status = nv::spdm::Task::spdm_tx(tx);
        if (status != nv::spdm::Status::Ok) {
            return;
        }
    }
    else {
        return;
    }
}

void Driver::on_forward_message(ipc::Queue::Item& rx_item)
{
    auto& tx     = from(_tx_buf);
    auto& rx     = nv::mctp::Packet::from(rx_item);
    auto  size   = rx.priv.packet_length;
    auto  client = static_cast<Client>(rx.priv.packet_interface);
    // loop is 1 for single package
    auto loop = 1U;
    if (size > PktBufDataLen) {
        loop += (size - PktBufDataLen - 1) / sizeof(nv::mctp::Packet::msg) + 1;
    }

    if (loop == 1) {
        _control.update_eid(rx, rx.priv.packet_interface);
        forward(client, rx);
    }
    else {
        for (uint32_t index = 0; index < loop; index++) {
            bool is_complete = false;
            _composer.gen_packet(tx, size, index, is_complete, rx_item);
            _control.update_eid(tx, rx.priv.packet_interface);
            forward(client, tx);
            if (is_complete == true) {
                break;
            }
        }
    }
}

void Driver::on_receive_application(nv::ipc::Queue& queue, ipc::Queue::Item& rx_item)
{
    auto status = queue.recv(rx_item, 100ms);
    if (status != ipc::Queue::Status::Ok) {
        return;
    }
    on_forward_message(rx_item);
}

void Driver::on_enumerate()
{
    auto is_enumeate = _control.is_enumerated();
    if (!is_enumeate) {
        return;
    }

    constexpr static uint8_t IsRespnse = false;
    constexpr static uint8_t MsgTag    = 0;
    constexpr static uint8_t HdrVer    = 1;
    constexpr static uint8_t DstEid    = 0;

    Packet tx{};

    auto index = 0;
    if (!ipc::DownStreamInfos.empty()) {
        auto& map = _control.routing_map();
        for (auto& info : ipc::DownStreamInfos) {
            // prepare packets, gen mctp header
            if (map.at(index).is_need_enumerate == true) {
                /* should clear enumerate result */
                map.at(index).is_enumerated = false;
                Composer::construct_mctp_header(
                    tx,
                    Control::SetEidReqSize,
                    IsRespnse,
                    DstEid,
                    static_cast<uint8_t>(static_cast<uint16_t>(info.client) & UINT8_MAX),
                    MsgTag,   // msg_tag
                    HdrVer);  // hdr_ver

                // gen set eid pkt
                _control.on_gen_set_endpoint_id(
                    tx,
                    static_cast<uint8_t>((_control.get_start_eid() + index) & UINT8_MAX),
                    SetEndpoint::SetEidForced);

                forward(info.client, tx);
                logger::info(
                    logger::Event::MctpEnumerateSetEid,
                    {static_cast<uint8_t>(static_cast<uint16_t>(info.client) & UINT8_MAX),
                     static_cast<uint8_t>((_control.get_start_eid() + index) & UINT8_MAX)});
            }
            index++;
        }
        _enum_done_timer.start();
    }
}

void Driver::on_enumerate_done()
{
    auto& map = _control.routing_map();

    for (auto& entry : map) {
        if (entry.is_enumerated == true) {
            uint8_t log{};
            if (entry.client >= Client::End) {
                return;
            }
            else {
                log = static_cast<uint8_t>(entry.client);
            }
            logger::info(logger::Event::MctpEnumerateResult, {entry.assigned_eid, log});
        }
    }

    pdk::mctp::app::Control::RoutingMap tmp_map;
    tmp_map       = {};
    auto item_tmp = ipc::Queue::Item(std::bit_cast<uint8_t*>(&tmp_map), sizeof(tmp_map));

    // flush router queue of stale routing table events
    while (_router_queue.recv(item_tmp, 0s) == ipc::Queue::Status::Ok) {
        // do nothing, to replace _router_queue.reset();
    }
    auto item = ipc::Queue::Item(std::bit_cast<uint8_t*>(&map), sizeof(map));

    // send routing table to upstream task
    auto queue_status = _router_queue.send(item, 100ms);
    if (queue_status != ipc::Queue::Status::Ok) {
        logger::error(
            logger::Event::MctpRouterQueueFail,
            {static_cast<uint8_t>(queue_status), static_cast<uint8_t>(Client::UsUsb)});
    }
    usb::Task::set_update_routing_table_event();

    for (auto upstream_index = 0; upstream_index < ipc::UpStreamNum; upstream_index++) {
        auto entry = map.at(upstream_index + ipc::DownStreamNum);
        if (entry.is_enumerated && entry.client == mctp::Client::UsI2c) {
            auto queue_status = _router_queue.send(item, 100ms);
            if (queue_status != ipc::Queue::Status::Ok) {
                logger::error(
                    logger::Event::MctpRouterQueueFail,
                    {static_cast<uint8_t>(queue_status), static_cast<uint8_t>(entry.client)});
            }
            i2c::Task::update_routing_table(entry.client);
        }
    }

    // send routing table to downstream task
    if (!ipc::DownStreamInfos.empty()) {
        for (auto& downstream_entry : ipc::DownStreamInfos) {
            if (downstream_entry.client == mctp::Client::DsI2c0
                || downstream_entry.client == mctp::Client::DsI2c1
                || downstream_entry.client == mctp::Client::DsI2c2
                || downstream_entry.client == mctp::Client::DsI2c3) {
                // update routing table to io task
                auto queue_status = _router_queue.send(item, 100ms);
                if (queue_status != ipc::Queue::Status::Ok) {
                    logger::error(logger::Event::MctpRouterQueueFail,
                                  {static_cast<uint8_t>(queue_status),
                                   static_cast<uint8_t>(downstream_entry.client)});
                }
                i2c::Task::update_routing_table(downstream_entry.client);
            }
            else if (downstream_entry.client == mctp::Client::DsI3c0
                     || downstream_entry.client == mctp::Client::DsI3c1) {
                auto queue_status = _router_queue.send(item, 100ms);
                if (queue_status != ipc::Queue::Status::Ok) {
                    logger::error(logger::Event::MctpRouterQueueFail,
                                  {static_cast<uint8_t>(queue_status),
                                   static_cast<uint8_t>(downstream_entry.client)});
                }
                i3c::Task::update_routing_table(downstream_entry.client);
            }
            else if (downstream_entry.client == mctp::Client::Spi0
                     || downstream_entry.client == mctp::Client::Spi1
                     || downstream_entry.client == mctp::Client::Spi2) {
                if constexpr (nv::ipc::Spi_Available) {
                    auto queue_status = _router_queue.send(item, 100ms);
                    if (queue_status != ipc::Queue::Status::Ok) {
                        logger::error(logger::Event::MctpRouterQueueFail,
                                      {static_cast<uint8_t>(queue_status),
                                       static_cast<uint8_t>(downstream_entry.client)});
                    }
                    spi::Task::update_routing_table(downstream_entry.client);
                }
            }
        }
    }

    // send discovery notify
    if (_is_send_notify) {
        _is_send_notify = false;
        on_discovery_notify();
    }
}

void Driver::on_discovery_notify()
{
    auto is_enumeate = _control.is_enumerated();
    if (!is_enumeate) {
        return;
    }

    constexpr static uint8_t IsRespnse = false;
    constexpr static uint8_t MsgTag    = 0;
    constexpr static uint8_t HdrVer    = 1;

    auto& map = _control.routing_map();
    for (uint8_t upstream_index = 0; upstream_index < ipc::UpStreamNum; upstream_index++) {
        auto entry = map.at(upstream_index + ipc::DownStreamNum);
        if (!entry.is_enumerated) {
            continue;
        }
        uint8_t dst_eid{};
        Client  client{};
        Packet  tx{};

        if (entry.client == mctp::Client::UsI2c) {
            dst_eid = _control.get_us_i2c_eid();
            client  = mctp::Client::UsI2c;
        }
        else if (entry.client == mctp::Client::UsUsb) {
            dst_eid = _control.get_allocate_src_eid();
            client  = mctp::Client::UsUsb;
        }
        else {
            continue;
        }

        Composer::construct_mctp_header(tx,
                                        Control::NotifyDisReqSize,
                                        IsRespnse,
                                        dst_eid,
                                        static_cast<uint8_t>(client),
                                        MsgTag,   // msg_tag
                                        HdrVer);  // hdr_ver

        _control.on_gen_notify_discovery(tx);
        _control.update_eid(tx, static_cast<uint8_t>(client));

        forward(client, tx);
        logger::info(logger::Event::MctpDiscoveryNotify,
                     {dst_eid, static_cast<uint8_t>(client)});
    }
}

void Driver::dump_packet(const Packet& pkt, uint32_t len)
{
#if defined(MCTP_DEBUG_ENABLED)
    auto buf = pkt.to_span();
    for (uint32_t i = 0; i < ((len + 3) & 0xFC); i += 4) {
        logger::info(logger::Event::MctpDumpPacket,
                     {static_cast<uint8_t>(i & UINT8_MAX),
                      static_cast<uint8_t>((i >> 8) & UINT8_MAX),
                      static_cast<uint8_t>((i >> 16) & UINT8_MAX),
                      static_cast<uint8_t>((i >> 24) & UINT8_MAX),
                      buf[i + 0],
                      buf[i + 1],
                      buf[i + 2],
                      buf[i + 3]});
    }
#endif
}

void Driver::on_enumeration_done_timer([[maybe_unused]] ipc::Timer& id)
{
    mctp_send_cmd(CmdCode::EnumerateDone);
}

void Driver::on_endpint_status_change(uint8_t end_point, uint8_t status)
{
    logger::info(logger::Event::MctpEndpointState, {end_point, status});

    if (status == 0) {
        // remove from table
        auto is_valid = _control.remove_routing_entry(end_point);
        if (!is_valid) {
            return;
        }

        if (!ipc::DownStreamInfos.empty()) {
            auto client = ipc::DownStreamInfos.at(end_point).client;
            if (client == Client::DsI3c0 || client == Client::DsI3c1) {
                i3c::Task::init_bus(client, false);
            }
        }

        _is_send_notify = true;

        // update table to each task
        on_enumerate_done();
    }
    else if (status == 1) {
        auto is_valid = _control.add_routing_entry(end_point);
        if (!is_valid) {
            return;
        }

        // init interface
        if (!ipc::DownStreamInfos.empty()) {
            auto client = ipc::DownStreamInfos.at(end_point).client;
            if (client == Client::DsI3c0 || client == Client::DsI3c1) {
                i3c::Task::init_bus(client, true);
            }
        }

        // do enumerate again
        on_enumerate();

        _is_send_notify = true;
    }
}

void Driver::on_type6_event(NsmFwEvent event)
{
    const NsmMsgType nv_msg_type   = NsmMsgType::Firmware;
    uint8_t          event_version = 0;
    uint8_t          event_id      = 0;
    uint8_t          event_class   = 0;
    uint16_t         event_state   = 0;
    uint8_t          data_size     = 0;

    switch (event) {
        case NsmFwEvent::RotStateInformationChangeEvent: {
            event_version = 0;
            event_id      = (uint8_t)event;
            event_class   = 0;
            event_state   = 0;
            data_size     = 0;
            break;
        }
        default: return;
    }

    // Event not enable
    if (!_nsm.is_event_source_enable(nv_msg_type, event_id)) {
        return;
    }

    // The global event generation setting is not PUSH
    if (!_nsm.is_global_event_setting_push()) {
        return;
    }

    // Event setting (Push) : Push event messages to the subscribed receiver
    // Create event log
    EventLog event_log{};
    fill_event_log(
        event_log, nv_msg_type, event_version, event_id, event_class, event_state, data_size);

    // Fill in the event msg and send by forward
    Packet event_msg{};
    _nsm.fill_event_msg(event_log, event_msg);
    auto client = static_cast<Client>(event_msg.priv.packet_interface);
    forward(client, event_msg);
}

bool Driver::is_allow_bridge(const Packet& pkt)
{
    auto& ctl_pkt = mctp::Control::PktReq::from(pkt);
    // Check if it's the 1st packet, and is set_eid or routing info update command
    if ((ctl_pkt.som) && ctl_pkt.msg_type == mctp::MsgType::Control
        && (ctl_pkt.command_code == mctp::Cmd::SetEpId
            || ctl_pkt.command_code == mctp::Cmd::RoutingInfoUpdate)) {
        return false;
    }
    return true;
}

void Driver::on_wdt_event()
{
    nv::watchdog::Runtime::mark_task_alive(nv::watchdog::TaskMonitorIndex::Mctp);
}

void Driver::wdt_notify()
{
    mctp_send_cmd(CmdCode::WdtEvent);
}

void Driver::fill_event_log(EventLog&  event_log,
                            NsmMsgType nv_msg_type,
                            uint8_t    event_version,
                            uint8_t    event_id,
                            uint8_t    event_class,
                            uint16_t   event_state,
                            uint8_t    data_size)
{
    event_log.nv_msg_type   = nv_msg_type;
    event_log.event_version = event_version;
    event_log.event_id      = event_id;
    event_log.event_class   = event_class;
    event_log.event_state   = event_state;
    event_log.data_size     = data_size;
    // TODO - data undefined
}

mctp::Status Driver::mctp_send(ipc::Queue::Item& item, ipc::QueueId id, Client client)
{
    auto& queue        = ipc::Queue::make(id);
    auto  queue_status = queue.send(item, 100ms);
    if (queue_status != ipc::Queue::Status::Ok) {
        return Status::QueueSendFail;
    }

    const Command Cmd{.cmd = static_cast<uint16_t>(client)};
    auto          cmd_item  = ipc::Queue::Item(std::bit_cast<uint8_t*>(&Cmd), sizeof(Command));
    auto&         cmd_queue = ipc::Queue::make(ipc::QueueId::MctpCmd);
    queue_status            = cmd_queue.send(cmd_item, 100ms);
    if (queue_status != ipc::Queue::Status::Ok) {
        return Status::QueueSendFail;
    }

    return Status::Ok;
}

mctp::Status Driver::mctp_send_cmd(CmdCode cmd, uint8_t data1, uint8_t data2)
{
    const Command Cmd{.cmd = static_cast<uint16_t>(cmd), .data1 = data1, .data2 = data2};
    auto          cmd_item  = ipc::Queue::Item(std::bit_cast<uint8_t*>(&Cmd), sizeof(Command));
    auto&         cmd_queue = ipc::Queue::make(ipc::QueueId::MctpCmd);

    auto is_isr = ipc::Supervisor::is_in_isr();

    ipc::Queue::Status status{};
    if (!is_isr) {
        status = cmd_queue.send(cmd_item, 100ms);
    }
    else {
        status = cmd_queue.send_isr(cmd_item);
    }
    if (status != ipc::Queue::Status::Ok) {
        return Status::QueueSendFail;
    }

    return Status::Ok;
}

nv::mctp::Status Driver::mctp_send_from_spi_0(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpSpi0Request, Client::Spi0);
}

nv::mctp::Status Driver::mctp_send_from_spi_1(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpSpi1Request, Client::Spi1);
}

nv::mctp::Status Driver::mctp_send_from_spi_2(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpSpi2Request, Client::Spi2);
}

nv::mctp::Status Driver::mctp_send_from_us_i2c_0(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpUsI2c0Request, Client::UsI2c);
}

nv::mctp::Status Driver::mctp_send_from_ds_i2c_0(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpDsI2c0Request, Client::DsI2c0);
}

nv::mctp::Status Driver::mctp_send_from_ds_i2c_1(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpDsI2c1Request, Client::DsI2c1);
}

nv::mctp::Status Driver::mctp_send_from_ds_i2c_2(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpDsI2c2Request, Client::DsI2c2);
}

nv::mctp::Status Driver::mctp_send_from_ds_i2c_3(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpDsI2c3Request, Client::DsI2c3);
}

nv::mctp::Status Driver::mctp_send_from_ds_i3c_0(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpDsI3c0Request, Client::DsI3c0);
}

nv::mctp::Status Driver::mctp_send_from_ds_i3c_1(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpDsI3c1Request, Client::DsI3c1);
}

nv::mctp::Status Driver::mctp_send_from_us_usb(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpUsUsbRequest, Client::UsUsb);
}

nv::mctp::Status Driver::mctp_send_from_pldm(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpPldmRequest, Client::Pldm);
}

nv::mctp::Status Driver::mctp_send_from_spdm(nv::ipc::Queue::Item& item)
{
    return mctp_send(item, ipc::QueueId::MctpSpdmRequest, Client::Spdm);
}

mctp::Status Driver::endpoint_status_change(uint8_t end_point, uint8_t status)
{
    return mctp_send_cmd(CmdCode::EndPointStatusChange, end_point, status);
}