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
#include "app/pdk-mctp-app-vendor.h"

#include "app/pdk-mctp-app-packet-plat.h"
#include "pdk-cmn-flowcontrol.h"

using namespace pdk::mctp::app;

PacketType Vendor::get_vendor_packet_type(const Packet& buf)
{
    auto& vdr = app::VendorPktReq::from(buf);
    if (vdr.tag_owner == 1 && vdr.rq == 1 && vdr.d == 0) {
        return app::PacketType::Request;
    }
    if (vdr.tag_owner == 0 && vdr.rq == 0 && vdr.d == 0) {
        return app::PacketType::Response;
    }
    if (vdr.tag_owner == 1 && vdr.rq == 1 && vdr.d == 1) {
        return app::PacketType::Datagram;
    }

    return app::PacketType::Other;
}

void Vendor::unsupported_command(const Packet& rx, Packet& tx) const
{
    switch (get_vendor_packet_type(rx)) {
        case PacketType::Request:
            fill_error_packet(platforms::Ccode::ErrorUnsupportedCmd, rx, tx);
            break;
        case PacketType::Response: platforms::set_packet_length(tx, 0); break;
        default                  : break;
    }
}

void Vendor::fill_vendor_msg_header(const Packet& rx, Packet& tx) const
{
    auto& vrx           = VendorPktReq::from(rx);
    auto& vtx           = VendorPktRes::from(tx);
    vtx.msg_type        = MsgType::VendorIani;
    vtx.iana            = vrx.iana;  // nvidia = 0x1647
    vtx.d               = 0;
    vtx.rq              = 0;
    vtx.rsvd0           = 0;
    vtx.instance_id     = vrx.instance_id;
    vtx.vendor_msg_type = 0x01;
    vtx.command_code    = vrx.command_code;
    vtx.msg_version     = vrx.msg_version;
}

void Vendor::fill_error_packet(platforms::Ccode code, const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vtx           = VendorPktRes::from(tx);
    vtx.completion_code = code;
    platforms::set_packet_length(tx, sizeof(app::TransportHeader) + HeaderSizeResponse);
}

void Vendor::fill_packet_header(const Packet& rx, Packet& tx) const
{
    pdk::cmn::flowcontrol::corepdk_assert(&rx != &tx, "rx and tx are the same buffer");

    tx.hdr.rsvd    = 0;
    tx.hdr.hdr_ver = 1;
    tx.hdr.dst_eid = rx.hdr.src_eid;
    tx.hdr.src_eid = rx.hdr.dst_eid;
    tx.hdr.som     = 1;
    tx.hdr.eom     = 1;
    tx.hdr.pkt_seq = 0;

    // for response packet
    auto& vdr        = VendorPktRes::from(tx);
    tx.hdr.tag_owner = (vdr.rq) ? 1 : 0;
    tx.hdr.msg_tag   = rx.hdr.msg_tag;

    tx.priv = {};
    platforms::set_packet_interface(tx, platforms::get_packet_interface(rx));
    platforms::set_packet_length(tx, sizeof(app::TransportHeader) + HeaderSizeResponse);

    _ctl.update_eid(tx, platforms::get_packet_interface(rx));
}
