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
#include <cstring>

#include "app/pdk-mctp-app-control-wrapper.h"
#include "app/pdk-mctp-app-enums.h"
#include "app/pdk-mctp-app-packet-plat.h"
#include "app/pdk-mctp-app-router-plat.h"
#include "pdk-mctp-platforms-config.h"
#include "ubs/unittest.hpp"
using namespace ubs::unittest;
using namespace pdk::mctp;

UBS_TEST(Control, FromTest)
{
    app::Packet pkt{};
    auto&       crx = platforms::Control::PktReq::from(pkt);
    ensure::is_eq(&crx, &pkt.hdr);
    auto& ctx = platforms::Control::PktRes::from(pkt);
    ensure::is_eq(&ctx, &pkt.hdr);
};

UBS_TEST(Control, FillPacketHeaderTest)
{
    platforms::Control control;
    app::Packet const  rx{
         .priv =
             {
                    .packet_length    = 0,
                    .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                    .reserved0        = 0,
                    },
         .hdr = {
                    .hdr_ver   = 0,
                    .rsvd      = 0,
                    .dst_eid   = 0x01,
                    .src_eid   = 0x02,
                    .msg_tag   = 0x1,
                    .tag_owner = 0,
                    .pkt_seq   = 0,
                    .eom       = 1,
                    .som       = 1,
                    }
    };
    app::Packet tx{};
    auto&       ctl = platforms::Control::PktRes::from(tx);
    platforms::set_cur_eid(control.router(), 0x01, 0x01);
    ctl.rq = 0;
    control.fill_packet_header(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.rsvd), 0);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.hdr_ver), 1);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.dst_eid), static_cast<uint8_t>(rx.hdr.src_eid));
    // src_eid should be NULL_EID(0x00) if eid is not set yet
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.src_eid), app::NULL_EID);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.som), 1);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.eom), 1);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.pkt_seq), 0);
    ensure::is_eq(static_cast<uint8_t>(ctl.rq), 0);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.tag_owner), 0);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.msg_tag), static_cast<uint8_t>(rx.hdr.msg_tag));
    ensure::is_eq(static_cast<uint8_t>(platforms::get_packet_interface(tx)),
                  static_cast<uint8_t>(platforms::get_packet_interface(rx)));
    ctl.rq = 1;
    control.fill_packet_header(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctl.rq), 1);
    ensure::is_eq(static_cast<uint8_t>(tx.hdr.src_eid),
                  static_cast<uint8_t>(platforms::get_cur_eid(
                      control.router(), static_cast<uint8_t>(platforms::Interface::UsI2c))));
};

UBS_TEST(Control, FillControlMsgHeaderTest)
{
    platforms::Control control;
    // Initialize rx packet's msg with array of 0
    app::Packet const rx{};
    app::Packet       tx{};
    auto&             crx = platforms::Control::PktReq::from(rx);
    auto&             ctl = platforms::Control::PktRes::from(tx);
    control.fill_control_msg_header(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctl.msg_type),
                  static_cast<uint8_t>(app::MsgType::Control));
    ensure::is_eq(static_cast<uint8_t>(ctl.d), 0);
    ensure::is_eq(static_cast<uint8_t>(ctl.rq), 0);
    ensure::is_eq(static_cast<uint8_t>(ctl.rsvd0), 0);
    ensure::is_eq(static_cast<uint8_t>(ctl.instance_id), static_cast<uint8_t>(crx.instance_id));
    ensure::is_eq(static_cast<uint8_t>(ctl.command_code),
                  static_cast<uint8_t>(crx.command_code));
};

UBS_TEST(Control, FillErrorPacketTest)
{
    platforms::Control control;
    app::Packet const  rx{};
    app::Packet        tx{};
    auto&              ctl = platforms::Control::PktRes::from(tx);
    control.fill_error_packet(platforms::Ccode::ErrorGeneral, rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctl.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorGeneral));
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse));
    control.fill_error_packet(platforms::Ccode::ErrorInvalidData, rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctl.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse));
};

UBS_TEST(Control, GetPacketTypeTest)
{
    platforms::Control control;
    app::Packet        rx{};
    auto&              crx = platforms::Control::PktReq::from(rx);
    // rx is a request packet
    crx.tag_owner = 1;
    crx.rq        = 1;
    crx.d         = 0;
    ensure::is_eq(control.get_packet_type(rx), app::PacketType::Request);
    // rx is a response packet
    crx.tag_owner = 0;
    crx.rq        = 0;
    crx.d         = 0;
    ensure::is_eq(control.get_packet_type(rx), app::PacketType::Response);
    // rx is a datagram packet
    crx.tag_owner = 1;
    crx.rq        = 1;
    crx.d         = 1;
    ensure::is_eq(control.get_packet_type(rx), app::PacketType::Datagram);
    // rx is a other packet
    crx.tag_owner = 1;
    crx.rq        = 0;
    crx.d         = 1;
    ensure::is_eq(control.get_packet_type(rx), app::PacketType::Other);
};

