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
#include "pdk-mctp-platforms-control.h"

#include <cstring>

#include "app/pdk-mctp-app-packet-plat.h"
#include "app/pdk-mctp-app-router-plat.h"

using namespace pdk::mctp::platforms;

bool Control::process(const app::Packet& rx, app::Packet& tx)
{
    auto& ctrl = app::Control::PktReq::from(rx);

    switch (ctrl.command_code) {
        case app::Cmd::SetEpId           : on_set_endpoint_id(rx, tx); break;
        case app::Cmd::GetEpId           : on_get_endpoint_id(rx, tx); break;
        case app::Cmd::GetEpUuid         : on_get_endpoint_uuid(rx, tx); break;
        case app::Cmd::GetMctpVerSupport : on_get_mctp_version_support(rx, tx); break;
        case app::Cmd::GetMsgTypeSupport : on_get_msg_type_support(rx, tx); break;
        case app::Cmd::GetVndrMsgSupport : on_get_vendor_msg_support(rx, tx); break;
        case app::Cmd::AllocateEpId      : on_allocate_endpoint_id(rx, tx); break;
        case app::Cmd::GetRoutTableEntry : on_get_routing_table_entry(rx, tx); break;
        case app::Cmd::PrepareEpDiscovery: on_prepare_endpoint_discovery(rx, tx); break;
        case app::Cmd::EpDiscovery       : on_endpoint_discovery(rx, tx); break;
        case app::Cmd::DiscoveryNotify   : on_discovery_notify(rx, tx); break;
        default                          : fill_error_packet(platforms::Ccode::ErrorUnsupportedCmd, rx, tx); return false;
    }

    return true;
}

void Control::on_set_endpoint_id(const app::Packet& rx, app::Packet& tx)
{
    auto& crx = Control::PktReq::from(rx);
    auto& ctx = Control::PktRes::from(tx);

    auto type = get_packet_type(rx);
    if (type == app::PacketType::Request) {
        switch (static_cast<app::SetEndpoint>(crx.data[0])) {
            case app::SetEndpoint::SetEidNormal:
            case app::SetEndpoint::SetEidForced:
                // @todo this will also take 0xff 0x00 illegal endpoint need to fix that
                platforms::set_cur_eid(
                    _router, platforms::get_packet_interface(rx), crx.data[1]);
                fill_packet_header(rx, tx);
                fill_control_msg_header(rx, tx);
                ctx.completion_code = Ccode::Success;
                ctx.data[0] = static_cast<uint8_t>(app::SetEndpoint::EidAcceptedAndEidPool);
                ctx.data[1] = platforms::get_cur_eid(_router,
                                                     platforms::get_packet_interface(rx));
                ctx.data[2] = EidPoolSize;
                break;

            case app::SetEndpoint::SetEidReset:
                // invalid data. Send the reply packet with error code.
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                break;

            case app::SetEndpoint::SetEidDiscovered:
                fill_packet_header(rx, tx);
                fill_control_msg_header(rx, tx);

                // fill the payload/message data
                ctx.completion_code = Ccode::Success;
                ctx.data[0] = static_cast<uint8_t>(app::SetEndpoint::EidAcceptedAndEidPool);
                ctx.data[1] = platforms::get_cur_eid(_router,
                                                     platforms::get_packet_interface(rx));
                ctx.data[2] = EidPoolSize;
                break;
            default: fill_error_packet(Ccode::ErrorInvalidData, rx, tx); break;
        }
        if (ctx.completion_code == Ccode::Success) {
            platforms::set_packet_length(tx,
                                         sizeof(app::TransportHeader) + HeaderSizeResponse + 3);
        }
    }
    else if (type == app::PacketType::Response) {
        if (EidPoolSize != 0) {
            for (auto& entry : _routing_map) {
                if (entry.assigned_eid == rx.hdr.src_eid) {
                    // [5:4] Eid assignment status
                    const uint8_t eid_assign_status = static_cast<uint8_t>(crx.data[1] >> 4U)
                                                    & 0b11U;
                    // Case of assign status is not accept or eid not match
                    if (eid_assign_status != static_cast<uint8_t>(app::EidAssignStatus::Accept)
                        || entry.assigned_eid != crx.data[2]) {
                        pdk::cmn::console::info(
                            "MCTP: Set Endpoint ID Reject, eid_assign_status: %d, "
                            "eid_pool_size: %d, assigned_eid: "
                            "%d",
                            eid_assign_status,
                            crx.data[2],
                            entry.assigned_eid);
                    }
                    else {
                        entry.is_enumerated = true;
                        entry.client        = static_cast<platforms::Interface>(
                            platforms::get_packet_interface(rx));

                        if (_num_enumerate_eid < EidPoolSize) {
                            _num_enumerate_eid++;
                        }
                    }
                }
            }
        }
        // response no need to response
        platforms::set_packet_length(tx, 0);
    }
}

