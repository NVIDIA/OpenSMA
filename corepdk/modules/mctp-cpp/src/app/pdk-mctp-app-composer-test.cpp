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
#include <cstring>

#include "pdk-mctp-app-composer.h"
#include "pdk-mctp-app-packet-plat.h"
#include "pdk-mctp-platforms-enums.h"
#include "ubs/unittest.hpp"

using namespace ubs::unittest;
using namespace pdk::mctp::app;

const size_t test_max_size = 220;
Composer     _composer;

void compare_two_packet_hdr(Packet& pkt1, Packet& pkt2)
{
    ensure::is_eq(static_cast<uint8_t>(pkt1.hdr.hdr_ver),
                  static_cast<uint8_t>(pkt2.hdr.hdr_ver));
    ensure::is_eq(static_cast<uint8_t>(pkt1.hdr.rsvd), static_cast<uint8_t>(pkt2.hdr.rsvd));
    ensure::is_eq(pkt1.hdr.dst_eid, pkt2.hdr.dst_eid);
    ensure::is_eq(pkt1.hdr.src_eid, pkt2.hdr.src_eid);
    ensure::is_eq(static_cast<uint8_t>(pkt1.hdr.msg_tag),
                  static_cast<uint8_t>(pkt2.hdr.msg_tag));
    ensure::is_eq(static_cast<uint8_t>(pkt1.hdr.tag_owner),
                  static_cast<uint8_t>(pkt2.hdr.tag_owner));
}

// size contain header & msg payload (exclude priv header)
void test_gen_multi_packet(size_t msg_size)
{
    size_t                        size     = msg_size + hdrSize;
    const size_t                  max_size = 1000;
    std::array<uint8_t, max_size> test_arr = {};
    // gen_arr should not use over test_max_size
    std::array<uint8_t, test_max_size> gen_arr    = {};
    uint32_t                           gen_offset = privSize + hdrSize;

    Packet full_pkt{};
    _composer.construct_mctp_header(full_pkt, size, false, 0x32, 0, 0, 1);

    for (size_t i = 0; i < (size + privSize); ++i) {
        test_arr.at(i) = i % UINT8_MAX;
    }

    memcpy(test_arr.begin(), &full_pkt, privSize + hdrSize);
    memcpy(gen_arr.begin(), &full_pkt, privSize + hdrSize);

    uint32_t loop = 1;
    // Decide how many packet should generate
    if (size > static_cast<uint32_t>(PktBufDataLen)) {
        loop += (size - static_cast<uint32_t>(PktBufDataLen) - 1) / sizeof(msgSize) + 1;
    }

    Packet gen_single_pkt{};
    bool   gen_multi_status = false;
    // Generate packet for "loop" time
    for (uint32_t index = 0; index < loop; ++index) {
        bool is_complete = false;
        gen_multi_status = _composer.gen_packet(
            gen_single_pkt, size, index, is_complete, test_arr);
        // Compare TransportHeader
        ensure::is_eq(gen_multi_status, true);
        compare_two_packet_hdr(full_pkt, gen_single_pkt);
        ensure::is_eq(static_cast<uint8_t>(gen_single_pkt.hdr.pkt_seq), (index & 0b11));
        ensure::is_eq(static_cast<uint8_t>(gen_single_pkt.hdr.som), (index == 0));
        ensure::is_eq(static_cast<uint8_t>(gen_single_pkt.hdr.eom), (index == loop - 1));

        // Copy msg from gen_single_pkt to gen_arr for checking
        memcpy(gen_arr.begin() + gen_offset,
               gen_single_pkt.msg.begin(),
               pdk::mctp::platforms::get_packet_length(gen_single_pkt) - hdrSize);
        gen_offset += pdk::mctp::platforms::get_packet_length(gen_single_pkt) - hdrSize;

        // If last packet -> end for loop
        if (is_complete == true) {
            break;
        }
    }
    auto ret = std::mismatch(
                   gen_arr.begin(), gen_arr.begin() + size + privSize, test_arr.begin())
                   .first
            == gen_arr.begin() + size + privSize;
    ensure::is_eq(ret, true);
}

/* Construct packet with pkt.msg = [0, 1, 2, 3 ......62, 63] */
void construct_packet(Packet& pkt, uint8_t som, uint8_t eom, uint32_t msg_size)
{
    if (msg_size > msgSize) {
        msg_size = msgSize;
    }
    _composer.construct_mctp_header(pkt, msg_size + hdrSize, false, 0x32, 0, 0, 1);
    pkt.hdr.som = som;
    pkt.hdr.eom = eom;
    for (size_t i = 0; i < msg_size; ++i) {
        pkt.msg.at(i) = i % UINT8_MAX;
    }
}