UBS_TEST(Control, UpdateEidTest)
{
    // Packet
    platforms::Control control;
    platforms::set_cur_eid(control.router(), 0x01, 0x01);
    app::Packet tx{};
    control.update_eid(tx, 0x01);
    ensure::is_eq(tx.hdr.src_eid, platforms::get_cur_eid(control.router(), 0x01));
    // NsmPacket
    platforms::set_cur_eid(control.router(), 0x01, 0x02);
    platforms::NsmPacket nsm_tx{};
    control.update_eid(nsm_tx, 0x01);
    ensure::is_eq(nsm_tx.hdr.src_eid, platforms::get_cur_eid(control.router(), 0x01));
};

UBS_TEST(Control, OnGenSetEndpointIdTest)
{
    platforms::Control control;
    app::Packet const  rx{};
    app::Packet        tx{};
    auto&              ctx = platforms::Control::PktReq::from(tx);
    control.on_gen_set_endpoint_id(tx, 0x01, app::SetEndpoint::SetEidNormal);
    ensure::is_eq(static_cast<uint8_t>(ctx.msg_type),
                  static_cast<uint8_t>(app::MsgType::Control));
    ensure::is_eq(static_cast<uint8_t>(ctx.d), 0);
    ensure::is_eq(static_cast<uint8_t>(ctx.rq), 1);
    ensure::is_eq(static_cast<uint8_t>(ctx.rsvd0), 0);
    ensure::is_eq(static_cast<uint8_t>(ctx.instance_id), 0);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::SetEpId));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]),
                  static_cast<uint8_t>(app::SetEndpoint::SetEidNormal));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]), 0x01);
    control.on_gen_set_endpoint_id(tx, 0x01, app::SetEndpoint::SetEidDiscovered);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]),
                  static_cast<uint8_t>(app::SetEndpoint::SetEidDiscovered));
};

UBS_TEST(Control, OnGenNotifyDiscoveryTest)
{
    platforms::Control control;
    app::Packet const  rx{};
    app::Packet        tx{};
    auto&              ctx = platforms::Control::PktReq::from(tx);
    control.on_gen_notify_discovery(tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.msg_type),
                  static_cast<uint8_t>(app::MsgType::Control));
    ensure::is_eq(static_cast<uint8_t>(ctx.d), 1);
    ensure::is_eq(static_cast<uint8_t>(ctx.rq), 1);
    ensure::is_eq(static_cast<uint8_t>(ctx.rsvd0), 0);
    ensure::is_eq(static_cast<uint8_t>(ctx.instance_id), 0);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::DiscoveryNotify));
};

UBS_TEST(Control, GetRoutingClientTest)
{
    platforms::Control control;
    control.routing_map().at(0x00).client = platforms::Interface::UsI2c;
    ensure::is_eq(static_cast<uint8_t>(control.get_routing_client(0x00)),
                  static_cast<uint8_t>(platforms::Interface::UsI2c));
    ensure::is_eq(static_cast<uint8_t>(control.get_routing_client(0x01)),
                  static_cast<uint8_t>(platforms::Interface::End));
};

class RoutingEntryTestHelper : public platforms::Control
{
public:
    using platforms::Control::_num_enumerate_eid;
};

UBS_TEST(Control, RemoveRoutingEntryTest)
{
    RoutingEntryTestHelper control;
    control._num_enumerate_eid                       = 1;
    control.routing_map().at(0x00).is_need_enumerate = true;
    control.routing_map().at(0x00).is_enumerated     = true;
    control.remove_routing_entry(0x00);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(0x00).is_need_enumerate),
                  false);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(0x00).is_enumerated), false);
    ensure::is_eq(static_cast<uint8_t>(control._num_enumerate_eid), 0);

    control._num_enumerate_eid                       = 1;
    control.routing_map().at(0x00).is_need_enumerate = true;
    control.routing_map().at(0x00).is_enumerated     = false;
    control.remove_routing_entry(0x00);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(0x00).is_need_enumerate),
                  false);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(0x00).is_enumerated), false);
    ensure::is_eq(static_cast<uint8_t>(control._num_enumerate_eid), 1);
};

