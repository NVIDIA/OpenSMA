/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

namespace nv::ipc {

enum class MctpBridgeEndpointType : uint8_t
{
    RealDownstream,
    CpuAlias,
};

struct MctpBridgeEndpointInfo
{
    uint8_t                eid_offset;
    MctpBridgeEndpointType type;
    uint8_t                downstream_slot;
    uint8_t                phy_id;
    uint8_t                phy_medium_id;
    uint8_t                client;
    uint8_t                port_address;
};

template<std::size_t N>
constexpr bool
has_unique_bridge_endpoint_offsets(const std::array<MctpBridgeEndpointInfo, N>& endpoints)
{
    for (std::size_t i = 0; i < N; i++) {
        for (std::size_t j = i + 1; j < N; j++) {
            if (endpoints[i].eid_offset == endpoints[j].eid_offset) {
                return false;
            }
        }
    }
    return true;
}

template<std::size_t N>
constexpr uint8_t
get_bridge_endpoint_pool_size(const std::array<MctpBridgeEndpointInfo, N>& endpoints)
{
    uint16_t pool_size = 0;
    for (const auto& endpoint : endpoints) {
        const uint16_t endpoint_pool_size = static_cast<uint16_t>(endpoint.eid_offset) + 1U;
        if (endpoint_pool_size > pool_size) {
            pool_size = endpoint_pool_size;
        }
    }
    return static_cast<uint8_t>(pool_size);
}

struct DefaultMctpBridgeConfig
{
    static constexpr bool    UsI3cBridgeMode       = false;
    static constexpr uint8_t BridgeEndpointNum     = 0;
    static constexpr uint8_t BridgeCpuAliasNum     = 0;
    static constexpr uint8_t BmcAliasSlotIndex     = 0;
    static constexpr uint8_t CpuAliasSlotBase      = 0;
    static constexpr uint8_t DownStreamStartOffset = 0;
    // Product switch for the periodic UsI3c/MCTP-I3C bridge debug traces (CCC
    // sent, force-bus-idle, pull-up status, CPU rx, forward-to-CPU). Off by
    // default so other products are unaffected and pay no cost.
    static constexpr bool EnableUsI3cDebugLog = false;
    // Product switch for deriving CPU liveness on the UsI3c link from the I3C
    // readiness GPIO, which requires I3CPullUpCheck to select that pin. Off by
    // default: other products aim that same pin at a board power-good rail, where
    // it only gates DAA and must not drive MCTP endpoint status.
    static constexpr bool EnableUsI3cReadyGpioCheck = false;
    static constexpr std::array<MctpBridgeEndpointInfo, BridgeEndpointNum>
        BridgeEndpointInfos{};
};

template<typename = void>
struct MctpBridgeConfig : DefaultMctpBridgeConfig
{};

}  // namespace nv::ipc
