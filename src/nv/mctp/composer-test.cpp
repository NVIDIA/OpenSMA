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
#include "nv/mctp/composer.h"

#include <cstring>

#include "nv/mctp/driver.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/interface.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;

constexpr static uint32_t MctpPrvHeaderSize = sizeof(mctp::PrivateHeader);
constexpr static uint32_t MctpHeaderSize    = sizeof(mctp::Header);
constexpr static uint32_t TypeHeaderSize    = 4;
constexpr static uint32_t PayloadSize       = 256;
constexpr static uint32_t BufferSize        = MctpPrvHeaderSize + MctpHeaderSize + PayloadSize;
using Buffer                                = std::array<uint8_t, BufferSize>;
static mctp::Packet& from(Buffer& arr)
{
    return *std::bit_cast<mctp::Packet*>(&arr[0]);
}

TEST(MctpComposer, ReceiveMulti)
{
    using namespace mctp;

    Composer composer{};
    composer.clear();

    Buffer tx_buf{};
    Buffer rx_buf{};

    auto& tx = from(tx_buf);

    // generate large tx packet
    composer.construct_mctp_header(tx, MctpHeaderSize + PayloadSize, true, 0x18, 1, 0, 1);
    // initial payload from 0 to 255
    for (auto i = 0; i < 256; i++) {
        tx_buf[MctpPrvHeaderSize + MctpHeaderSize + i] = i;
    }

    // generate multi-packets and receive
    auto size = tx.priv.packet_length;

    uint16_t pkt_size{};
    for (uint32_t i = 0; i < 100; i++) {
        bool   is_complete{};
        Packet pkt{};

        composer.gen_packet(pkt, size, i, is_complete, tx_buf);

        if (!composer.recv_packet(pkt_size, pkt, rx_buf)) {
            continue;
        }
        else {
            ensure::is_eq(1, is_complete);
            break;
        }
    }

    /* compare payload */
    bool is_same = true;
    for (uint32_t i = MctpHeaderSize + MctpPrvHeaderSize;
         i < MctpHeaderSize + MctpPrvHeaderSize + PayloadSize;
         i++) {
        if (tx_buf[i] != rx_buf[i]) {
            nv::info("i %d tx_buf 0x%x rx_buf 0x%x\n", i, tx_buf[i], rx_buf[i]);
            is_same = false;
            break;
        }
    }
    ensure::is_eq(true, is_same);
};