UBS_TEST(Control, AddRoutingEntryTest)
{
    RoutingEntryTestHelper control;
    control.routing_map().at(0x00).is_need_enumerate = false;
    control._num_enumerate_eid                       = 1;
    control.add_routing_entry(0x00);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(0x00).is_need_enumerate), true);
    ensure::is_eq(static_cast<uint8_t>(control._num_enumerate_eid), 0);
};

// only test tx.command_code and tx.completion_code for unsupported command
UBS_TEST(Control, ProcessTest)
{
    platforms::Control control;
    app::Packet        tx{};
    auto&              ctx = platforms::Control::PktRes::from(tx);
    // SetEpId command
    app::Packet SetEpId_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::SetEpId)}
    };
    auto& crx     = platforms::Control::PktRes::from(SetEpId_rx);
    crx.tag_owner = 1;
    crx.rq        = 1;
    crx.d         = 0;
    control.process(SetEpId_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::SetEpId));
    // GetEpId command
    app::Packet const GetEpId_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetEpId)}
    };
    control.process(GetEpId_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::GetEpId));
    // GetEpUuid command
    app::Packet const GetEpUuid_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetEpUuid)}
    };
    control.process(GetEpUuid_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::GetEpUuid));
    // GetMctpVerSupport command
    app::Packet const GetMctpVerSupport_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetMctpVerSupport)}
    };
    control.process(GetMctpVerSupport_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::GetMctpVerSupport));
    // GetMsgTypeSupport command
    app::Packet const GetMsgTypeSupport_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetMsgTypeSupport)}
    };
    control.process(GetMsgTypeSupport_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::GetMsgTypeSupport));
    // GetVndrMsgSupport command
    app::Packet const GetVndrMsgSupport_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetVndrMsgSupport)}
    };
    control.process(GetVndrMsgSupport_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::GetVndrMsgSupport));
    // AllocateEpId command
    app::Packet const AllocateEpId_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::AllocateEpId)}
    };
    control.process(AllocateEpId_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::AllocateEpId));
    // GetRoutTableEntry command
    app::Packet const GetRoutTableEntry_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetRoutTableEntry)}
    };
    control.process(GetRoutTableEntry_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::GetRoutTableEntry));
    // PrepareEpDiscovery command
    app::Packet const PrepareEpDiscovery_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::PrepareEpDiscovery)}
    };
    control.process(PrepareEpDiscovery_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::PrepareEpDiscovery));
    // EpDiscovery command
    app::Packet const EpDiscovery_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::EpDiscovery)}
    };
    control.process(EpDiscovery_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.command_code),
                  static_cast<uint8_t>(app::Cmd::EpDiscovery));
    // DiscoveryNotify command
    app::Packet const DiscoveryNotify_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::DiscoveryNotify)}
    };
    control.process(DiscoveryNotify_rx, tx);
    // UnsupportCmd command
    app::Packet const UnsupportCmd_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, 0xFF}
    };
    control.process(UnsupportCmd_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorUnsupportedCmd));
};

class CommandTestHelper : public platforms::Control
{
public:
    using platforms::Control::_allocate_eid_bus;
    using platforms::Control::_num_enumerate_eid;
    using platforms::Control::_start_eid;
    using platforms::Control::EidPoolSize;
    using platforms::Control::EidSize;
    uint8_t IanaNextVendorIdSetSelector = 0x01;
    uint8_t IanaVendorIdFormat          = 0x01;
    uint8_t IanaVendorIdByte0           = 0x00;
    uint8_t IanaVendorIdByte1           = 0x00;
    uint8_t IanaVendorIdByte2           = 0x16;
    uint8_t IanaVendorIdByte3           = 0x47;
    uint8_t NoMoreCapability            = 0xFF;
    using platforms::Control::on_allocate_endpoint_id;
    using platforms::Control::on_discovery_notify;
    using platforms::Control::on_endpoint_discovery;
    using platforms::Control::on_get_endpoint_id;
    using platforms::Control::on_get_endpoint_uuid;
    using platforms::Control::on_get_mctp_version_support;
    using platforms::Control::on_get_msg_type_support;
    using platforms::Control::on_get_routing_table_entry;
    using platforms::Control::on_get_vendor_msg_support;
    using platforms::Control::on_prepare_endpoint_discovery;
    using platforms::Control::on_set_endpoint_id;
    uint8_t PciVendorIdFormat = 0x00;
    uint8_t PciVendorIdByte0  = 0x10;
    uint8_t PciVendorIdByte1  = 0xDE;
    using platforms::Control::PhyAddrSize;
    using platforms::Control::PortNum;
    using platforms::Control::RoutingEntryNum;
    uint8_t SupportedTypeNum = platforms::SupportedTypeNum;
    using platforms::Control::_additional_eid_count;
    using platforms::Control::NoNextEntry;
    using platforms::Control::on_routing_info_update;
};

