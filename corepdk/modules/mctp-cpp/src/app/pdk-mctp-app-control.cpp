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
    if (crx.data[0] == 0) {
        ctx.data[0] = 0x01;                // Next Vendor ID Set Selector
        ctx.data[1] = VendorIdFormatIana;  // IANA Enterprise Number
        ctx.data[2] = VendorIanaIdByte0;   // NVIDIA = 0x00001647, MSB first
        ctx.data[3] = VendorIanaIdByte1;
        ctx.data[4] = VendorIanaIdByte2;
        ctx.data[5] = VendorIanaIdByte3;
        ctx.data[6] = 0x00;  // no command set versioning
        ctx.data[7] = 0x00;
        platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 8);
    }
    else if (crx.data[0] == 1) {
        ctx.data[0] = NoMoreCapability;   // Next Vendor ID Set Selector
        ctx.data[1] = VendorIdFormatPci;  // PCI Vendor ID
        ctx.data[2] = VendorPciIdByte0;   // NVIDIA = 0x10DE, MSB first
        ctx.data[3] = VendorPciIdByte1;
        ctx.data[4] = 0x00;  // no command set versioning
        ctx.data[5] = 0x00;
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
    auto& ctx                      = Control::PktRes::from(tx);
    auto& crx                      = Control::PktReq::from(rx);
    ctx.completion_code            = platforms::Ccode::Success;
    const uint8_t packet_interface = platforms::get_packet_interface(rx);

    if (packet_interface >= static_cast<uint8_t>(platforms::Interface::End)) {
        ctx.completion_code = platforms::Ccode::ErrorInvalidData;
        platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 3);
        return;
    }

    // DSP0236: the first interface to allocate owns the EID pool and every other
    // bus owner is answered Reject. Products that bridge a second upstream domain
    // are enumerated by two bus owners at once, so they skip the latch.
    bool accept = true;
    if constexpr (!platforms::allows_concurrent_bus_owners()) {
        accept = (_allocate_eid_bus == static_cast<uint8_t>(platforms::Interface::End))
              || (_allocate_eid_bus == packet_interface);
    }

    if (accept) {
        // Only a product with two concurrent bus owners needs to remember more than
        // the latch below: where one owner is accepted the mask can never hold a
        // second bit, so it carries no information the latch does not already have.
        if constexpr (platforms::allows_concurrent_bus_owners()) {
            if (packet_interface <= MaxEidBusMaskBit) {
                _allocate_eid_bus_mask |= (uint32_t{1} << packet_interface);
            }
        }
        _allocate_eid_bus = packet_interface;

        // clear the additional eid count and infos by RoutingInfoUpdate if force allocate
        if (crx.data[0] == static_cast<uint8_t>(AllocateEndpoint::ForceAllocate)) {
            for (uint8_t index = platforms::DefaultRoutingTableSize;
                 index < platforms::DefaultRoutingTableSize + _additional_eid_count;
                 index++) {
                _routing_map.at(index).is_need_enumerate = false;
                _routing_map.at(index).is_enumerated     = false;
                _routing_map.at(index).is_set_eid_sent   = false;
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
        // _us_i2c_eid is deliberately not latched here. Addressing the SMBus bus
        // owner by its real EID is only correct on a transparent I2C bridge, which
        // is a per-product property, so the platform sets it from its own
        // configuration. Latching it unconditionally would make every other
        // product send DiscoveryNotify to the owner's real EID instead of the
        // NULL EID that DSP0236 pre-discovery expects.
        if (packet_interface == static_cast<uint8_t>(platforms::Interface::UsUsb)) {
            _us_usb_eid = rx.hdr.src_eid;
        }
        else if (platforms::is_us_i3c_interface(packet_interface)) {
            _us_i3c_eid = rx.hdr.src_eid;
        }

        // TODO: should change to index 0-3
        if (platforms::should_allocate_downstream_pool(packet_interface)) {
            _ds_pool_start_eid    = crx.data[2];
            _is_ds_pool_allocated = true;
            for (uint32_t index = 0; index < EidPoolSize; index++) {
                _routing_map.at(index).assigned_eid = platforms::get_downstream_pool_eid(
                    _ds_pool_start_eid, static_cast<uint8_t>(index));
            }
        }

        // store update stream information
        const uint8_t upstream_index = platforms::get_upstream_routing_index(packet_interface);
        if (upstream_index >= _routing_map.size()) {
            ctx.completion_code = platforms::Ccode::ErrorInvalidData;
            platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 3);
            return;
        }
        _routing_map.at(upstream_index).is_enumerated = true;
        _routing_map.at(upstream_index).assigned_eid  = rx.hdr.src_eid;
        _routing_map.at(upstream_index).client        = static_cast<platforms::Interface>(
            packet_interface);

        // use platform specific driver to enumerate
        platforms::on_enumerate_plat();

        ctx.data[0] = static_cast<uint8_t>(AllocateEndpointStatus::Accept);
    }
    else {
        ctx.data[0] = static_cast<uint8_t>(AllocateEndpointStatus::Reject);
    }

    const uint8_t reported_pool_size = platforms::get_reported_eid_pool_size(packet_interface);
    ctx.data[1]                      = reported_pool_size;
    ctx.data[2]                      = (reported_pool_size == 0) ? 0 : _start_eid;
    platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse + 3);
}

