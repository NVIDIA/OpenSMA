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

#include "nv/mctp/control.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/interface.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;

TEST(MctpControlSetEndpointEid, SetEndPointIdNormal)
{
    using namespace mctp;

    constexpr uint8_t src_eid = 0xC8, dst_eid = 0x27;
    constexpr uint8_t interface = 0x00;
    //                hdr_ver rsvd    dst eid  src eid  msgtag tag, seq   eom  som
    Header        hdr{0b0001, 0b0000, dst_eid, src_eid, 0b000, 0b1, 0b00, 0b0, 0b0};
    PrivateHeader prv{0x02, interface, 0};

    Packet rx{prv, hdr}, tx{};
    // illegal endpoint 00
    auto &req = Control::PktReq::from(rx);
    req.msg_type = MsgType::Control;
    req.ic       = 0;
    req.instance_id = 0;
    req.rsvd0       = 0;
    req.d           = 0;
    req.rq          = 1;
    req.command_code = Cmd::SetEpId;
    req.data[0] = common::to_underlying(SetEndpoint::SetEidNormal);
    req.data[1] = 0;

    auto& res = Control::PktRes::from(tx);

    Control ctl{};
    auto&   router  = ctl.router();
    auto    cur_eid = router.ec.cur_eid[interface];

    auto set_eid_in_packet = [&](uint8_t eid) {
        req.data[1] = eid;
        memcpy(&rx.hdr, &req, sizeof(req));
        tx = {};
        ctl.on_set_endpoint_id(rx, tx);
    };

    // 1. 00 endpoint ID is invalid
    set_eid_in_packet(0x00);
    ensure::is_eq(router.ec.cur_eid[interface], cur_eid);  // untouched
    expect::is_eq(res.completion_code, Ccode::ErrorInvalidData);

    // 2. FF endpoint ID is invalid
    set_eid_in_packet(0xFF);
    expect::is_eq(router.ec.cur_eid[interface], cur_eid);
    expect::is_eq(res.completion_code, Ccode::ErrorInvalidData);

    // 3. all others should be valid
    for (int eid = 1; eid < 0xff; eid++) {
        set_eid_in_packet(eid);
        ensure::is_eq(router.ec.cur_eid[interface], eid);
        ensure::is_eq(res.completion_code, Ccode::Success);
    }
};

#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)
TEST(MctpControlUsbFs, ReportsEndpointOnlyCapabilities)
{
    using namespace mctp;

    constexpr uint8_t HostEid = 0xC8;
    constexpr uint8_t McuEid  = 0x27;
    constexpr auto    UsbFs   = static_cast<uint8_t>(Client::UsUsbFs);

    Packet rx{};
    Packet tx{};
    rx.priv.packet_interface = UsbFs;
    rx.hdr.dst_eid           = 0;
    rx.hdr.src_eid           = HostEid;
    rx.hdr.som               = 1;
    rx.hdr.eom               = 1;
    rx.hdr.tag_owner         = 1;

    auto& req        = Control::PktReq::from(rx);
    req.msg_type     = MsgType::Control;
    req.rq           = 1;
    req.command_code = Cmd::SetEpId;
    req.data[0]      = common::to_underlying(SetEndpoint::SetEidNormal);
    req.data[1]      = McuEid;

    Control control{};
    control.process(rx, tx);

    auto& set_eid_resp = Control::PktRes::from(tx);
    ensure::is_eq(set_eid_resp.completion_code, Ccode::Success);
    ensure::is_eq(set_eid_resp.data[0],
                  common::to_underlying(SetEndpoint::EidAcceptedAndNoEidPool));
    ensure::is_eq(set_eid_resp.data[1], McuEid);
    ensure::is_eq(set_eid_resp.data[2], 0);

    tx               = {};
    req.command_code = Cmd::GetEpId;
    control.process(rx, tx);

    auto& get_eid_resp = Control::PktRes::from(tx);
    ensure::is_eq(get_eid_resp.completion_code, Ccode::Success);
    ensure::is_eq(get_eid_resp.data[0], McuEid);
    ensure::is_eq(get_eid_resp.data[1],
                  common::to_underlying(EndpointIdType::DynamicEid));
    ensure::is_eq(get_eid_resp.data[2], 0);
    ensure::is_eq(tx.priv.packet_interface, UsbFs);
    ensure::is_eq(tx.hdr.dst_eid, HostEid);
    ensure::is_eq(tx.hdr.src_eid, McuEid);

    tx               = {};
    req.command_code = Cmd::GetRoutTableEntry;
    req.data[0]      = 0;
    control.process(rx, tx);

    auto& routing_resp = Control::PktRes::from(tx);
    ensure::is_eq(routing_resp.completion_code, Ccode::ErrorUnsupportedCmd);
    ensure::is_eq(tx.priv.packet_interface, UsbFs);
    ensure::is_eq(tx.hdr.dst_eid, HostEid);
    ensure::is_eq(tx.hdr.src_eid, McuEid);
}

