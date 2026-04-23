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

/**
 * @file mctp_router.h
 * @brief Shared MCTP packet routing logic for USB tasks.
 *
 * Provides routing table lookup and downstream forwarding used by both
 * the single-core USB task (nv::usb::Task) and dual-core USB proxy
 * (nv::usb_proxy::Task). The only difference between the two is the I/O
 * layer: single-core talks to the USB hardware driver directly, while
 * dual-core proxies through C2C IPC. The routing logic is identical.
 */
#pragma once

#include <array>
#include <cstdint>

#include "nv/ipc/queue.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/router.h"

namespace nv::usb {

using RoutingTable = std::array<mctp::ShardRoutingTable, ipc::RoutingTableSize>;

/**
 * @brief Result of MCTP downstream routing attempt.
 */
enum class RouteResult
{
    Forwarded,  ///< Packet was forwarded to a downstream task (I2C/I3C/SPI)
    Dropped,    ///< Packet was dropped (bridge filter or downstream send error)
    NotRouted,  ///< No routing table match — caller should send to MCTP driver
};

/**
 * @brief Route an MCTP packet to a downstream task using the routing table.
 *
 * Looks up pkt.hdr.dst_eid in @p routing_table. On match, applies bridge
 * filtering, then forwards the packet to the appropriate I2C/I3C/SPI task.
 *
 * @param pkt            MCTP packet (packet_interface field may be updated)
 * @param routing_table  Current routing table snapshot
 * @return RouteResult indicating how the packet was handled
 */
RouteResult route_mctp_to_downstream(mctp::Packet& pkt, const RoutingTable& routing_table);

/**
 * @brief Receive a routing table update from the RoutingTable queue.
 *
 * @param routing_table  Routing table to populate
 */
void update_routing_table(RoutingTable& routing_table);

}  // namespace nv::usb