void Control::on_routing_info_update(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctx = Control::PktRes::from(tx);
    platforms::set_packet_length(tx, sizeof(TransportHeader) + HeaderSizeResponse);

    // if RoutingInfoUpdateSize is 0, not support update routing info
    if (platforms::RoutingInfoUpdateSize == 0) {
        ctx.completion_code = platforms::Ccode::ErrorUnsupportedCmd;
        return;
    }

    // only the bus owner can update routing info
    const uint8_t rx_interface = platforms::get_packet_interface(rx);
    bool          is_bus_owner = _allocate_eid_bus == rx_interface;
    if constexpr (platforms::allows_concurrent_bus_owners()) {
        // The latch holds only the most recent of the two owners, so an update from
        // the other one is still legitimate and has to be looked up in the mask.
        is_bus_owner = is_bus_owner || has_allocate_eid_bus(rx_interface);
    }
    if (!is_bus_owner) {
        ctx.completion_code = platforms::Ccode::ErrorInvalidData;
        return;
    }

    if constexpr (platforms::tolerant_routing_info_update()) {
        apply_routing_info_update_tolerant(rx, tx);
    }
    else {
        apply_routing_info_update_strict(rx, tx);
    }
}

// DSP0236 contract, for products enumerated by a single bus owner: one storable
// SingleEndpoint entry per request, and the owner is told what it got wrong so it
// can correct the request instead of repeating it.
void Control::apply_routing_info_update_strict(const Packet& rx, Packet& tx)
{
    auto& crx = Control::PktReq::from(rx);
    auto& ctx = Control::PktRes::from(tx);

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
    // check if the update eid is any current eid of this endpoint
    const uint8_t update_eid = crx.data[3];
    for (const auto current_eid : _router.ec.cur_eid) {
        if (update_eid == current_eid) {
            ctx.completion_code = platforms::Ccode::ErrorInvalidData;
            return;
        }
    }
    // check if the update eid already exists in the routing table
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
    const auto index = static_cast<uint8_t>(platforms::DefaultRoutingTableSize
                                            + _additional_eid_count);
    _routing_map.at(index).assigned_eid = update_eid;
    _routing_map.at(index).client       = static_cast<platforms::Interface>(
        platforms::get_packet_interface(rx));
    _routing_map.at(index).is_enumerated = true;

    // Store the additional address only for transports that need it in
    // GetRoutingTableEntries.
    if (platforms::should_store_additional_address(platforms::get_packet_interface(rx))) {
        _additional_address.at(_additional_eid_count) = crx.data[4];
    }

    _additional_eid_count++;

    // use platform specific driver to update routing info
    platforms::on_routing_info_update_plat(
        rx.hdr.src_eid, platforms::get_packet_interface(rx), update_eid);

    ctx.completion_code = platforms::Ccode::Success;
}

