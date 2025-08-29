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
#include "corepdk/platforms/mcxn236/mctp-cpp/src/pdk-mctp-platforms-control.h"

#include <cstring>

#include "nv/mctp/driver.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/interface.h"
#include "nv/nv.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"

using namespace nv;
using namespace nv::mctp;
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
        case app::Cmd::RoutingInfoUpdate : on_routing_info_update(rx, tx); break;
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
    if (type == PacketType::Request) {
        switch (static_cast<SetEndpoint>(crx.data[0])) {
            case SetEndpoint::SetEidNormal:
            case SetEndpoint::SetEidForced:
                // @todo this will also take 0xff 0x00 illegal endpoint need to fix that
                _router.ec.cur_eid.at(rx.priv.packet_interface) = crx.data[1];
                fill_packet_header(rx, tx);
                fill_control_msg_header(rx, tx);

                if constexpr (nv::ipc::I2cIsEndpoint) {
                    if (rx.priv.packet_interface
                        == static_cast<uint16_t>(mctp::Client::UsI2c)) {
                        // GFWLYNT1-792: upstream I2C WAR for FPGA bringup
                        ctx.completion_code = Ccode::Success;
                        ctx.data[0]         = static_cast<uint8_t>(
                            SetEndpoint::EidAcceptedAndNoEidPool);
                        ctx.data[1] = _router.ec.cur_eid.at(rx.priv.packet_interface);
                        ctx.data[2] = 0;
                    }
                    else {
                        ctx.completion_code = Ccode::Success;
                        ctx.data[0] = static_cast<uint8_t>(SetEndpoint::EidAcceptedAndEidPool);
                        ctx.data[1] = _router.ec.cur_eid.at(rx.priv.packet_interface);
                        ctx.data[2] = EidPoolSize;
                    }
                }
                else {
                    ctx.completion_code = Ccode::Success;
                    ctx.data[0] = static_cast<uint8_t>(SetEndpoint::EidAcceptedAndEidPool);
                    ctx.data[1] = _router.ec.cur_eid.at(rx.priv.packet_interface);
                    ctx.data[2] = EidPoolSize;
                }

                if constexpr (ipc::I2cTransparent) {
                    if (rx.priv.packet_interface
                        == static_cast<uint16_t>(mctp::Client::UsI2c)) {
                        // Transparent bridge
                        static_assert(ipc::RoutingTableSize > (ipc::DownStreamNum + 1)
                                          || (!ipc::I2cTransparent),
                                      "Invalid RoutingTableSize");

                        _us_i2c_eid                                    = rx.hdr.src_eid;
                        _routing_map.at(EidPoolSize + 1).is_enumerated = true;
                        _routing_map.at(EidPoolSize + 1).assigned_eid  = rx.hdr.src_eid;
                        // Validate packet_interface value before casting to prevent undefined
                        // behavior
                        const auto interface_value = static_cast<uint32_t>(
                            rx.priv.packet_interface);
                        if (interface_value >= static_cast<uint32_t>(mctp::Client::End)) {
                            // Invalid interface value - log error and discard response
                            logger::error(nv::logger::Event::MctpInvalidInterface,
                                          {static_cast<uint8_t>(interface_value), 0, 0});
                            tx.priv.packet_length = 0;
                            return;
                        }
                        // Safe to cast since we've validated the value
                        _routing_map.at(EidPoolSize + 1).client = static_cast<mctp::Client>(
                            rx.priv.packet_interface);
                        Driver::mctp_send_cmd(Driver::CmdCode::EnumerateDone);
                    }
                }

                break;

            case SetEndpoint::SetEidReset:
                // invalid data. Send the reply packet with error code.
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                break;

            case SetEndpoint::SetEidDiscovered:
                fill_packet_header(rx, tx);
                fill_control_msg_header(rx, tx);

                // fill the payload/message data
                ctx.completion_code = Ccode::Success;
                ctx.data[0]         = static_cast<uint8_t>(SetEndpoint::EidAcceptedAndEidPool);
                ctx.data[1]         = _router.ec.cur_eid.at(rx.priv.packet_interface);
                ctx.data[2]         = EidPoolSize;
                break;
            default: fill_error_packet(Ccode::ErrorInvalidData, rx, tx); break;
        }
        if (ctx.completion_code == Ccode::Success) {
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 3;
        }
    }
    else if (type == PacketType::Response) {
        if (EidPoolSize != 0) {
            for (auto& entry : _routing_map) {
                if (entry.assigned_eid == rx.hdr.src_eid) {
                    // [5:4] Eid assignment status
                    const uint8_t eid_assign_status = static_cast<uint8_t>(crx.data[1] >> 4U)
                                                    & 0b11U;
                    // Case of assign status is not accept or eid not match
                    logger::info(
                        nv::logger::Event::MctpRecvSetEid,
                        {entry.assigned_eid, rx.priv.packet_interface, eid_assign_status});

                    if (entry.is_enumerated == false) {
                        // Validate packet_interface value before casting to prevent undefined
                        // behavior
                        const auto interface_value = static_cast<uint32_t>(
                            rx.priv.packet_interface);
                        if (interface_value >= static_cast<uint32_t>(mctp::Client::End)) {
                            // Invalid interface value - log error and skip this entry but
                            // continue processing This prevents undefined behavior while
                            // allowing other valid entries to be processed
                            logger::error(nv::logger::Event::MctpInvalidInterface,
                                          {static_cast<uint8_t>(interface_value), 0, 0});
                            continue;
                        }
                        // Only mark as enumerated if interface value is valid
                        entry.is_enumerated = true;
                        // Safe to cast since we've validated the value
                        entry.client = static_cast<mctp::Client>(rx.priv.packet_interface);

                        if (_num_enumerate_eid < EidPoolSize) {
                            _num_enumerate_eid++;
                        }
                    }
                    else {
                        // do nothing since already enumerate
                    }
                }
            }
        }
        // response no need to response
        tx.priv.packet_length = 0;
    }
}