void Control::on_get_routing_table_entry(const app::Packet& rx, app::Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctx       = Control::PktRes::from(tx);
    auto& crx       = Control::PktReq::from(rx);
    auto  cur_entry = crx.data[0];

    if (platforms::get_packet_interface(rx)
        >= static_cast<uint8_t>(platforms::Interface::UsEnd)) {
        ctx.completion_code = Ccode::ErrorInvalidData;
        platforms::set_packet_length(tx, sizeof(app::TransportHeader) + HeaderSizeResponse + 1);
    }
    else if (cur_entry > _num_enumerate_eid) {
        ctx.completion_code = Ccode::ErrorInvalidData;
        platforms::set_packet_length(tx, sizeof(app::TransportHeader) + HeaderSizeResponse + 1);
    }
    else {
        ctx.completion_code = Ccode::Success;

        // TODO : replace index magic number
        ctx.data[1] = RoutingEntryNum;
        ctx.data[2] = EidSize;
        ctx.data[7] = PhyAddrSize;

        // report MCU routing info
        if (cur_entry == _num_enumerate_eid) {
            ctx.data[0] = NoNextEntry;
            ctx.data[3] = platforms::get_cur_eid(_router, platforms::get_packet_interface(rx));
            ctx.data[4] = PortNum;
            ctx.data[5] = static_cast<uint8_t>(app::PhyId::MctpOverUsb);
            ctx.data[6] = static_cast<uint8_t>(app::PhyMediumId::Usb20);
            ctx.data[8] = 0x00;  // physical address
        }
        else {
            // WAR, should enhance how to search
            // search cur_entry in routing map
            uint32_t entry_in_map     = 0;
            uint32_t index_enumerated = 0;
            for (auto& entry : _routing_map) {
                if (entry.is_enumerated == true) {
                    if (index_enumerated == cur_entry) {
                        break;
                    }
                    index_enumerated++;
                }
                entry_in_map++;
            }
            // report cur_entry enumerated endpoint routing info
            ctx.data[3] = _routing_map.at(entry_in_map).assigned_eid;

            if (_routing_map.at(entry_in_map).client < platforms::Interface::End) {
                ctx.data[4] = static_cast<uint8_t>(_routing_map.at(entry_in_map).client);
            }
            else {
                ctx.data[4] = 0;
            }
            const auto index = static_cast<uint8_t>(
                                   (_routing_map.at(entry_in_map).assigned_eid - _start_eid))
                             & OneByteMask;
            auto& ds_info = DownStreamInfos.at(index);
            ctx.data[5]   = static_cast<uint8_t>(ds_info.phy_id);
            ctx.data[6]   = static_cast<uint8_t>(ds_info.phy_medium_id);
            ctx.data[8]   = static_cast<uint8_t>(ds_info.port_address);

            ctx.data[0] = ++cur_entry;
        }

        platforms::set_packet_length(
            tx, sizeof(app::TransportHeader) + HeaderSizeResponse + 8 + PhyAddrSize);
    }
}