UBS_TEST(Control, OnSetEndpointIdTest)
{
    CommandTestHelper control;
    // type == app::PacketType::Request, crx.data[0] ==
    // app::SetEndpoint::SetEidNormal
    app::Packet tx1{};
    auto&       ctx = platforms::Control::PktRes::from(tx1);
    app::Packet rx1{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver = 0,
                   .rsvd    = 0,
                   .dst_eid = 0x01,
                   .src_eid = 0x02,
                   },
        .msg = {
                   0x00, 0x00,
                   static_cast<uint8_t>(app::Cmd::SetEpId),
                   static_cast<uint8_t>(app::SetEndpoint::SetEidNormal),
                   }
    };
    auto& crx     = platforms::Control::PktReq::from(rx1);
    crx.tag_owner = 1;
    crx.rq        = 1;
    crx.d         = 0;
    control.on_set_endpoint_id(rx1, tx1);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]),
                  static_cast<uint8_t>(app::SetEndpoint::EidAcceptedAndEidPool));
    ensure::is_eq(
        static_cast<uint8_t>(ctx.data[1]),
        platforms::get_cur_eid(control.router(), platforms::get_packet_interface(rx1)));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]), control.EidPoolSize);
    // type == app::PacketType::Request, crx.data[0] ==
    // app::SetEndpoint::SetEidForced
    app::Packet tx2{};
    app::Packet rx2{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver = 0,
                   .rsvd    = 0,
                   .dst_eid = 0x01,
                   .src_eid = 0x02,
                   },
        .msg = {
                   0x00, 0x00,
                   static_cast<uint8_t>(app::Cmd::SetEpId),
                   static_cast<uint8_t>(app::SetEndpoint::SetEidForced),
                   }
    };
    auto& ctx2     = platforms::Control::PktRes::from(tx2);
    auto& crx2     = platforms::Control::PktReq::from(rx2);
    crx2.tag_owner = 1;
    crx2.rq        = 1;
    crx2.d         = 0;
    control.on_set_endpoint_id(rx2, tx2);
    ensure::is_eq(static_cast<uint8_t>(ctx2.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx2.data[0]),
                  static_cast<uint8_t>(app::SetEndpoint::EidAcceptedAndEidPool));
    ensure::is_eq(
        static_cast<uint8_t>(ctx2.data[1]),
        platforms::get_cur_eid(control.router(), platforms::get_packet_interface(rx2)));
    ensure::is_eq(static_cast<uint8_t>(ctx2.data[2]), control.EidPoolSize);
    // type == app::PacketType::Request, crx.data[0] ==
    // app::SetEndpoint::SetEidReset
    app::Packet tx3{};
    app::Packet rx3{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver = 0,
                   .rsvd    = 0,
                   .dst_eid = 0x01,
                   .src_eid = 0x02,
                   },
        .msg = {
                   0x00, 0x00,
                   static_cast<uint8_t>(app::Cmd::SetEpId),
                   static_cast<uint8_t>(app::SetEndpoint::SetEidReset),
                   }
    };
    auto& ctx3     = platforms::Control::PktRes::from(tx3);
    auto& crx3     = platforms::Control::PktReq::from(rx3);
    crx3.tag_owner = 1;
    crx3.rq        = 1;
    crx3.d         = 0;
    control.on_set_endpoint_id(rx3, tx3);
    ensure::is_eq(static_cast<uint8_t>(ctx3.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    // type == app::PacketType::Request, crx.data[0] ==
    // app::SetEndpoint::SetEidDiscovered
    app::Packet tx4{};
    app::Packet rx4{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver = 0,
                   .rsvd    = 0,
                   .dst_eid = 0x01,
                   .src_eid = 0x02,
                   },
        .msg = {
                   0x00, 0x00,
                   static_cast<uint8_t>(app::Cmd::SetEpId),
                   static_cast<uint8_t>(app::SetEndpoint::SetEidDiscovered),
                   }
    };
    auto& ctx4     = platforms::Control::PktRes::from(tx4);
    auto& crx4     = platforms::Control::PktReq::from(rx4);
    crx4.tag_owner = 1;
    crx4.rq        = 1;
    crx4.d         = 0;
    control.on_set_endpoint_id(rx4, tx4);
    ensure::is_eq(static_cast<uint8_t>(ctx4.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx4.data[0]),
                  static_cast<uint8_t>(app::SetEndpoint::EidAcceptedAndEidPool));
    ensure::is_eq(
        static_cast<uint8_t>(ctx4.data[1]),
        platforms::get_cur_eid(control.router(), platforms::get_packet_interface(rx4)));
    ensure::is_eq(static_cast<uint8_t>(ctx4.data[2]), control.EidPoolSize);
    // type == app::PacketType::Request, crx.data[0] == others
    app::Packet tx5{};
    app::Packet rx5{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver = 0,
                   .rsvd    = 0,
                   .dst_eid = 0x01,
                   .src_eid = 0x02,
                   },
        .msg = {
                   0x00, 0x00,
                   static_cast<uint8_t>(app::Cmd::SetEpId),
                   0xFF, }
    };
    auto& ctx5     = platforms::Control::PktRes::from(tx5);
    auto& crx5     = platforms::Control::PktReq::from(rx5);
    crx5.tag_owner = 1;
    crx5.rq        = 1;
    crx5.d         = 0;
    control.on_set_endpoint_id(rx5, tx5);
    ensure::is_eq(static_cast<uint8_t>(ctx5.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    // type == app::PacketType::Response
    app::Packet tx6{};
    app::Packet rx6{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver = 0,
                   .rsvd    = 0,
                   .dst_eid = 0x01,
                   .src_eid = 0x02,
                   },
        .msg = {
                   0x00, 0x00,
                   static_cast<uint8_t>(app::Cmd::SetEpId),
                   0xFF, }
    };
    auto& ctx6     = platforms::Control::PktRes::from(tx6);
    auto& crx6     = platforms::Control::PktReq::from(rx6);
    crx6.tag_owner = 0;
    crx6.rq        = 0;
    crx6.d         = 0;
    control.on_set_endpoint_id(rx6, tx6);
    ensure::is_eq(0, platforms::get_packet_length(tx6));
};

