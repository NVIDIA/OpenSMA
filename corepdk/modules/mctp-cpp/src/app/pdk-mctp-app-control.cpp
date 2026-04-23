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
#include "app/pdk-mctp-app-control.h"

#include <cstring>

#include "app/pdk-mctp-app-control-plat.h"
#include "app/pdk-mctp-app-packet-plat.h"
#include "app/pdk-mctp-app-router-plat.h"
#include "pdk-cmn-flowcontrol.h"
#include "pdk-mctp-platforms-nsm.h"

using namespace pdk::mctp::app;

PacketType Control::get_packet_type(const Packet& buf)
{
    auto& ctl = Control::PktReq::from(buf);
    if (ctl.tag_owner == 1 && ctl.rq == 1 && ctl.d == 0) {
        return PacketType::Request;
    }
    if (ctl.tag_owner == 0 && ctl.rq == 0 && ctl.d == 0) {
        return PacketType::Response;
    }
    if (ctl.tag_owner == 1 && ctl.rq == 1 && ctl.d == 1) {
        return PacketType::Datagram;
    }
    return PacketType::Other;
}

void Control::on_get_endpoint_id(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);

    auto& ctl           = Control::PktRes::from(tx);
    ctl.completion_code = platforms::Ccode::Success;
    ctl.data[0]         = platforms::get_cur_eid(_router, platforms::get_packet_interface(rx));
    const uint8_t endpoint_type = (EidPoolSize == 0)
                                    ? static_cast<uint8_t>(EndpointType::SimpleEndpoint)
                                    : static_cast<uint8_t>(EndpointType::BusOwnerBridge);
    // [5:4] endpoint type, [1:0] endpoint id type
    ctl.data[1] = ((static_cast<uint8_t>(endpoint_type) & 0b11U) << EndpointTypeShift)
                | ((static_cast<uint8_t>(EndpointIdType::DynamicEid) & 0b11U));

    ctl.data[2] = 0x00;

    platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 3);
}

void Control::on_get_endpoint_uuid(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);

    auto& ctl           = Control::PktRes::from(tx);
    ctl.completion_code = platforms::Ccode::Success;
    memcpy(
        &ctl.data[0], platforms::get_uuid(_router).data(), platforms::get_uuid(_router).size());
    platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 16);
}

void Control::on_get_mctp_version_support(const Packet& rx, Packet& tx) const
{
    auto& crx = Control::PktReq::from(rx);
    auto& ctx = Control::PktRes::from(tx);
    auto  ver = Version::Base;
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);

    switch (VersionType(crx.data[0])) {
        case VersionType::Base      : ver = Version::Base; break;
        case VersionType::ControlMsg: ver = Version::ControlMsg; break;
        case VersionType::Pldm      : ver = Version::Pldm; break;
        case VersionType::Vendor    : ver = Version::Vendor; break;
        default                     : fill_error_packet(platforms::Ccode::ErrorInvalidData, rx, tx); break;
    }

    ctx.data[0] = 1;  //  entry
    memcpy(&ctx.data[1], &ver, sizeof(Version));

    //  1 (entry) + 4 (Version)
    platforms::set_packet_length(
        tx, (sizeof(TransportHeader) + HeaderSizeResponse + 1 + 4) & OneByteMask);
}

void Control::on_get_msg_type_support(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctl           = Control::PktRes::from(tx);
    ctl.completion_code = platforms::Ccode::Success;
    ctl.data[0]         = platforms::SupportedTypeNum;  // count
    memcpy(&ctl.data[1], platforms::SupportedType.data(), platforms::SupportedType.size());
    platforms::set_packet_length(
        tx, sizeof(TransportHeader) + HeaderSizeResponse + 1 + platforms::SupportedTypeNum);
}

void Control::on_get_vendor_msg_support(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctx           = Control::PktRes::from(tx);
    auto& crx           = Control::PktReq::from(rx);
    ctx.completion_code = platforms::Ccode::Success;
    // TODO: verify this format
    if (crx.data[0] == 0) {
        ctx.data[0] = 0x00;             // Vendor ID Set Selector
        ctx.data[1] = 0x01;             // IANI - Vendor ID Data Length (4)
        ctx.data[2] = VendorIaniByte2;  // MSB NVIDIA = 0x1647
        ctx.data[3] = VendorIaniByte1;  //
        ctx.data[4] = 0x00;             //
        ctx.data[5] = 0x00;             //
        ctx.data[6] = 0x00;             // no command set versioning
        ctx.data[7] = 0x00;             //
        platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 8);
    }
    else if (crx.data[0] == 1) {
        ctx.data[0] = 0x01;            // Vendor ID Set Selector
        ctx.data[1] = 0x00;            // PCI - Vendor ID Data Length (2)
        ctx.data[2] = VendorPciByte2;  // NVIDIA = 0x10DE
        ctx.data[3] = VendorPciByte1;  //
        ctx.data[4] = 0x00;            // no command set versioning
        ctx.data[5] = 0x00;            //
        platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 6);
    }
    else {
        ctx.data[0] = NoMoreCapability;  // no more
        platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 1);
    }
}

