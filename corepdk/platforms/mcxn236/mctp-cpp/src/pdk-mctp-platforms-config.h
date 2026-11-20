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

#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-enums.h"
#include "corepdk/platforms/mcxn236/mctp-cpp/src/pdk-mctp-platforms-router.h"

#include "nv/ipc/mctp_bridge_config.h"
#include "nv/ipc/queue.h"

namespace pdk::mctp::platforms {
constexpr uint8_t   DownStreamNum         = nv::ipc::DownStreamNum;
constexpr uint8_t   DownStreamStartOffset = nv::ipc::MctpBridgeConfig<>::DownStreamStartOffset;
constexpr uint8_t   UpStreamNum           = nv::ipc::UpStreamNum;
constexpr uint8_t   DefaultRoutingTableSize  = nv::ipc::DefaultRoutingTableSize;
constexpr uint8_t   EidPoolSize              = DownStreamNum;
constexpr uint8_t   RoutingInfoUpdateSize    = nv::ipc::RoutingInfoUpdateSize;
constexpr uint8_t   RoutingTableSize         = nv::ipc::RoutingTableSize;
constexpr Interface DefaultInterface         = Interface::UsUsb;
constexpr bool      UsI3cBridgeMode          = nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode;
constexpr bool      EnableUsI3cDebugLog      = nv::ipc::MctpBridgeConfig<>::EnableUsI3cDebugLog;
constexpr uint8_t   BridgeEndpointNum        = nv::ipc::MctpBridgeConfig<>::BridgeEndpointNum;
constexpr uint8_t   BridgeCpuAliasNum        = nv::ipc::MctpBridgeConfig<>::BridgeCpuAliasNum;
constexpr uint8_t   BmcAliasSlotIndex        = nv::ipc::MctpBridgeConfig<>::BmcAliasSlotIndex;
constexpr uint8_t   CpuAliasSlotBase         = nv::ipc::MctpBridgeConfig<>::CpuAliasSlotBase;
constexpr inline auto BridgeEndpointInfos    = nv::ipc::MctpBridgeConfig<>::BridgeEndpointInfos;
constexpr uint8_t     BridgeEndpointPoolSize = nv::ipc::get_bridge_endpoint_pool_size(
    BridgeEndpointInfos);
static_assert(BridgeEndpointInfos.size() == BridgeEndpointNum,
              "BridgeEndpointInfos must match BridgeEndpointNum");
static_assert(nv::ipc::has_unique_bridge_endpoint_offsets(BridgeEndpointInfos),
              "Bridge endpoint EID offsets must be unique");

constexpr inline std::array<RoutingTableEntry, DownStreamNum>
    DownStreamInfos = nv::ipc::DownStreamInfos;

constexpr std::array<app::MsgType, 4> SupportedType = {
    app::MsgType::Pldm,
    app::MsgType::Spdm,
    app::MsgType::VendorPci,
    app::MsgType::VendorIani,
};
constexpr uint8_t SupportedTypeNum = static_cast<uint8_t>(SupportedType.size());

// Per-product policy: on a point-to-point upstream link where this device is
// the sole target, the bus owner may address us by an EID we no longer
// recognise (e.g. the one it assigned before our reset). Accept control-plane
// messages on such a link regardless of dst EID so the owner always gets a
// response instead of retrying (an IBI storm). Gated on the product's
// UsI3cBridgeMode so only the CPU-bridge products enable it; the
// common validator only queries this predicate.
constexpr bool interface_accepts_any_dst_control(Interface interface)
{
    return UsI3cBridgeMode && (interface == Interface::UsI3c);
}

// A CPU-bridge product is enumerated by two bus owners at once, the BMC upstream
// and the bridged CPU, so neither can be latched as the sole owner of the EID
// pool. Every other product follows DSP0236: the first interface to allocate
// owns the pool and any other one is answered Reject.
constexpr bool allows_concurrent_bus_owners()
{
    return UsI3cBridgeMode;
}

// The CPU bridge's bus owner pushes its whole routing table in one request and
// retries forever if a single entry is refused, so those products take every
// entry and silently skip what they cannot store. Everywhere else the bus owner
// is told what it got wrong.
constexpr bool tolerant_routing_info_update()
{
    return UsI3cBridgeMode;
}

// DSP0236 Get Routing Table Entries "Entry Type / Port Number" byte for this
// device's own entry: [7:6] entry type, [5] static EID (0, ours is dynamic),
// [4:0] port. A device that routes for any EID other than its own is an MCTP
// bridge and must say so; only a product with no downstream pool and no Routing
// Information Update slots is a plain endpoint.
constexpr uint8_t self_routing_entry_type_port(uint8_t port)
{
    constexpr uint8_t EntryTypeShift = 6;
    constexpr uint8_t PortMask       = 0x1F;
    constexpr bool    IsBridge       = (EidPoolSize > 0) || (RoutingInfoUpdateSize > 0)
                           || UsI3cBridgeMode;
    constexpr auto Type = IsBridge ? app::EntryType::SingleEndpointBridge
                                   : app::EntryType::SingleEndpoint;
    return static_cast<uint8_t>((static_cast<uint8_t>(Type) << EntryTypeShift)
                                | (port & PortMask));
}

// Whether `packet_interface` is the bridged CPU's I3C link. Only the CPU-bridge
// products have that interface at all, so everywhere else this folds to false and
// the callers are left with their single-upstream behaviour.
constexpr bool is_us_i3c_interface(uint8_t packet_interface)
{
    return UsI3cBridgeMode && (packet_interface == static_cast<uint8_t>(Interface::UsI3c));
}

// Interfaces whose Routing Information Update carries an address that Get Routing
// Table Entry has to report back. The SMBus upstream always does; the bridged CPU
// link only exists on the CPU-bridge products.
constexpr bool should_store_additional_address(uint8_t packet_interface)
{
    return packet_interface == static_cast<uint8_t>(Interface::UsI2c)
        || is_us_i3c_interface(packet_interface);
}

}  // namespace pdk::mctp::platforms