void Control::on_get_routing_table_entry(const app::Packet& rx, app::Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctx       = Control::PktRes::from(tx);
    auto& crx       = Control::PktReq::from(rx);
    auto  cur_entry = crx.data[0];

    if (rx.priv.packet_interface >= static_cast<uint8_t>(mctp::Client::UsEnd)) {
        ctx.completion_code   = Ccode::ErrorInvalidData;
        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
    }
    else if (cur_entry > _num_enumerate_eid + _additional_eid_count) {
        ctx.completion_code   = Ccode::ErrorInvalidData;
        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
    }
    else {
        ctx.completion_code = Ccode::Success;

        // TODO : replace index magic number
        ctx.data[1] = RoutingEntryNum;
        ctx.data[2] = EidSize;
        ctx.data[7] = PhyAddrSize;

        // report MCU routing info
        if (cur_entry == _num_enumerate_eid + _additional_eid_count) {
            ctx.data[0] = NoNextEntry;
            ctx.data[3] = _router.ec.cur_eid.at(rx.priv.packet_interface);
            ctx.data[4] = PortNum;
            ctx.data[5] = static_cast<uint8_t>(PhyId::MctpOverUsb);
            ctx.data[6] = static_cast<uint8_t>(PhyMediumId::Usb20);
            ctx.data[8] = 0x00;  // physical address
        }
        // report additional routing entry, skip if no additional one
        else if (_additional_eid_count > 0 && cur_entry >= _num_enumerate_eid) {
            const uint8_t index = DefaultRoutingTableSize
                                + static_cast<uint8_t>(cur_entry - _num_enumerate_eid);
            ctx.data[0] = ++cur_entry;
            ctx.data[3] = _routing_map.at(index).assigned_eid;
            if (_routing_map.at(index).client >= mctp::Client::UsEnd) {
                ctx.data[4] = InvalidPortNum;
            }
            else {
                ctx.data[4] = static_cast<uint8_t>(_routing_map.at(index).client);
            }
            if (_routing_map.at(index).client == mctp::Client::UsI2c) {
                ctx.data[5] = static_cast<uint8_t>(PhyId::MctpOverSmbus);
                ctx.data[6] = static_cast<uint8_t>(PhyMediumId::Smbus30I2cFast);
                ctx.data[8] = get_additional_address(cur_entry - _num_enumerate_eid);
            }
            else {
                ctx.data[5] = static_cast<uint8_t>(PhyId::MctpOverUsb);
                ctx.data[6] = static_cast<uint8_t>(PhyMediumId::Usb20);
                ctx.data[8] = 0x00;
            }
        }
        else {
            // WAR, should enhance how to search
            // search cur_entry in routing map
            uint32_t entry_in_map     = 0;
            uint32_t index_enumerated = 0;
            bool     found_entry      = false;

            for (auto& entry : _routing_map) {
                if (entry.is_enumerated == true) {
                    if (index_enumerated == cur_entry) {
                        found_entry = true;
                        break;
                    }
                    index_enumerated++;
                }

                // entry_in_map = UINT32_MAX_VALUE isn't a possible case actually
                // We add this check only to avoid Coverity issue
                if (entry_in_map < UINT32_MAX_VALUE) {
                    entry_in_map++;
                }
            }

            // Verify we found the entry
            if (!found_entry) {
                // Entry not found - log error and set response data similar to no next entry
                logger::error(nv::logger::Event::MctpRoutingEntryNotFound,
                              {static_cast<uint8_t>(found_entry),
                               static_cast<uint8_t>(entry_in_map),
                               static_cast<uint8_t>(_routing_map.size())});
                ctx.data[0]           = NoNextEntry;
                ctx.data[3]           = _router.ec.cur_eid.at(rx.priv.packet_interface);
                ctx.data[4]           = PortNum;
                ctx.data[5]           = static_cast<uint8_t>(PhyId::MctpOverUsb);
                ctx.data[6]           = static_cast<uint8_t>(PhyMediumId::Usb20);
                ctx.data[8]           = 0x00;  // physical address
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8 + PhyAddrSize;
                return;
            }

            // report cur_entry enumerated endpoint routing info
            ctx.data[3] = _routing_map.at(entry_in_map).assigned_eid;
            if (_routing_map.at(entry_in_map).client < mctp::Client::End) {
                ctx.data[4] = static_cast<uint8_t>(
                    static_cast<uint32_t>(_routing_map.at(entry_in_map).client) & OneByteMask);
            }
            else {
                ctx.data[4] = InvalidPortNum;
            }

            // Check for potential negative or overflow in the calculation
            const int32_t eid_diff = static_cast<int32_t>(
                                         _routing_map.at(entry_in_map).assigned_eid)
                                   - static_cast<int32_t>(_start_eid);
            if (eid_diff < 0 || eid_diff > OneByteMask) {
                // Invalid EID difference - log error and set response data similar to no next
                // entry
                logger::error(nv::logger::Event::MctpInvalidEidDifference,
                              {static_cast<uint8_t>(eid_diff & OneByteMask),
                               _routing_map.at(entry_in_map).assigned_eid,
                               _start_eid});
                ctx.data[0]           = NoNextEntry;
                ctx.data[3]           = _router.ec.cur_eid.at(rx.priv.packet_interface);
                ctx.data[4]           = PortNum;
                ctx.data[5]           = static_cast<uint8_t>(PhyId::MctpOverUsb);
                ctx.data[6]           = static_cast<uint8_t>(PhyMediumId::Usb20);
                ctx.data[8]           = 0x00;  // physical address
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8 + PhyAddrSize;
                return;
            }
            const auto index   = static_cast<uint8_t>(eid_diff) & OneByteMask;
            auto&      ds_info = ipc::DownStreamInfos.at(index);
            ctx.data[5]        = static_cast<uint8_t>(ds_info.phy_id);
            ctx.data[6]        = static_cast<uint8_t>(ds_info.phy_medium_id);
            ctx.data[8]        = static_cast<uint8_t>(ds_info.port_address);

            ctx.data[0] = ++cur_entry;
        }

        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8 + PhyAddrSize;
    }
}
