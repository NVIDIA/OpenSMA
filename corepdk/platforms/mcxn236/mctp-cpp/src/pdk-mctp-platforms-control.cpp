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

#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)
    // USB FS is an endpoint only; it must not own or update the downstream pool.
    if (rx.priv.packet_interface == static_cast<uint16_t>(mctp::Client::UsUsbFs)
        && get_packet_type(rx) == PacketType::Request
        && (ctrl.command_code == app::Cmd::AllocateEpId
            || ctrl.command_code == app::Cmd::RoutingInfoUpdate)) {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return false;
    }
#endif

    switch (ctrl.command_code) {
        case app::Cmd::SetEpId          : on_set_endpoint_id(rx, tx); break;
        case app::Cmd::GetEpId          : on_get_endpoint_id(rx, tx); break;
        case app::Cmd::GetEpUuid        : on_get_endpoint_uuid(rx, tx); break;
        case app::Cmd::GetMctpVerSupport: on_get_mctp_version_support(rx, tx); break;
        case app::Cmd::GetMsgTypeSupport: on_get_msg_type_support(rx, tx); break;
        case app::Cmd::GetVndrMsgSupport: on_get_vendor_msg_support(rx, tx); break;
        case app::Cmd::AllocateEpId:
            on_allocate_endpoint_id(rx, tx);
            if (get_packet_type(rx) == PacketType::Request
                && app::Control::PktRes::from(tx).completion_code == Ccode::Success
                && app::Control::PktRes::from(tx).data[0]
                       == static_cast<uint8_t>(app::AllocateEndpointStatus::Accept)
                && rx.priv.packet_interface < static_cast<uint16_t>(mctp::Client::UsEnd)) {
                record_upstream_source(static_cast<mctp::Client>(rx.priv.packet_interface),
                                       rx.hdr.src_eid);
            }
            break;
        case app::Cmd::RoutingInfoUpdate : on_routing_info_update(rx, tx); break;
        case app::Cmd::GetRoutTableEntry : on_get_routing_table_entry(rx, tx); break;
        case app::Cmd::PrepareEpDiscovery: on_prepare_endpoint_discovery(rx, tx); break;
        case app::Cmd::EpDiscovery       : on_endpoint_discovery(rx, tx); break;
        case app::Cmd::DiscoveryNotify   : on_discovery_notify(rx, tx); break;
        default                          : fill_error_packet(platforms::Ccode::ErrorUnsupportedCmd, rx, tx); return false;
    }

    return true;
}

uint8_t Control::get_upstream_src_eid(mctp::Client client) const
{
    auto index = static_cast<std::size_t>(client);
    return index < _upstream_src_eids.size() ? _upstream_src_eids.at(index) : 0U;
}

void Control::record_upstream_source(mctp::Client client, uint8_t source_eid)
{
    auto client_index = static_cast<std::size_t>(client);
    if (client_index >= _upstream_src_eids.size()) {
        return;
    }
    _upstream_src_eids.at(client_index) = source_eid;
}

void Control::on_get_endpoint_id(const app::Packet& rx, app::Packet& tx) const
{
    app::Control::on_get_endpoint_id(rx, tx);

#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)
    if (rx.priv.packet_interface == static_cast<uint16_t>(mctp::Client::UsUsbFs)) {
        auto& ctx   = Control::PktRes::from(tx);
        ctx.data[1] = static_cast<uint8_t>(app::EndpointIdType::DynamicEid);
    }
#endif
}

