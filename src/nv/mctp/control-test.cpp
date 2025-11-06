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
