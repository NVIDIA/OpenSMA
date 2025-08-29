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

namespace pdk::mctp::app {

struct [[gnu::packed]] TransportHeader
{
    uint8_t hdr_ver : 4;    ///< Header Version
    uint8_t rsvd    : 4;    ///< MCTP Reserved
    uint8_t dst_eid;        ///< Destination Endpoint ID
    uint8_t src_eid;        ///< Source Endpoint ID
    uint8_t msg_tag   : 3;  ///< Message Tag
    uint8_t tag_owner : 1;  ///< Tag Owner
    uint8_t pkt_seq   : 2;  ///< Packet Sequence Number
    uint8_t eom       : 1;  ///< End Of Message
    uint8_t som       : 1;  ///< Start Of Message
};

// Refer to MCTP Base Specification 12.5 Get Endpoint UUID
// https://www.dmtf.org/sites/default/files/standards/documents/DSP0236_1.3.1.pdf
constexpr size_t UuidLen = 16;
using Uuid               = std::array<uint8_t, UuidLen>;

}  // namespace pdk::mctp::app