void Control::on_set_endpoint_id(const app::Packet& rx, app::Packet& tx)
{
    auto& crx = Control::PktReq::from(rx);
    auto& ctx = Control::PktRes::from(tx);

    auto type = get_packet_type(rx);
    if (type == PacketType::Request) {
        // UsI3c now sits before UsEnd (alongside UsUsbFs), so the UsEnd bound
        // again admits every upstream bus owner and rejects downstream.
        if (rx.priv.packet_interface >= static_cast<uint16_t>(mctp::Client::UsEnd)) {
            logger::error(nv::logger::Event::MctpInvalidInterfaceIndex,
                          {static_cast<uint8_t>(rx.priv.packet_interface)});
            fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
            return;
        }
        auto eid_pool_size = EidPoolSize;
        if constexpr (nv::ipc::I2cIsEndpoint) {
            if (rx.priv.packet_interface == common::to_underlying(mctp::Client::UsI2c)) {
                eid_pool_size = 0;
            }
        }
        if constexpr (!UsI3cBridgeMode) {
            if (rx.priv.packet_interface == common::to_underlying(mctp::Client::UsI3c)) {
                eid_pool_size = 0;
            }
        }
#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)
        if (rx.priv.packet_interface == common::to_underlying(mctp::Client::UsUsbFs)) {
            eid_pool_size = 0;
        }
#endif

        auto eid_allocation_status = eid_pool_size > 0 ? SetEndpoint::EidAcceptedAndEidPool
                                                       : SetEndpoint::EidAcceptedAndNoEidPool;

        switch (static_cast<SetEndpoint>(crx.data[0])) {
            case SetEndpoint::SetEidNormal:
            case SetEndpoint::SetEidForced: {
                // @todo this will also take 0xff 0x00 illegal endpoint need to fix that
                _router.ec.cur_eid.at(rx.priv.packet_interface) = crx.data[1];
                fill_packet_header(rx, tx);
                fill_control_msg_header(rx, tx);

                ctx.completion_code = Ccode::Success;
                ctx.data[0]         = static_cast<uint8_t>(eid_allocation_status);
                ctx.data[1]         = _router.ec.cur_eid.at(rx.priv.packet_interface);
                ctx.data[2]         = eid_pool_size;

                auto upstream = static_cast<mctp::Client>(rx.priv.packet_interface);
                if (upstream == mctp::Client::UsUsb
#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)
                    || upstream == mctp::Client::UsUsbFs
#endif
                ) {
                    record_upstream_source(upstream, rx.hdr.src_eid);
                }

                // Only the CPU-bridge products have a UsI3c interface, so this
                // arm is unreachable elsewhere. Gating it also keeps the
                // UsI3cSlotIndex accesses out of products whose routing table
                // is too small to hold that slot.
                if constexpr (UsI3cBridgeMode) {
                    if (rx.priv.packet_interface
                        == static_cast<uint16_t>(mctp::Client::UsI3c)) {
                        constexpr uint8_t UsI3cSlotIndex = EidPoolSize + 2;
                        // A discarded if constexpr arm is still fully analysed
                        // outside a template, so the assert has to hold for
                        // non-bridge products as well.
                        static_assert(!UsI3cBridgeMode
                                          || ipc::RoutingTableSize > UsI3cSlotIndex,
                                      "RoutingTableSize too small for UsI3c upstream slot");

                        _us_i3c_eid                                   = rx.hdr.src_eid;
                        _routing_map.at(UsI3cSlotIndex).is_enumerated = true;
                        _routing_map.at(UsI3cSlotIndex).assigned_eid  = rx.hdr.src_eid;
                        _routing_map.at(UsI3cSlotIndex).client        = mctp::Client::UsI3c;
                        if constexpr (EnableUsI3cDebugLog) {
                            // Snapshot the bridge EIDs so the CPU-facing MCU EID
                            // (cur_eid[UsI3c]) can be compared against the dst the
                            // CPU actually uses, and the BMC alias base (start_eid).
                            logger::info(nv::logger::Event::MctpUsI3cEidsV2,
                                         {_router.ec.cur_eid.at(rx.priv.packet_interface),
                                          get_start_eid(),
                                          _us_i3c_eid,
                                          get_us_usb_eid()});
                        }
                        Driver::mctp_send_cmd(Driver::CmdCode::EnumerateDone);
                        break;
                    }
                }

                if constexpr (UsI3cBridgeMode) {
                    if (rx.priv.packet_interface
                        == static_cast<uint16_t>(mctp::Client::UsUsb)) {
                        ctx.data[2] = static_cast<uint8_t>(DownStreamStartOffset + EidPoolSize
                                                           + BridgeCpuAliasNum);
                    }
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
            }

            case SetEndpoint::SetEidReset:
                // invalid data. Send the reply packet with error code.
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                break;

            case SetEndpoint::SetEidDiscovered:
                fill_packet_header(rx, tx);
                fill_control_msg_header(rx, tx);

                // fill the payload/message data
                ctx.completion_code = Ccode::Success;
                ctx.data[0]         = static_cast<uint8_t>(eid_allocation_status);
                ctx.data[1]         = _router.ec.cur_eid.at(rx.priv.packet_interface);
                ctx.data[2]         = eid_pool_size;
                break;
            default: fill_error_packet(Ccode::ErrorInvalidData, rx, tx); break;
        }
        if (ctx.completion_code == Ccode::Success) {
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 3;
        }
    }
    else if (type == PacketType::Response) {
        if (EidPoolSize != 0) {
            // Correlate by the "EID Setting" field (crx.data[2]), which the spec
            // requires to echo the assigned EID on an accepted Set EID. The MCTP
            // source EID is unreliable here -- it carries the endpoint's previous
            // EID (NULL_EID on first enumeration). Skip NULL_EID to avoid
            // false-matching an unassigned slot.
            const uint8_t eid_setting = crx.data[2];
            for (auto& entry : _routing_map) {
                if ((eid_setting != app::NULL_EID) && (entry.assigned_eid == eid_setting)) {
                    // [5:4] EID assignment status
                    const uint8_t eid_assign_status = static_cast<uint8_t>(crx.data[1] >> 4U)
                                                    & 0b11U;
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

void Control::on_allocate_endpoint_id(const app::Packet& rx, app::Packet& tx)
{
    // Only a bus owner on an upstream link may allocate an EID pool. The base
    // implementation only bounds the interface index, which also admits every
    // downstream interface.
    if (rx.priv.packet_interface >= static_cast<uint16_t>(mctp::Client::UsEnd)) {
        logger::error(nv::logger::Event::MctpInvalidInterfaceIndex,
                      {static_cast<uint8_t>(rx.priv.packet_interface)});
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    app::Control::on_allocate_endpoint_id(rx, tx);
}

void Control::on_get_vendor_msg_support(const app::Packet& rx, app::Packet& tx) const
{
    // Products without the I3C bridge have no UsI3c interface, so the check below
    // can never take the bridge arm; gating it keeps that body out of their image.
    if constexpr (UsI3cBridgeMode) {
        const auto packet_interface = static_cast<mctp::Client>(rx.priv.packet_interface);
        if (packet_interface == mctp::Client::UsI3c) {
            // The CPU is the MCTP bus owner on the I3C link and queries our vendor
            // message support. Report exactly one set (NVIDIA IANA PEN 0x00001647) and
            // mark it as the last set via the next-set selector = NoMoreCapability, so
            // the CPU accepts the response and stops retrying. The common handler
            // returns a next-set selector equal to the requested selector (0 -> 0),
            // which the CPU rejects as non-advancing and retries every ~25 ms.
            fill_packet_header(rx, tx);
            fill_control_msg_header(rx, tx);
            auto& ctx             = Control::PktRes::from(tx);
            ctx.completion_code   = Ccode::Success;
            ctx.data[0]           = NoMoreCapability;  // no more sets after this one
            ctx.data[1]           = 0x01;  // Vendor ID Format: IANA Enterprise Number
            ctx.data[2]           = 0x00;
            ctx.data[3]           = 0x00;
            ctx.data[4]           = VendorIanaIdByte2;  // 0x16 (bits 15:8 of 0x1647)
            ctx.data[5]           = VendorIanaIdByte3;  // 0x47 (bits 7:0 of 0x1647)
            ctx.data[6]           = 0x00;               // no command set versioning
            ctx.data[7]           = 0x00;
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8;
            return;
        }
    }
    // BMC / other interfaces keep the common behaviour.
    app::Control::on_get_vendor_msg_support(rx, tx);
}

void Control::on_get_routing_table_entry(const app::Packet& rx, app::Packet& tx) const
{
#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)
    if (rx.priv.packet_interface == static_cast<uint16_t>(mctp::Client::UsUsbFs)) {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return;
    }
#endif

    fill_packet_header(rx, tx);
    fill_control_msg_header(rx, tx);
    auto& ctx       = Control::PktRes::from(tx);
    auto& crx       = Control::PktReq::from(rx);
    auto  cur_entry = crx.data[0];

    if constexpr (UsI3cBridgeMode) {
        const auto packet_interface = static_cast<mctp::Client>(rx.priv.packet_interface);
        if (packet_interface == mctp::Client::UsI3c) {
            // The CPU is the MCTP bus owner on the I3C link and queries us (its
            // bridge) for the routing table. Return a single valid entry for
            // ourselves over I3C with no next entry so the CPU completes
            // discovery. Without this the request fell through to the USB path
            // and returned MctpOverUsb entries, which the CPU rejects and
            // retries every ~25 ms - an IBI storm that (with I3C IRQ preempting
            // USB) starves the USB/BMC link and drops MCU EID enumeration.
            if (cur_entry != 0) {
                ctx.completion_code   = Ccode::ErrorInvalidData;
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
                return;
            }
            ctx.completion_code   = Ccode::Success;
            ctx.data[0]           = NoNextEntry;
            ctx.data[1]           = RoutingEntryNum;
            ctx.data[2]           = EidSize;
            ctx.data[3]           = _router.ec.cur_eid.at(rx.priv.packet_interface);
            ctx.data[4]           = self_routing_entry_type_port(PortNum);
            ctx.data[5]           = static_cast<uint8_t>(PhyId::MctpOverI3c);
            ctx.data[6]           = static_cast<uint8_t>(PhyMediumId::I3c);
            ctx.data[7]           = PhyAddrSize;
            ctx.data[8]           = 0x00;
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8 + PhyAddrSize;
            return;
        }
        if (packet_interface < mctp::Client::UsEnd && packet_interface != mctp::Client::UsI3c) {
            uint8_t total          = 0;
            uint8_t cpu_alias_slot = 0;
            for (const auto& bridge_endpoint : BridgeEndpointInfos) {
                bool is_visible = false;
                if (bridge_endpoint.type == nv::ipc::MctpBridgeEndpointType::RealDownstream
                    && bridge_endpoint.downstream_slot < EidPoolSize) {
                    const auto& entry = _routing_map.at(bridge_endpoint.downstream_slot);
                    is_visible        = entry.is_enumerated;
                }
                else if (bridge_endpoint.type == nv::ipc::MctpBridgeEndpointType::CpuAlias) {
                    if (cpu_alias_slot < BridgeCpuAliasNum) {
                        const auto& alias = _routing_map.at(CpuAliasSlotBase + cpu_alias_slot);
                        is_visible        = alias.is_enumerated && alias.bridge_real_eid != 0
                                  && alias.client == mctp::Client::UsI3c;
                    }
                    cpu_alias_slot++;
                }
                if (is_visible) {
                    total++;
                }
            }

            if (cur_entry > total) {
                ctx.completion_code   = Ccode::ErrorInvalidData;
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
                return;
            }

            ctx.completion_code = Ccode::Success;
            ctx.data[1]         = RoutingEntryNum;
            ctx.data[2]         = EidSize;
            ctx.data[7]         = PhyAddrSize;

            if (cur_entry == total) {
                ctx.data[0] = NoNextEntry;
                ctx.data[3] = _router.ec.cur_eid.at(rx.priv.packet_interface);
                ctx.data[4] = self_routing_entry_type_port(PortNum);
                ctx.data[5] = static_cast<uint8_t>(PhyId::MctpOverUsb);
                ctx.data[6] = static_cast<uint8_t>(PhyMediumId::Usb20);
                ctx.data[8] = 0x00;
            }
            else {
                uint8_t seen   = 0;
                cpu_alias_slot = 0;
                for (const auto& bridge_endpoint : BridgeEndpointInfos) {
                    bool    is_visible   = false;
                    uint8_t assigned_eid = 0;
                    if (bridge_endpoint.type == nv::ipc::MctpBridgeEndpointType::RealDownstream
                        && bridge_endpoint.downstream_slot < EidPoolSize) {
                        const auto& entry = _routing_map.at(bridge_endpoint.downstream_slot);
                        is_visible        = entry.is_enumerated;
                        assigned_eid      = entry.assigned_eid;
                    }
                    else if (bridge_endpoint.type
                             == nv::ipc::MctpBridgeEndpointType::CpuAlias) {
                        if (cpu_alias_slot < BridgeCpuAliasNum) {
                            const auto& alias = _routing_map.at(CpuAliasSlotBase
                                                                + cpu_alias_slot);
                            is_visible = alias.is_enumerated && alias.bridge_real_eid != 0
                                      && alias.client == mctp::Client::UsI3c;
                            assigned_eid = alias.assigned_eid;
                        }
                        cpu_alias_slot++;
                    }
                    if (!is_visible) {
                        continue;
                    }
                    if (seen == cur_entry) {
                        ctx.data[0] = static_cast<uint8_t>(cur_entry + 1);
                        ctx.data[3] = assigned_eid;
                        ctx.data[4] = bridge_endpoint.client;
                        ctx.data[5] = bridge_endpoint.phy_id;
                        ctx.data[6] = bridge_endpoint.phy_medium_id;
                        ctx.data[8] = bridge_endpoint.port_address;
                        break;
                    }
                    seen++;
                }
            }

            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8 + PhyAddrSize;
            return;
        }
    }

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
            ctx.data[4] = self_routing_entry_type_port(PortNum);
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
            else if (_routing_map.at(index).client == mctp::Client::UsI3c) {
                ctx.data[5] = static_cast<uint8_t>(PhyId::MctpOverI3c);
                ctx.data[6] = static_cast<uint8_t>(PhyMediumId::I3c);
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
                ctx.data[4]           = self_routing_entry_type_port(PortNum);
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
                ctx.data[4]           = self_routing_entry_type_port(PortNum);
                ctx.data[5]           = static_cast<uint8_t>(PhyId::MctpOverUsb);
                ctx.data[6]           = static_cast<uint8_t>(PhyMediumId::Usb20);
                ctx.data[8]           = 0x00;  // physical address
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8 + PhyAddrSize;
                return;
            }
            if (_routing_map.at(entry_in_map).client == mctp::Client::UsUsb) {
                ctx.data[5] = static_cast<uint8_t>(PhyId::MctpOverUsb);
                ctx.data[6] = static_cast<uint8_t>(PhyMediumId::Usb20);
                ctx.data[8] = 0x00;
            }
            else {
                int32_t downstream_index = eid_diff;
                if constexpr (UsI3cBridgeMode) {
                    downstream_index -= DownStreamStartOffset;
                }
                if (downstream_index < 0 || downstream_index > OneByteMask) {
                    logger::error(nv::logger::Event::MctpInvalidEidDifference,
                                  {static_cast<uint8_t>(downstream_index & OneByteMask),
                                   _routing_map.at(entry_in_map).assigned_eid,
                                   _start_eid,
                                   DownStreamStartOffset});
                    ctx.data[0]           = NoNextEntry;
                    ctx.data[3]           = _router.ec.cur_eid.at(rx.priv.packet_interface);
                    ctx.data[4]           = self_routing_entry_type_port(PortNum);
                    ctx.data[5]           = static_cast<uint8_t>(PhyId::MctpOverUsb);
                    ctx.data[6]           = static_cast<uint8_t>(PhyMediumId::Usb20);
                    ctx.data[8]           = 0x00;
                    tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8
                                          + PhyAddrSize;
                    return;
                }
                const auto index = static_cast<std::size_t>(
                    static_cast<uint8_t>(downstream_index) & OneByteMask);
                if (index >= ipc::DownStreamInfos.size()) {
                    logger::error(nv::logger::Event::MctpInvalidEidDifference,
                                  {static_cast<uint8_t>(index),
                                   _routing_map.at(entry_in_map).assigned_eid,
                                   _start_eid,
                                   static_cast<uint8_t>(ipc::DownStreamInfos.size())});
                    ctx.data[0]           = NoNextEntry;
                    ctx.data[3]           = _router.ec.cur_eid.at(rx.priv.packet_interface);
                    ctx.data[4]           = self_routing_entry_type_port(PortNum);
                    ctx.data[5]           = static_cast<uint8_t>(PhyId::MctpOverUsb);
                    ctx.data[6]           = static_cast<uint8_t>(PhyMediumId::Usb20);
                    ctx.data[8]           = 0x00;
                    tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8
                                          + PhyAddrSize;
                    return;
                }
                auto& ds_info = ipc::DownStreamInfos.at(index);
                ctx.data[5]   = static_cast<uint8_t>(ds_info.phy_id);
                ctx.data[6]   = static_cast<uint8_t>(ds_info.phy_medium_id);
                ctx.data[8]   = static_cast<uint8_t>(ds_info.port_address);
            }

            ctx.data[0] = ++cur_entry;
        }

        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8 + PhyAddrSize;
    }
}
