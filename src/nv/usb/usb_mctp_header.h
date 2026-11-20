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

#include <cstdint>

namespace nv::usb {

// USB MCTP Header (shared between Core0 and Core1)
// Byte 0-1: dmtf_id (0xB41A, little-endian)
// Byte 2: reserved
// Byte 3: length (including header)
struct __attribute__((packed)) UsbMctpHeader
{
    uint16_t dmtf_id;  // 0xB41A
    uint8_t  rsvb;     // Reserved
    uint8_t  length;   // Packet length (including this header)
};

// USB full-speed MCTP packet-spanning header.
// Byte 2 bits 7:5: reserved
// Byte 2 bits 4:0 and byte 3: 13-bit big-endian length (including header)
struct __attribute__((packed)) UsbMctpSpanningHeader
{
    static constexpr uint16_t MaxPacketLength = 0x1FFFU;

    uint16_t dmtf_id;
    uint8_t  length_msb;
    uint8_t  length_lsb;

    [[nodiscard]] constexpr uint16_t packet_length() const
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(length_msb & 0x1FU) << 8U)
                                     | length_lsb);
    }

    constexpr void set_packet_length(uint16_t length)
    {
        length_msb = static_cast<uint8_t>((length >> 8U) & 0x1FU);
        length_lsb = static_cast<uint8_t>(length & 0xFFU);
    }
};
static_assert(sizeof(UsbMctpSpanningHeader) == 4U);

constexpr uint16_t UsbDmtfId = 0xB41A;

}  // namespace nv::usb
