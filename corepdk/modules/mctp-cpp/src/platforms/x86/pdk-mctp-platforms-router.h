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
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "app/pdk-mctp-app-common.h"
#include "app/pdk-mctp-app-enums.h"
#include "app/pdk-mctp-app-packet.h"

namespace pdk::mctp::platforms {

using PrivateEidMap = std::array<uint8_t, static_cast<size_t>(Interface::UsEnd)>;

struct ShardRoutingTable
{
    bool      is_need_enumerate;
    bool      is_enumerated;
    uint8_t   assigned_eid;
    Interface client;
};

struct RoutingTableEntry
{
    app::PhyId       phy_id;
    app::PhyMediumId phy_medium_id;
    Interface        client;
    uint8_t          port_address;
};

struct [[gnu::packed]] RoutingTable
{
    struct [[gnu::packed]] Routing
    {
        struct Bitfield
        {
            uint8_t eid_type      : 2;
            uint8_t eid_state     : 2;
            uint8_t endpoint_type : 2;
            uint8_t disc          : 1;
            uint8_t rsvd          : 1;
        };

        uint8_t                             default_eid;
        pdk::mctp::platforms::PrivateEidMap cur_eid;
        uint8_t                             smb_address;
        pdk::mctp::app::Uuid                uuid;
    } ec, mc;
};

}  // namespace pdk::mctp::platforms