void test_recv_multi_packet(uint32_t msg_size)
{
    std::array<uint8_t, test_max_size> test_arr = {};
    std::array<uint8_t, test_max_size> recv_arr = {};
    // test_arr will be [(priv) (hdr) (0, 1, 2, 3 ... 63) , (0, 1, 2, 3 ... 63) ,
    // (0, 1, 2, 3 ... 63) ....]
    for (size_t i = 0; i < msg_size; i += msgSize) {
        for (size_t j = 0; j < msgSize; ++j) {
            test_arr[i + j + privSize + hdrSize] = j % UINT8_MAX;
        }
    }

    Packet                                   pkt               = {};
    pdk::mctp::platforms::Packet::LengthType full_pkt_size     = 0;
    bool                                     recv_multi_status = false;
    for (uint32_t cur_msg_size = 0, id = 0; cur_msg_size <= msg_size;
         cur_msg_size += msgSize, id++) {
        // construct single packet used for recv_multi_packet
        construct_packet(
            pkt, id == 0, cur_msg_size + msgSize >= msg_size, msg_size - cur_msg_size);
        recv_multi_status = _composer.recv_packet(full_pkt_size, pkt, recv_arr);
        if (recv_multi_status == true) {
            if (full_pkt_size > privSize) {
                full_pkt_size = full_pkt_size - privSize;
            }
        }
    }

    if (msg_size > test_max_size - privSize - hdrSize) {
        return;
    }

    size_t start_offset = privSize + hdrSize;
    size_t end_offset   = privSize + hdrSize + msg_size;

    auto ret = std::mismatch(recv_arr.begin() + start_offset,
                             recv_arr.begin() + end_offset,
                             test_arr.begin() + start_offset)
                   .first
            == recv_arr.begin() + end_offset;
    ensure::is_eq(ret, true);

    ensure::is_eq(full_pkt_size, msg_size + hdrSize);
}

void recv_first_packet_of_multi()
{
    std::array<uint8_t, test_max_size>       temp_arr          = {};
    Packet                                   first_pkt         = {};
    pdk::mctp::platforms::Packet::LengthType full_pkt_size     = 0;
    bool                                     recv_multi_status = false;

    construct_packet(first_pkt, 1, 0, msgSize);

    recv_multi_status = _composer.recv_packet(full_pkt_size, first_pkt, temp_arr);
    ensure::is_eq(recv_multi_status,
                  false);             // indicate multi pkt not end -> false
    ensure::is_eq(full_pkt_size, 0);  // indicate multi pkt not end -> 0
}

UBS_TEST(MctpComposer, ConstructMctpHeader)
{
    Packet   pkt{};
    uint16_t test_len         = 300;
    auto     test_interface   = pdk::mctp::platforms::Interface::Pldm;
    uint8_t  test_hdr_ver     = 1;
    uint8_t  test_dst_eid     = 50;
    uint8_t  test_msg_tag     = 5;
    bool     test_is_response = true;

    _composer.construct_mctp_header(pkt,
                                    test_len,
                                    test_is_response,
                                    test_dst_eid,
                                    static_cast<uint8_t>(test_interface),
                                    test_msg_tag,
                                    test_hdr_ver);

    ensure::is_eq(pdk::mctp::platforms::get_packet_length(pkt), test_len);
    ensure::is_eq(pdk::mctp::platforms::get_packet_interface(pkt),
                  static_cast<uint8_t>(test_interface));
    ensure::is_eq(static_cast<uint8_t>(pkt.hdr.hdr_ver), test_hdr_ver);
    ensure::is_eq(static_cast<uint8_t>(pkt.hdr.rsvd), 0);
    ensure::is_eq(pkt.hdr.dst_eid, test_dst_eid);
    // src_eid not set in construct_mctp_header
    ensure::is_eq(pkt.hdr.src_eid, 0);
    ensure::is_eq(static_cast<uint8_t>(pkt.hdr.msg_tag), test_msg_tag);
    ensure::is_eq(static_cast<uint8_t>(pkt.hdr.tag_owner), (test_is_response == false));
    ensure::is_eq(static_cast<uint8_t>(pkt.hdr.pkt_seq), 0);
    ensure::is_eq(static_cast<uint8_t>(pkt.hdr.eom), 1);
    ensure::is_eq(static_cast<uint8_t>(pkt.hdr.som), 1);
};

UBS_TEST(MctpComposer, GenMultiPacket)
{
    for (uint16_t i = msgSize - 1; i <= msgSize + 1; ++i) {
        test_gen_multi_packet(i);
    }
};

UBS_TEST(MctpComposer, RecvMultiPacket)
{
    for (uint16_t i = msgSize - 1; i <= msgSize + 1; ++i) {
        test_recv_multi_packet(i);
    }
    for (uint16_t i = test_max_size - 1; i <= test_max_size + 1; ++i) {
        test_recv_multi_packet(i);
    }
};

UBS_TEST(MctpComposer, RecvMultiPacketWhenPrevNotEnd)
{
    _composer.clear();
    recv_first_packet_of_multi();
    test_recv_multi_packet(msgSize * 2 + 5);
};

UBS_TEST(MctpComposer, RecvMultiPacketOverflow)
{
    test_recv_multi_packet(test_max_size + msgSize + 1);
};