UBS_TEST(Control, OnGetEndpointIdTest)
{
    CommandTestHelper control;
    app::Packet const rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = 0x01,
                   .reserved0        = 0,
                   },
    };
    app::Packet tx{};
    auto&       ctx = platforms::Control::PktRes::from(tx);
    uint8_t     eid = 0x01;
    platforms::set_cur_eid(control.router(), platforms::get_packet_interface(rx.priv), eid);
    control.on_get_endpoint_id(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), static_cast<uint8_t>(eid));
    const uint8_t endpoint_type = (control.EidPoolSize == 0)
                                    ? static_cast<uint8_t>(app::EndpointType::SimpleEndpoint)
                                    : static_cast<uint8_t>(app::EndpointType::BusOwnerBridge);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]),
                  ((endpoint_type & 0b11) << app::EndpointTypeShift)
                      | ((static_cast<uint8_t>(app::EndpointIdType::DynamicEid) & 0b11)));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]), 0x00);
    ensure::is_eq(static_cast<uint8_t>(platforms::get_packet_length(tx)),
                  sizeof(app::TransportHeader) + control.HeaderSizeResponse + 3);
    eid = 0x02;
    platforms::set_cur_eid(control.router(), platforms::get_packet_interface(rx.priv), eid);
    control.on_get_endpoint_id(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), static_cast<uint8_t>(eid));
};

