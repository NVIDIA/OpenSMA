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
#include "app/pdk-mctp-app-router-plat.h"
#include "pdk-mctp-platforms-config.h"
#include "pdk/cmn/log/log.h"

namespace pdk::mctp::platforms {

uint8_t get_cur_eid(const RoutingTable& routing_table, Packet::InterfaceType interface)
{
    if (interface >= static_cast<uint16_t>(Interface::UsEnd)) {
        pdk::cmn::log::here().info("MCTP invalid interface index: %d\n", interface);
        return routing_table.ec.cur_eid.at(
            static_cast<Packet::InterfaceType>(DefaultInterface));
    }
    return routing_table.ec.cur_eid.at(interface);
}

void set_cur_eid(RoutingTable& routing_table, Packet::InterfaceType interface, uint8_t eid)
{
    routing_table.ec.cur_eid.at(interface) = eid;
}

pdk::mctp::app::Uuid get_uuid(const RoutingTable& routing_table)
{
    return routing_table.ec.uuid;
}
}  // namespace pdk::mctp::platforms