void Control::apply_routing_info_update_tolerant(const Packet& rx, Packet& tx)
{
    auto& crx = Control::PktReq::from(rx);
    auto& ctx = Control::PktRes::from(tx);

    // A bus owner may push its entire routing table in a single Routing
    // Information Update, i.e. multiple entries in one request, including entry
    // types we do not store (bridge / EID range) and EIDs we already know (our
    // own EID, already-enumerated downstreams). Answering the strict policy's
    // ErrorInvalidData / ErrorInsufficientSpace to such an owner makes it retry
    // the same request forever.
    //
    // Walk every entry, store the ones we can actually use (new SingleEndpoint
    // reachable via this interface) and silently skip the rest, then always
    // acknowledge with Success so the owner can complete enumeration. Routing to
    // downstream endpoints does not depend on this additional table being fully
    // populated; it is handled by the normal forward path plus the entries
    // created during enumeration.
    const uint8_t  update_count = crx.data[0];
    const uint8_t* req          = &crx.data[0];

    // Each entry is 4 bytes: [entry_type, eid_range, starting_eid, address].
    // Clamp the iteration to what the message buffer can hold so a bogus count
    // can never make us read past the packet.
    constexpr uint8_t BytesPerEntry = 4;
    constexpr auto    MaxEntries    = static_cast<uint8_t>((platforms::TransmitUnit - 1)
                                                     / BytesPerEntry);
    const uint8_t     entry_count   = (update_count < MaxEntries) ? update_count : MaxEntries;

    bool    any_added      = false;
    uint8_t last_added_eid = 0;
    for (uint8_t i = 0; i < entry_count; i++) {
        const auto    base       = static_cast<uint8_t>(1 + (i * BytesPerEntry));
        const uint8_t entry_type = req[base];
        const uint8_t eid_range  = req[base + 1];
        const uint8_t update_eid = req[base + 2];

        // Only store single, non-bridge endpoints with a 1-EID range. Other
        // entry types (bridge / EID range) are accepted but not stored.
        if ((entry_type & 0b11U) != static_cast<uint8_t>(EntryType::SingleEndpoint)
            || eid_range != 0x01) {
            continue;
        }

        // Never treat one of our own current EIDs as a new downstream entry.
        // Skip it (do not reject the whole request) so the bus owner can still
        // push its full table; preserves the Bug-6222894 guard under the
        // tolerant multi-entry policy.
        bool is_current_eid = false;
        for (const auto current_eid : _router.ec.cur_eid) {
            if (update_eid == current_eid) {
                is_current_eid = true;
                break;
            }
        }
        if (is_current_eid) {
            continue;
        }

        // Skip EIDs we already know (our own EID or an already-enumerated
        // endpoint); re-adding them is unnecessary and must not be an error.
        bool already_known = false;
        for (auto& info : _routing_map) {
            if (info.assigned_eid == update_eid) {
                already_known = true;
                break;
            }
        }
        if (already_known) {
            continue;
        }

        // Stop storing once the additional routing slots are full, but keep
        // acknowledging the command with Success.
        if (_additional_eid_count >= platforms::RoutingInfoUpdateSize) {
            continue;
        }

        const auto index = static_cast<uint8_t>(platforms::DefaultRoutingTableSize
                                                + _additional_eid_count);
        _routing_map.at(index).assigned_eid = update_eid;
        _routing_map.at(index).client       = static_cast<platforms::Interface>(
            platforms::get_packet_interface(rx));
        _routing_map.at(index).is_enumerated = true;

        // Store the additional address only for transports that need it in
        // GetRoutingTableEntries.
        if (platforms::should_store_additional_address(platforms::get_packet_interface(rx))) {
            _additional_address.at(_additional_eid_count) = req[base + 3];
        }

        _additional_eid_count++;
        any_added      = true;
        last_added_eid = update_eid;
    }

    // use platform specific driver to update routing info if anything changed
    if (any_added) {
        platforms::on_routing_info_update_plat(
            rx.hdr.src_eid, platforms::get_packet_interface(rx), last_added_eid);
    }

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
    _routing_map.at(index).is_set_eid_sent   = false;

    return true;
}

bool Control::add_routing_entry(uint8_t index)
{
    if (index > platforms::DownStreamNum) {
        return false;
    }

    _routing_map.at(index).is_need_enumerate = true;
    _routing_map.at(index).is_set_eid_sent   = false;
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