UBS_TEST(Control, OnGetEndpointUuidTest)
{
    CommandTestHelper control;
    app::Packet const rx{};
    app::Packet       tx{};
    auto&             ctx                 = platforms::Control::PktRes::from(tx);
    platforms::get_uuid(control.router()) = {0x01,
                                             0x02,
                                             0x03,
                                             0x04,
                                             0x05,
                                             0x06,
                                             0x07,
                                             0x08,
                                             0x09,
                                             0x0A,
                                             0x0B,
                                             0x0C,
                                             0x0D,
                                             0x0E,
                                             0x0F,
                                             0x10};
    control.on_get_endpoint_uuid(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    for (int i = 0; i < platforms::get_uuid(control.router()).size(); i++) {
        ensure::is_eq(static_cast<uint8_t>(ctx.data[i]),
                      static_cast<uint8_t>(platforms::get_uuid(control.router()).data()[i]));
    }
    platforms::get_uuid(control.router()) = {0x10,
                                             0x0F,
                                             0x0E,
                                             0x0D,
                                             0x0C,
                                             0x0B,
                                             0x0A,
                                             0x09,
                                             0x08,
                                             0x07,
                                             0x06,
                                             0x05,
                                             0x04,
                                             0x03,
                                             0x02,
                                             0x01};
    control.on_get_endpoint_uuid(rx, tx);
    for (int i = 0; i < platforms::get_uuid(control.router()).size(); i++) {
        ensure::is_eq(static_cast<uint8_t>(ctx.data[i]),
                      static_cast<uint8_t>(platforms::get_uuid(control.router()).data()[i]));
    }
};

UBS_TEST(Control, OnGetMctpVersionSupportTest)
{
    CommandTestHelper control;
    app::Packet       tx{};
    auto&             ctx = platforms::Control::PktRes::from(tx);
    uint32_t          ver;
    // Base
    app::Packet const Base_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::GetMctpVerSupport),
                   static_cast<uint8_t>(app::VersionType::Base)}
    };
    control.on_get_mctp_version_support(Base_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), 1);
    memcpy(&ver, &ctx.data[1], sizeof(uint32_t));
    ensure::is_eq(static_cast<uint32_t>(ver), static_cast<uint32_t>(app::Version::Base));
    // ControlMsg
    app::Packet const ControlMsg_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::GetMctpVerSupport),
                   static_cast<uint8_t>(app::VersionType::ControlMsg)}
    };
    control.on_get_mctp_version_support(ControlMsg_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), 1);
    memcpy(&ver, &ctx.data[1], sizeof(uint32_t));
    ensure::is_eq(static_cast<uint32_t>(ver), static_cast<uint32_t>(app::Version::ControlMsg));
    // Pldm
    app::Packet const Pldm_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::GetMctpVerSupport),
                   static_cast<uint8_t>(app::VersionType::Pldm)}
    };
    control.on_get_mctp_version_support(Pldm_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), 1);
    memcpy(&ver, &ctx.data[1], sizeof(uint32_t));
    ensure::is_eq(static_cast<uint32_t>(ver), static_cast<uint32_t>(app::Version::Pldm));
    // Vendor
    app::Packet const Vendor_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::GetMctpVerSupport),
                   static_cast<uint8_t>(app::VersionType::Vendor)}
    };
    control.on_get_mctp_version_support(Vendor_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), 1);
    memcpy(&ver, &ctx.data[1], sizeof(uint32_t));
    ensure::is_eq(static_cast<uint32_t>(ver), static_cast<uint32_t>(app::Version::Vendor));
    // Invalid
    app::Packet const Invalid_rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetMctpVerSupport), 0x02}
    };
    control.on_get_mctp_version_support(Invalid_rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
};

UBS_TEST(Control, OnGetMsgTypeSupportTest)
{
    CommandTestHelper control;
    app::Packet const rx{};
    app::Packet       tx{};
    auto&             ctx = platforms::Control::PktRes::from(tx);
    control.on_get_msg_type_support(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]),
                  static_cast<uint8_t>(control.SupportedTypeNum));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]), static_cast<uint8_t>(app::MsgType::Pldm));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]), static_cast<uint8_t>(app::MsgType::Spdm));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[3]),
                  static_cast<uint8_t>(app::MsgType::VendorPci));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[4]),
                  static_cast<uint8_t>(app::MsgType::VendorIani));
};

UBS_TEST(Control, OnGetVendorMsgSupportTest)
{
    CommandTestHelper control;
    app::Packet       tx{};
    auto&             ctx = platforms::Control::PktRes::from(tx);
    // crx.data[0] == 0
    app::Packet const rx0{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetVndrMsgSupport), 0x00}
    };
    control.on_get_vendor_msg_support(rx0, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), control.IanaNextVendorIdSetSelector);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]), control.IanaVendorIdFormat);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]),
                  static_cast<uint8_t>(control.IanaVendorIdByte0));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[3]),
                  static_cast<uint8_t>(control.IanaVendorIdByte1));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[4]),
                  static_cast<uint8_t>(control.IanaVendorIdByte2));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[5]),
                  static_cast<uint8_t>(control.IanaVendorIdByte3));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[6]), 0x00);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[7]), 0x00);
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse + 8));
    // crx.data[0] == 1
    app::Packet const rx1{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetVndrMsgSupport), 0x01}
    };
    control.on_get_vendor_msg_support(rx1, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), control.NoMoreCapability);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]), control.PciVendorIdFormat);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]),
                  static_cast<uint8_t>(control.PciVendorIdByte0));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[3]),
                  static_cast<uint8_t>(control.PciVendorIdByte1));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[4]), 0x00);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[5]), 0x00);
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse + 6));
    // crx.data[0] == NoMoreCapability
    app::Packet const rx2{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::GetVndrMsgSupport),
                   control.NoMoreCapability}
    };
    control.on_get_vendor_msg_support(rx2, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), control.NoMoreCapability);
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse + 1));
};