TEST(MctpControlUsbFs, CannotOwnOrModifyDownstreamPool)
{
    using namespace mctp;

    constexpr auto    UsbFs   = static_cast<uint8_t>(Client::UsUsbFs);
    constexpr auto    UsbHs   = static_cast<uint8_t>(Client::UsUsb);
    constexpr uint8_t HostEid = 0xC8;
    constexpr uint8_t McuEid  = 0x27;

    Control control{};
    control.router().ec.cur_eid.at(UsbFs) = McuEid;

    Packet rx{}, tx{};
    rx.hdr.src_eid   = HostEid;
    rx.hdr.som       = 1;
    rx.hdr.eom       = 1;
    rx.hdr.tag_owner = 1;
    auto& req       = Control::PktReq::from(rx);
    req.msg_type    = MsgType::Control;
    req.rq          = 1;

    auto reject_fs_requests = [&]() {
        const auto owner      = control.get_allocate_eid_bus();
        const auto owner_eid  = control.get_allocate_src_eid();
        const auto start_eid  = control.get_start_eid();
        const auto pool_eid   = control.get_ds_pool_start_eid();
        const auto allocated  = control.is_ds_pool_allocated();
        const auto additional = control.get_additional_eid_count();
        const auto map        = control.routing_map();

        rx.priv.packet_interface = UsbFs;
        for (auto command : {Cmd::AllocateEpId, Cmd::RoutingInfoUpdate}) {
            req.command_code = command;
            for (auto operation : {AllocateEndpoint::Allocate,
                                   AllocateEndpoint::ForceAllocate,
                                   AllocateEndpoint::GetInfo}) {
                req.data[0] = common::to_underlying(operation);
                req.data[1] = 1;
                req.data[2] = 0x40;
                if (command == Cmd::RoutingInfoUpdate) {
                    req.data[0] = 1;
                    req.data[1] = common::to_underlying(EntryType::SingleEndpoint);
                    req.data[2] = 1;
                    req.data[3] = 0x70;
                }
                tx          = {};
                control.process(rx, tx);

                ensure::is_eq(Control::PktRes::from(tx).completion_code,
                              Ccode::ErrorUnsupportedCmd);
                ensure::is_eq(tx.priv.packet_interface, UsbFs);
                ensure::is_eq(tx.hdr.dst_eid, HostEid);
                ensure::is_eq(tx.hdr.src_eid, McuEid);
                ensure::is_eq(control.get_upstream_src_eid(Client::UsUsbFs), 0);
                ensure::is_eq(control.get_allocate_eid_bus(), owner);
                ensure::is_eq(control.get_allocate_src_eid(), owner_eid);
                ensure::is_eq(control.get_start_eid(), start_eid);
                ensure::is_eq(control.get_ds_pool_start_eid(), pool_eid);
                ensure::is_eq(control.is_ds_pool_allocated(), allocated);
                ensure::is_eq(control.get_additional_eid_count(), additional);
                for (size_t i = 0; i < map.size(); ++i) {
                    const auto& entry = control.routing_map().at(i);
                    ensure::is_eq(entry.assigned_eid, map.at(i).assigned_eid);
                    ensure::is_eq(entry.client, map.at(i).client);
                    ensure::is_eq(entry.is_need_enumerate, map.at(i).is_need_enumerate);
                    ensure::is_eq(entry.is_enumerated, map.at(i).is_enumerated);
                    ensure::is_eq(entry.is_set_eid_sent, map.at(i).is_set_eid_sent);
                    ensure::is_eq(entry.bridge_real_eid, map.at(i).bridge_real_eid);
                }
            }
        }
    };

    reject_fs_requests();

    rx.priv.packet_interface = UsbHs;
    req.command_code         = Cmd::AllocateEpId;
    req.data[0]              = common::to_underlying(AllocateEndpoint::Allocate);
    req.data[1]              = nv::ipc::DownStreamNum;
    req.data[2]              = 0x30;
    tx                       = {};
    control.process(rx, tx);
    ensure::is_eq(Control::PktRes::from(tx).completion_code, Ccode::Success);
    ensure::is_eq(Control::PktRes::from(tx).data[0],
                  common::to_underlying(AllocateEndpointStatus::Accept));
    ensure::is_eq(control.get_allocate_eid_bus(), UsbHs);
    ensure::is_eq(control.get_upstream_src_eid(Client::UsUsb), HostEid);

    reject_fs_requests();
}
#endif

TEST(MctpControl, SetEndPointIdForced)
{
    ensure::is_true(true);
};

TEST(MctpControl, SetEndPointIdReset)
{
    ensure::is_true(true);
};

TEST(MctpControl, SetEndPointIdDiscovered)
{
    ensure::is_true(true);
};