void Control::on_allocate_endpoint_id(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctx           = Control::PktRes::from(tx);
    auto& crx           = Control::PktReq::from(rx);
    ctx.completion_code = platforms::Ccode::Success;

    if (_allocate_eid_bus == static_cast<uint8_t>(platforms::Interface::End)) {
        _allocate_eid_bus = platforms::get_packet_interface(rx);
    }

    if (_allocate_eid_bus == platforms::get_packet_interface(rx)) {
        // clear the additional eid count and infos by RoutingInfoUpdate if force allocate
        if (crx.data[0] == static_cast<uint8_t>(AllocateEndpoint::ForceAllocate)) {
            for (uint8_t index = platforms::DefaultRoutingTableSize;
                 index < platforms::DefaultRoutingTableSize + _additional_eid_count;
                 index++) {
                _routing_map.at(index).is_need_enumerate = false;
                _routing_map.at(index).is_enumerated     = false;
                _routing_map.at(index).assigned_eid      = 0;
                _routing_map.at(index).client            = static_cast<platforms::Interface>(
                    platforms::Interface::End);
            }
            _additional_eid_count = 0;
        }
        // clear privous enumerate result
        _num_enumerate_eid = 0;

        // store starting endpoint and source eid to allocate
        _start_eid        = crx.data[2];
        _allocate_src_eid = rx.hdr.src_eid;
        _is_enumerated    = true;

        // TODO: should change to index 0-3
        if (EidPoolSize > 0) {
            auto start_eid = crx.data[2];
            for (uint32_t index = 0; index < EidPoolSize; index++) {
                auto& info        = _routing_map.at(index);
                info.assigned_eid = start_eid;

                if (start_eid != UINT8_MAX) {
                    start_eid++;
                }
            }
        }

        // store update stream information
        _routing_map[EidPoolSize].is_enumerated = true;
        _routing_map[EidPoolSize].assigned_eid  = rx.hdr.src_eid;
        _routing_map[EidPoolSize].client        = static_cast<platforms::Interface>(
            platforms::get_packet_interface(rx));

        // use platform specific driver to enumerate
        platforms::on_enumerate_plat();

        ctx.data[0] = static_cast<uint8_t>(AllocateEndpointStatus::Accept);
    }
    else {
        ctx.data[0] = static_cast<uint8_t>(AllocateEndpointStatus::Reject);
    }

    ctx.data[1] = EidPoolSize;
    ctx.data[2] = (EidPoolSize == 0) ? 0 : _start_eid;
    platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 3);
}

void Control::on_routing_info_update(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& crx = Control::PktReq::from(rx);
    auto& ctx = Control::PktRes::from(tx);
    platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse);

    // if RoutingInfoUpdateSize is 0, not support update routing info
    if (platforms::RoutingInfoUpdateSize == 0) {
        ctx.completion_code = platforms::Ccode::ErrorUnsupportedCmd;
        return;
    }

    // only the bus owner can update routing info
    if (_allocate_eid_bus != platforms::get_packet_interface(rx)) {
        ctx.completion_code = platforms::Ccode::ErrorInvalidData;
        return;
    }
    // the update entry count should be 0 or 1
    const uint8_t update_count = crx.data[0];
    if (update_count != 0 && update_count != 1) {
        ctx.completion_code = platforms::Ccode::ErrorInvalidData;
        return;
    }
    // check if the entry type is single endpoint
    const uint8_t entry_type = crx.data[1];
    if ((entry_type & 0b11U) != static_cast<uint8_t>(EntryType::SingleEndpoint)) {
        ctx.completion_code = platforms::Ccode::ErrorInvalidData;
        return;
    }
    // only support update one eid at a time
    const uint8_t eid_range = crx.data[2];
    if (eid_range != 0x01) {
        ctx.completion_code = platforms::Ccode::ErrorInvalidData;
        return;
    }
    // check if the update eid already exists in the routing table
    const uint8_t update_eid = crx.data[3];
    for (auto& info : _routing_map) {
        if (info.assigned_eid == update_eid) {
            ctx.completion_code = platforms::Ccode::ErrorInvalidData;
            return;
        }
    }
    // check if the additional eid count reach the limit RoutingInfoUpdateSize
    if (_additional_eid_count == platforms::RoutingInfoUpdateSize) {
        ctx.completion_code = platforms::Ccode::ErrorInsufficientSpace;
        return;
    }

    // add the new eid to the routing table
    const uint8_t index = platforms::DefaultRoutingTableSize + _additional_eid_count;
    _routing_map.at(index).assigned_eid = update_eid;
    _routing_map.at(index).client       = static_cast<platforms::Interface>(
        platforms::get_packet_interface(rx));
    _routing_map.at(index).is_enumerated = true;

    // if the interface is UsI2c, store the additional i2c address
    // only used in get_rouuting_table_entry command
    if (platforms::get_packet_interface(rx)
        == static_cast<platforms::Packet::InterfaceType>(platforms::Interface::UsI2c)) {
        _additional_address.at(_additional_eid_count) = crx.data[4];
    }

    // increase the additional eid count
    _additional_eid_count++;

    // use platform specific driver to update routing info
    platforms::on_routing_info_update_plat();

    ctx.completion_code = platforms::Ccode::Success;
}