UBS_TEST(Control, OnAllocateEndpointIdTest)
{
    CommandTestHelper control;
    app::Packet       tx{};
    auto&             ctx = platforms::Control::PktRes::from(tx);
    // _allocate_eid_bus == static_cast<uint8_t>(platforms::Interface::End)
    control._allocate_eid_bus = static_cast<uint8_t>(platforms::Interface::End);
    app::Packet const rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::AllocateEpId),
                   static_cast<uint8_t>(app::AllocateEndpoint::ForceAllocate)}
    };
    control.on_allocate_endpoint_id(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]),
                  static_cast<uint8_t>(app::AllocateEndpointStatus::Accept));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]), control.EidPoolSize);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]), control.get_start_eid());
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse + 3));
    // _allocate_eid_bus == platforms::get_packet_interface(rx)
    control._allocate_eid_bus = platforms::get_packet_interface(rx);
    control.on_allocate_endpoint_id(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]),
                  static_cast<uint8_t>(app::AllocateEndpointStatus::Accept));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]), control.EidPoolSize);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]), control.get_start_eid());
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse + 3));
    // _allocate_eid_bus != platforms::get_packet_interface(rx)
    control._allocate_eid_bus = static_cast<uint8_t>(platforms::Interface::UsUsb);
    control.on_allocate_endpoint_id(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]),
                  static_cast<uint8_t>(app::AllocateEndpointStatus::Reject));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]), control.EidPoolSize);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]), control.get_start_eid());
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse + 3));
};

UBS_TEST(Control, OnPrepareEndpointDiscoveryTest)
{
    CommandTestHelper control;
    app::Packet const rx{};
    app::Packet       tx{};
    auto&             ctx = platforms::Control::PktRes::from(tx);
    control.on_prepare_endpoint_discovery(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse));
};

UBS_TEST(Control, OnEndpointDiscoveryTest)
{
    CommandTestHelper control;
    app::Packet const rx{};
    app::Packet       tx{};
    auto&             ctx = platforms::Control::PktRes::from(tx);
    control.on_endpoint_discovery(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse));
};

UBS_TEST(Control, OnDiscoveryNotifyTest)
{
    CommandTestHelper control;
    app::Packet       rx{};
    auto&             crx = app::Control::PktReq::from(rx);
    app::Packet       tx{};
    auto&             ctx = platforms::Control::PktRes::from(tx);
    // request
    crx.tag_owner    = 1;
    crx.rq           = 1;
    crx.d            = 0;
    crx.command_code = app::Cmd::DiscoveryNotify;
    control.on_discovery_notify(rx, tx);
    // response
    crx.tag_owner    = 0;
    crx.rq           = 0;
    crx.d            = 0;
    crx.command_code = app::Cmd::DiscoveryNotify;
    control.on_discovery_notify(rx, tx);
    ensure::is_eq(platforms::get_packet_length(tx), 0);
};

UBS_TEST(Control, OnGetRoutingTableEntryTest)
{
    // cur_entry (crx.data[0]) > _num_enumerate_eid
    CommandTestHelper control;
    control._num_enumerate_eid = 0x00;
    app::Packet const rx{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00, 0x00, static_cast<uint8_t>(app::Cmd::GetRoutTableEntry), 0x01}
    };
    app::Packet tx{};
    auto&       ctx = platforms::Control::PktRes::from(tx);
    control.on_get_routing_table_entry(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    // cur_entry (crx.data[0]) = _num_enumerate_eid
    control._num_enumerate_eid = 0x01;
    control.on_get_routing_table_entry(rx, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[0]), static_cast<uint8_t>(control.NoNextEntry));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[1]),
                  static_cast<uint8_t>(control.RoutingEntryNum));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[2]), static_cast<uint8_t>(control.EidSize));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[3]),
                  static_cast<uint8_t>(platforms::get_cur_eid(
                      control.router(), platforms::get_packet_interface(rx))));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[4]), static_cast<uint8_t>(control.PortNum));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[5]),
                  static_cast<uint8_t>(app::PhyId::MctpOverUsb));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[6]),
                  static_cast<uint8_t>(app::PhyMediumId::Usb20));
    ensure::is_eq(static_cast<uint8_t>(ctx.data[7]), control.PhyAddrSize);
    ensure::is_eq(static_cast<uint8_t>(ctx.data[8]), 0x00);
    ensure::is_eq(
        static_cast<uint8_t>(platforms::get_packet_length(tx)),
        static_cast<uint8_t>(sizeof(app::TransportHeader) + control.HeaderSizeResponse + 9));
};

