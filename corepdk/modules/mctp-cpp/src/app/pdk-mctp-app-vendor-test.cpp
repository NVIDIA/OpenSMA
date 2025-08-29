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
#include "app/pdk-mctp-app-packet-plat.h"
#include "app/pdk-mctp-app-router-plat.h"
#include "app/pdk-mctp-app-vendor.h"
#include "ubs/unittest.hpp"

using namespace ubs::unittest;
using namespace pdk::mctp;

UBS_TEST(Vendor, FromTest)
{
    app::Packet pkt{};
    auto&       vrx = app::VendorPktReq::from(pkt);
    ensure::is_eq(&vrx, &pkt.hdr);
    auto& vtx = app::VendorPktRes::from(pkt);
    ensure::is_eq(&vtx, &pkt.hdr);
};

// Helper class for testing app::Vendor's protected methods and constants
class VendorTestHelper : public app::Vendor
{
public:
    VendorTestHelper(platforms::Control& ctl) : app::Vendor(ctl) {}
    using app::Vendor::fill_error_packet;
    using app::Vendor::fill_packet_header;
    using app::Vendor::fill_vendor_msg_header;
    using app::Vendor::HeaderSizeResponse;
    using app::Vendor::unsupported_command;
};

UBS_TEST(Vendor, FillPacketHeaderTest)
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
    app::Packet      tx{};
    VendorTestHelper helper{control};
    platforms::set_cur_eid(control.router(), 0x01, 0x01);
    auto& vtx = app::VendorPktRes::from(tx);
    vtx.rq    = 0;
    helper.fill_packet_header(rx, tx);
    ensure::is_eq(uint8_t(tx.hdr.rsvd), 0);
    ensure::is_eq(uint8_t(tx.hdr.hdr_ver), 1);
    ensure::is_eq(uint8_t(tx.hdr.dst_eid), rx.hdr.src_eid);
    // src_eid should be NULL_EID(0x00) if eid is not set yet
    ensure::is_eq(uint8_t(tx.hdr.src_eid), app::NULL_EID);
    ensure::is_eq(uint8_t(tx.hdr.som), 1);
    ensure::is_eq(uint8_t(tx.hdr.eom), 1);
    ensure::is_eq(uint8_t(tx.hdr.pkt_seq), 0);
    ensure::is_eq(uint8_t(vtx.rq), 0);
    ensure::is_eq(uint8_t(tx.hdr.tag_owner), 0);
    ensure::is_eq(uint8_t(tx.hdr.msg_tag), rx.hdr.msg_tag);
    ensure::is_eq(platforms::get_packet_interface(tx), platforms::get_packet_interface(rx));
    ensure::is_eq(uint8_t(platforms::get_packet_length(tx)),
                  sizeof(app::TransportHeader) + helper.HeaderSizeResponse);
    ensure::is_eq(tx.hdr.src_eid,
                  platforms::get_cur_eid(control.router(),
                                         static_cast<uint8_t>(platforms::Interface::UsI2c)));
    vtx.rq = 1;
    helper.fill_packet_header(rx, tx);
    ensure::is_eq(uint8_t(vtx.rq), 1);
};

UBS_TEST(Vendor, FillVendorMsgHeaderTest)
{
    platforms::Control ctl{};
    VendorTestHelper   helper{ctl};
    app::Packet const  rx{};
    app::Packet        tx{};
    auto&              vrx = app::VendorPktReq::from(rx);
    auto&              vtx = app::VendorPktRes::from(tx);
    helper.fill_vendor_msg_header(rx, tx);
    ensure::is_eq(uint8_t(vtx.msg_type), uint8_t(app::MsgType::VendorIani));
    ensure::is_eq(uint32_t(vtx.iana), uint32_t(vrx.iana));
    ensure::is_eq(uint8_t(vtx.d), 0);
    ensure::is_eq(uint8_t(vtx.rq), 0);
    ensure::is_eq(uint8_t(vtx.rsvd0), 0);
    ensure::is_eq(uint8_t(vtx.instance_id), uint8_t(vrx.instance_id));
    ensure::is_eq(uint8_t(vtx.vendor_msg_type), 0x01);
    ensure::is_eq(uint8_t(vtx.command_code), uint8_t(vrx.command_code));
    ensure::is_eq(uint8_t(vtx.msg_version), uint8_t(vrx.msg_version));
};

UBS_TEST(Vendor, FillErrorPacketTest)
{
    platforms::Control ctl{};
    VendorTestHelper   helper{ctl};
    app::Packet const  rx{};
    app::Packet        tx{};
    auto&              vtx = app::VendorPktRes::from(tx);
    helper.fill_error_packet(platforms::Ccode::ErrorNonceMismatch, rx, tx);
    ensure::is_eq(uint8_t(vtx.completion_code), uint8_t(platforms::Ccode::ErrorNonceMismatch));
    ensure::is_eq(uint8_t(platforms::get_packet_length(tx)),
                  sizeof(app::TransportHeader) + helper.HeaderSizeResponse);

    helper.fill_error_packet(platforms::Ccode::ErrorDebugTokenInstalled, rx, tx);
    ensure::is_eq(uint8_t(vtx.completion_code),
                  uint8_t(platforms::Ccode::ErrorDebugTokenInstalled));
    ensure::is_eq(uint8_t(platforms::get_packet_length(tx)),
                  sizeof(app::TransportHeader) + helper.HeaderSizeResponse);
};

UBS_TEST(Vendor, GetVendorPacketTypeTest)
{
    platforms::Control ctl{};
    VendorTestHelper   helper{ctl};
    app::Packet        rx{};
    auto&              vrx = app::VendorPktReq::from(rx);
    vrx.tag_owner          = 1;
    vrx.rq                 = 1;
    vrx.d                  = 0;
    ensure::is_eq(helper.get_vendor_packet_type(rx), app::PacketType::Request);
    vrx.tag_owner = 0;
    vrx.rq        = 0;
    vrx.d         = 0;
    ensure::is_eq(helper.get_vendor_packet_type(rx), app::PacketType::Response);
    vrx.tag_owner = 1;
    vrx.rq        = 1;
    vrx.d         = 1;
    ensure::is_eq(helper.get_vendor_packet_type(rx), app::PacketType::Datagram);
    vrx.tag_owner = 0;
    vrx.rq        = 0;
    vrx.d         = 1;
    ensure::is_eq(helper.get_vendor_packet_type(rx), app::PacketType::Other);
};

UBS_TEST(Vendor, UnsupportedCommandTest)
{
    platforms::Control ctl{};
    VendorTestHelper   helper{ctl};
    app::Packet        tx{};
    app::Packet        rx{};
    auto&              vrx = app::VendorPktReq::from(rx);
    auto&              vtx = app::VendorPktRes::from(tx);
    // rx is a request packet
    vrx.tag_owner = 1;
    vrx.rq        = 1;
    vrx.d         = 0;
    helper.unsupported_command(rx, tx);
    ensure::is_eq(uint8_t(vtx.completion_code), uint8_t(platforms::Ccode::ErrorUnsupportedCmd));
    // rx is a response packet
    vrx.tag_owner = 0;
    vrx.rq        = 0;
    vrx.d         = 0;
    helper.unsupported_command(rx, tx);
    ensure::is_eq(uint8_t(platforms::get_packet_length(tx)), 0);
};