void Control::on_prepare_endpoint_discovery(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctx           = Control::PktRes::from(tx);
    ctx.completion_code = platforms::Ccode::Success;
    platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse);
}

void Control::on_endpoint_discovery(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctl           = Control::PktRes::from(tx);
    ctl.completion_code = platforms::Ccode::Success;
    platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse);
}

void Control::on_gen_set_endpoint_id(Packet& tx, uint8_t eid, SetEndpoint sub_code) const
{
    auto& ctx        = Control::PktReq::from(tx);
    ctx.msg_type     = MsgType::Control;
    ctx.d            = 0;
    ctx.rq           = 1;
    ctx.rsvd0        = 0;
    ctx.instance_id  = 0;
    ctx.command_code = Cmd::SetEpId;  // set eid
    ctx.data[0]      = static_cast<uint8_t>(sub_code);
    ctx.data[1]      = eid;
}

void Control::on_gen_notify_discovery(Packet& tx) const
{
    auto& ctx        = Control::PktReq::from(tx);
    ctx.msg_type     = MsgType::Control;
    ctx.d            = 1;
    ctx.rq           = 1;
    ctx.rsvd0        = 0;
    ctx.instance_id  = 0;
    ctx.command_code = Cmd::DiscoveryNotify;  // set eid
}

void Control::on_discovery_notify(const Packet& rx, Packet& tx) const
{
    auto type = get_packet_type(rx);
    if (type == PacketType::Request) {
        // no request now
    }
    else if (type == PacketType::Response) {
        platforms::set_packet_length(tx, 0);
    }
}

void Control::fill_error_packet(platforms::Ccode code, const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctl           = Control::PktRes::from(tx);
    ctl.completion_code = code;
    platforms::set_packet_length(tx, hdrSize + HeaderSizeResponse);
}

void Control::fill_packet_header(const Packet& rx, Packet& tx) const
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
    auto& ctl        = Control::PktRes::from(tx);
    tx.hdr.tag_owner = (ctl.rq) ? 1 : 0;
    tx.hdr.msg_tag   = rx.hdr.msg_tag;

    tx.priv = {};
    platforms::set_packet_interface(tx, platforms::get_packet_interface(rx));

    update_eid(tx, platforms::get_packet_interface(rx));
}

void Control::fill_control_msg_header(const Packet& rx, Packet& tx) const
{
    auto& ctx        = Control::PktRes::from(tx);
    auto& crx        = Control::PktReq::from(rx);
    ctx.msg_type     = MsgType::Control;
    ctx.d            = 0;
    ctx.rq           = 0;
    ctx.rsvd0        = 0;
    ctx.instance_id  = crx.instance_id;
    ctx.command_code = crx.command_code;
}

bool Control::remove_routing_entry(uint8_t index)
{
    if (index > platforms::DownStreamNum) {
        return false;
    }

    if (_num_enumerate_eid != 0 && _routing_map.at(index).is_enumerated == true) {
        _num_enumerate_eid--;
    }

    _routing_map.at(index).is_need_enumerate = false;
    _routing_map.at(index).is_enumerated     = false;

    return true;
}

bool Control::add_routing_entry(uint8_t index)
{
    if (index > platforms::DownStreamNum) {
        return false;
    }

    _routing_map.at(index).is_need_enumerate = true;
    _num_enumerate_eid                       = 0;
    return true;
}

pdk::mctp::platforms::Interface Control::get_routing_client(uint8_t index)
{
    if (index > platforms::DownStreamNum) {
        return platforms::Interface::End;
    }

    return _routing_map.at(index).client;
}

uint8_t Control::get_routing_index(platforms::Interface client)
{
    uint8_t index = 0;

    if (!platforms::DownStreamInfos.empty()) {
        for (auto& info : platforms::DownStreamInfos) {
            if (info.client == client) {
                break;
            }
            index++;
        }
    }
    // return downstream index or the upstream index if client not found
    return index;
}

void Control::update_eid(Packet& tx, uint8_t client) const
{
    // we should return NULL_EID(0x00) if eid is not set yet
    // get_cur_eid will return NULL_EID(0x00) if eid is not set yet
    tx.hdr.src_eid = platforms::get_cur_eid(_router, client);
}

void Control::update_eid(platforms::NsmPacket& tx, uint8_t client) const
{
    // we should return NULL_EID(0x00) if eid is not set yet
    // get_cur_eid will return NULL_EID(0x00) if eid is not set yet
    tx.hdr.src_eid = platforms::get_cur_eid(_router, client);
}