UBS_TEST(Control, OnUpdateRoutingInfoTest)
{
    CommandTestHelper control;
    app::Packet       tx{};
    auto&             ctx = platforms::Control::PktRes::from(tx);
    // assume the bus owner is UsI2c
    control._allocate_eid_bus = static_cast<uint8_t>(platforms::Interface::UsI2c);
    // packet interface is not the bus owner
    app::Packet const rx1{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsUsb),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x0C}
    };
    control.on_routing_info_update(rx1, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    // update entry is not 0 or 1
    app::Packet const rx2{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x02, 0x00,
                   0x01, 0x0CC,
                   0x51}
    };
    control.on_routing_info_update(rx2, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    // entry type is not Simple Endpoint
    app::Packet const rx3{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x01,
                   0x01, 0x0C,
                   0x51}
    };
    control.on_routing_info_update(rx3, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    // eid range is not 1
    app::Packet const rx4{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x02, 0x0C,
                   0x51}
    };
    control.on_routing_info_update(rx4, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    // eid already exists
    control.routing_map().at(0x00).assigned_eid = 0x0A;
    app::Packet const rx5{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x0A,
                   0x51}
    };
    control.on_routing_info_update(rx5, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    // check the value of _additional_eid_count and the values in routing_map()
    app::Packet const rx6{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x0C,
                   0x52}
    };
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x00);
    control.on_routing_info_update(rx6, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x01);
    uint8_t index = platforms::DefaultRoutingTableSize + control._additional_eid_count - 1;
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).assigned_eid), 0x0C);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).client),
                  static_cast<uint8_t>(platforms::Interface::UsI2c));
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).is_enumerated), true);
    ensure::is_eq(
        static_cast<uint8_t>(control.get_additional_address(control._additional_eid_count - 1)),
        0x52);
    app::Packet const rx7{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x0D,
                   0x53}
    };
    control.on_routing_info_update(rx7, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x02);
    index = platforms::DefaultRoutingTableSize + control._additional_eid_count - 1;
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).assigned_eid), 0x0D);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).client),
                  static_cast<uint8_t>(platforms::Interface::UsI2c));
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).is_enumerated), true);
    ensure::is_eq(
        static_cast<uint8_t>(control.get_additional_address(control._additional_eid_count - 1)),
        0x53);
    app::Packet const rx8{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x0E,
                   0x54}
    };
    control.on_routing_info_update(rx8, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x03);
    index = platforms::DefaultRoutingTableSize + control._additional_eid_count - 1;
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).assigned_eid), 0x0E);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).client),
                  static_cast<uint8_t>(platforms::Interface::UsI2c));
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).is_enumerated), true);
    ensure::is_eq(
        static_cast<uint8_t>(control.get_additional_address(control._additional_eid_count - 1)),
        0x54);
    app::Packet const rx9{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x0F,
                   0x55}
    };
    control.on_routing_info_update(rx9, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::Success));
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x04);
    index = platforms::DefaultRoutingTableSize + control._additional_eid_count - 1;
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).assigned_eid), 0x0F);
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).client),
                  static_cast<uint8_t>(platforms::Interface::UsI2c));
    ensure::is_eq(static_cast<uint8_t>(control.routing_map().at(index).is_enumerated), true);
    ensure::is_eq(
        static_cast<uint8_t>(control.get_additional_address(control._additional_eid_count - 1)),
        0x55);
    // _additional_eid_count is full (4)
    app::Packet const rx10{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x10,
                   0x56}
    };
    control.on_routing_info_update(rx10, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInsufficientSpace));
    control._additional_eid_count = 0;  // reset additional eid count after test
    // update eid is an current eid
    platforms::set_cur_eid(
        control.router(), static_cast<uint8_t>(platforms::Interface::UsI2c), 0x0B);
    platforms::set_cur_eid(
        control.router(), static_cast<uint8_t>(platforms::Interface::UsUsb), 0x0C);
    app::Packet const rx11{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x0B,
                   0x52}
    };
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x00);
    control.on_routing_info_update(rx11, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x00);
    app::Packet const rx12{
        .priv =
            {
                   .packet_length    = 0,
                   .packet_interface = static_cast<uint8_t>(platforms::Interface::UsUsb),
                   .reserved0        = 0,
                   },
        .hdr =
            {
                   .hdr_ver   = 0,
                   .rsvd      = 0,
                   .dst_eid   = 0x01,
                   .src_eid   = 0x02,
                   .msg_tag   = 0x1,
                   .tag_owner = 0,
                   .pkt_seq   = 0,
                   .eom       = 1,
                   .som       = 1,
                   },
        .msg = {0x00,
                   0x00, static_cast<uint8_t>(app::Cmd::RoutingInfoUpdate),
                   0x01, 0x00,
                   0x01, 0x0C,
                   0x52}
    };
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x00);
    control.on_routing_info_update(rx12, tx);
    ensure::is_eq(static_cast<uint8_t>(ctx.completion_code),
                  static_cast<uint8_t>(platforms::Ccode::ErrorInvalidData));
    ensure::is_eq(static_cast<uint8_t>(control._additional_eid_count), 0x00);
};
