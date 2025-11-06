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
#include <span>

#include "app/pdk-mctp-app-packet.h"
namespace pdk::mctp::app {
class Composer
{
public:
    Composer();

    void clear();

    // Function Usage : Assembly pkt and store in buf , if pkt is the last packet
    // of multi-packet, write pkt_size of sizeof(Private Header + MCTP Header + MCTP payload)
    // pkt_size : the full multi-packet size (including Private Header)
    // Return false in 2 case
    // 1. Unknown packet (Without a start packet) (Case 4)
    // 2. Current packet is not the Last packet of multi-packet
    // 3. Single packet which has overflow packet length (Likely not happen)
    bool recv_packet(platforms::Packet::LengthType& pkt_size,
                     const Packet&                  pkt,
                     std::span<uint8_t>             buf);

    // buf will store a full array of multi-packet :
    // (Private Header) (MCTP Header) (Msg 0) (Msg 1) ... (Msg n)
    // pkt will store and return index'th single packet
    // index'th single packet : (Private_Header) (Header) (Msg i)
    // bytes : Size of (MCTP Header) (Msg 0) (Msg 1) ... (Msg n)
    // index : Indicate which index'th single packet to store in pkt
    bool gen_packet(
        Packet& pkt, uint32_t bytes, uint32_t index, bool& is_complete, std::span<uint8_t> buf);
    static bool construct_mctp_header(Packet&                          pkt,
                                      platforms::Packet::LengthType    bytes,
                                      bool                             is_response,
                                      uint8_t                          dst_eid,
                                      platforms::Packet::InterfaceType client,
                                      uint8_t                          msg_tag,
                                      uint8_t                          hdr_ver);

protected:
    bool     _completed        = true;   // Whether assembly is completed
    bool     _som_recv         = false;  // Whether recv 1st packet of multi-packet
    bool     _overflowed       = false;  // Whether multi packet overflow _max_buf_size
    uint32_t _full_packet_size = 0;      // Record total size of multi pkt (include priv, hdr)
};
}  // namespace pdk::mctp::app
