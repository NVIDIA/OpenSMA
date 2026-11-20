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

#include <bit>
#include <cstdint>

#include "app/pdk-mctp-app-packet.h"
#include "pdk-mctp-platforms-config.h"

namespace pdk::mctp::platforms {
void on_enumerate_plat();
// Called once after a Routing Information Update request has been processed and
// at least one entry was stored. `update_eid` is the last entry stored: the
// strict handler applies exactly one per request, while the tolerant one may
// apply several and leaves the full result in the routing table.
void    on_routing_info_update_plat(uint8_t requester_eid,
                                    uint8_t requester_interface,
                                    uint8_t update_eid);
bool    should_allocate_downstream_pool(uint8_t packet_interface);
uint8_t get_upstream_routing_index(uint8_t packet_interface);
uint8_t get_reported_eid_pool_size(uint8_t packet_interface);
// EID that downstream routing slot `index` must be assigned, given the start EID of the
// pool its bus owner allocated. The slot index alone is not the offset on products that
// bridge a second upstream domain: those reserve leading EIDs of the pool for bridge
// aliases. Both the allocate-time pre-fill and the Set Endpoint ID path resolve the EID
// through here so they can never disagree.
uint8_t get_downstream_pool_eid(uint8_t start_eid, uint8_t index);
}  // namespace pdk::mctp::platforms
