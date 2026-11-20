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
#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-control-plat.h"

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

bool pdk::mctp::platforms::should_allocate_downstream_pool(uint8_t packet_interface)
{
    if constexpr (UsI3cBridgeMode) {
        return packet_interface != static_cast<uint8_t>(Client::UsI3c);
    }
    return true;
}

uint8_t pdk::mctp::platforms::get_upstream_routing_index(uint8_t packet_interface)
{
    if constexpr (UsI3cBridgeMode) {
        if (packet_interface == static_cast<uint8_t>(Client::UsI3c)) {
            return EidPoolSize + 2;
        }
    }
    return EidPoolSize;
}

uint8_t pdk::mctp::platforms::get_reported_eid_pool_size(uint8_t packet_interface)
{
    if constexpr (UsI3cBridgeMode) {
        if (packet_interface == static_cast<uint8_t>(Client::UsUsb)) {
            if constexpr (BridgeEndpointPoolSize > 0) {
                return BridgeEndpointPoolSize;
            }
            return static_cast<uint8_t>(DownStreamStartOffset + EidPoolSize
                                        + BridgeCpuAliasNum);
        }
    }
    return EidPoolSize;
}

uint8_t pdk::mctp::platforms::get_downstream_pool_eid(uint8_t start_eid, uint8_t index)
{
    uint8_t offset = index;
    if constexpr (UsI3cBridgeMode) {
        // The bus owner's pool starts with the bridge's own aliases, so downstream
        // slot 0 does not sit at the start of the pool.
        offset = static_cast<uint8_t>(DownStreamStartOffset + index);
        if constexpr (BridgeEndpointNum > 0) {
            for (const auto& bridge_endpoint : BridgeEndpointInfos) {
                if (bridge_endpoint.type == nv::ipc::MctpBridgeEndpointType::RealDownstream
                    && bridge_endpoint.downstream_slot == index) {
                    offset = bridge_endpoint.eid_offset;
                    break;
                }
            }
        }
    }

    const uint16_t eid = static_cast<uint16_t>(start_eid) + offset;
    return (eid > UINT8_MAX) ? UINT8_MAX : static_cast<uint8_t>(eid);
}

void pdk::mctp::platforms::on_enumerate_plat()
{
    // notify to do enumerate
    Driver::mctp_send_cmd(Driver::CmdCode::Enumerate);
    logger::info(nv::logger::Event::MctpEnumerated, {});
}

void pdk::mctp::platforms::on_routing_info_update_plat(uint8_t requester_eid,
                                                       uint8_t requester_interface,
                                                       uint8_t update_eid)
{
    logger::info(nv::logger::Event::MctpRoutingInfoUpdateRequesterV2,
                 {requester_eid, requester_interface, update_eid});

    // Use on_enumerate_done to update routing info to each task
    Driver::mctp_send_cmd(Driver::CmdCode::EnumerateDone);
}
