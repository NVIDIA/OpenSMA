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
#include "app/pdk-mctp-app-composer.h"

#include <cstring>
#include <limits>

#include "app/pdk-mctp-app-packet-plat.h"

namespace pdk::mctp::app {

Composer::Composer()
{
    clear();
}

void Composer::clear()
{
    _completed        = true;
    _som_recv         = false;
    _overflowed       = false;
    _full_packet_size = 0;
}

bool Composer::recv_packet(platforms::Packet::LengthType& pkt_size,
                           const Packet&                  pkt,
                           std::span<uint8_t>             buf)
{
    const auto current_packet_length = platforms::get_packet_length(pkt);
    /* Case 1 : Only 1 packet */
    if (pkt.hdr.som && pkt.hdr.eom) {
        /* Indicate previous packet is not finish -> drop previous */
        if (!_completed) {
            clear();
        }

        if (current_packet_length
            > std::numeric_limits<platforms::Packet::LengthType>::max() - privSize) {
            return false;
        }
        else {
            pkt_size = current_packet_length + privSize;
        }
        memcpy(buf.data(), &pkt, pkt_size);
        _completed = true;
    }
    /* Case 2 : Receipt a new start packet */
    else if (pkt.hdr.som) {
        /* Indicate previous packet is not finish -> drop previous */
        if (!_completed) {
            clear();
        }

        /* Start new packet assembly */
        // Copy full packet
        _full_packet_size = sizeof(Packet);
        memcpy(buf.data(), &pkt, _full_packet_size);

        _completed = false;
        _som_recv  = true;
    }
    // Case 3 : multi-packet message -> continued packets (Have receive 1st
    // packet)
    else if (_som_recv == true) {
        uint32_t msg_bytes = 0;
        // Condition 1 : last packet
        if (pkt.hdr.eom) {
            if (current_packet_length > hdrSize) {
                msg_bytes = current_packet_length - hdrSize;
            }
            // limit bytes to msg buffer size
            if (msg_bytes > msgSize) {
                msg_bytes = msgSize;
            }
            _som_recv = false;  // clean up _som_recv
        }
        // Condition 2 : middle packet
        else {
            msg_bytes = msgSize;
        }

        // make sure the buffer doesn't overflow and the checks don't overflow
        // the above check against messageSize will work to prevent overflow
        // as long as message is within one packet larger than buf
        // the below check is to handle case where a multi-packet msg
        // has overall length greater than 64 bytes larger than buf
        if (!_overflowed && ((_full_packet_size + msg_bytes) > buf.size())) {
            // TODO: this should never happen, we need to return an error here?
            // Setting flag to true will bypass this trace until flag is cleared
            _overflowed = true;
        }
        else {
            if (!_overflowed && (_full_packet_size < buf.size())) {
                // only copy if not overflowed
                memcpy(buf.data() + _full_packet_size, pkt.msg.begin(), msg_bytes);
                _full_packet_size += msg_bytes;
            }

            if (pkt.hdr.eom) {
                if (_overflowed) {
                    // throw away the bad multipacket
                    clear();
                }
                else {
                    // complete the good multipacket
                    pkt_size   = _full_packet_size;
                    _completed = true;
                }
            }
        }
    }
    /* Case 4 : Unknown middle and end packet -> Drop */
    else {
        return false;
    }
    return _completed;
}
bool Composer::gen_packet(
    Packet& pkt, uint32_t bytes, uint32_t index, bool& is_complete, std::span<uint8_t> buf)
{
    is_complete = false;

    // Check for buffer overflow
    if (bytes + privSize > buf.size()) {
        return false;
    }

    auto size = index >= 1 ? index * msgSize + hdrSize : hdrSize;

    // Invalid index
    if (size > bytes) {
        return false;
    }

    // Start with full-sized packets
    std::memcpy(&pkt.priv, buf.data(), privSize);
    std::memcpy(&pkt.hdr, buf.data() + privSize, hdrSize);
    platforms::set_packet_length(pkt, hdrSize + msgSize);

    const uint32_t payloadBytes = (msgSize < bytes - size) ? msgSize : bytes - size;

    pkt.hdr.som     = (size == hdrSize);
    pkt.hdr.eom     = (size + msgSize >= bytes);
    pkt.hdr.pkt_seq = (index & static_cast<uint8_t>(0b11));

    if (pkt.hdr.eom) {
        if (payloadBytes
            > std::numeric_limits<platforms::Packet::LengthType>::max() - hdrSize) {
            return false;
        }
        else {
            // Only last packet won't be full size (msgSize)
            platforms::set_packet_length(pkt, hdrSize + payloadBytes);
            is_complete = true;
        }
    }

    // Copy message payload using std::span
    std::memcpy(pkt.msg.begin(), buf.data() + privSize + size, payloadBytes);

    return true;
}

bool Composer::construct_mctp_header(Packet&                          pkt,
                                     platforms::Packet::LengthType    bytes,
                                     bool                             is_response,
                                     uint8_t                          dst_eid,
                                     platforms::Packet::InterfaceType client,
                                     uint8_t                          msg_tag,
                                     uint8_t                          hdr_ver)
{
    platforms::set_packet_length(pkt, bytes);
    platforms::set_packet_interface(pkt, client);
    pkt.hdr.hdr_ver   = hdr_ver;
    pkt.hdr.rsvd      = 0;
    pkt.hdr.dst_eid   = dst_eid;
    pkt.hdr.msg_tag   = msg_tag;
    pkt.hdr.tag_owner = is_response ? 0 : 1;
    pkt.hdr.pkt_seq   = 0;
    pkt.hdr.eom       = 1;
    pkt.hdr.som       = 1;

    // src_eid will filled in mctp stack
    return true;
}
}  // namespace pdk::mctp::app
