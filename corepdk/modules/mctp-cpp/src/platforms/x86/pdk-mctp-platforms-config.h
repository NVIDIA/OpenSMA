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

#include "app/pdk-mctp-app-enums.h"
#include "pdk-mctp-platforms-router.h"

namespace pdk::mctp::platforms {
// moving testrunner's config here for x86 build since we can't use nv::ipc in mctp-cpp module
constexpr uint8_t   DownStreamNum           = 0;
constexpr uint8_t   UpStreamNum             = 1;
constexpr uint8_t   DefaultRoutingTableSize = DownStreamNum + UpStreamNum;
constexpr uint8_t   EidPoolSize             = DownStreamNum;
constexpr uint8_t   RoutingInfoUpdateSize   = 4;
constexpr uint8_t   RoutingTableSize        = DefaultRoutingTableSize + RoutingInfoUpdateSize;
constexpr bool      I2cIsEndpoint           = false;
constexpr bool      I2cTransparent          = false;
constexpr Interface DefaultInterface        = Interface::UsUsb;
constexpr inline std::array<RoutingTableEntry, DownStreamNum> DownStreamInfos{

};

constexpr std::array<app::MsgType, 4> SupportedType = {
    app::MsgType::Pldm,
    app::MsgType::Spdm,
    app::MsgType::VendorPci,
    app::MsgType::VendorIani,
};
constexpr uint8_t SupportedTypeNum = static_cast<uint8_t>(SupportedType.size());

// x86 test platform has no point-to-point bus-owner bridge link, so control
// messages are always validated strictly. See the mcxn236 platform config for
// the product-gated (UsI3cBridgeMode) version. The common validator only
// queries this predicate.
constexpr bool interface_accepts_any_dst_control(Interface)
{
    return false;
}

// Single bus owner, so Allocate Endpoint ID latches the first interface to
// allocate and Routing Information Update reports the first problem back to the
// owner. See the mcxn236 platform config for the product-gated versions.
constexpr bool allows_concurrent_bus_owners()
{
    return false;
}

constexpr bool tolerant_routing_info_update()
{
    return false;
}

constexpr bool is_us_i3c_interface(uint8_t packet_interface)
{
    return packet_interface == static_cast<uint8_t>(Interface::UsI3c);
}

// Interfaces whose Routing Information Update carries an address that Get Routing
// Table Entry has to report back.
constexpr bool should_store_additional_address(uint8_t packet_interface)
{
    return packet_interface == static_cast<uint8_t>(Interface::UsI2c)
        || is_us_i3c_interface(packet_interface);
}

}  // namespace pdk::mctp::platforms