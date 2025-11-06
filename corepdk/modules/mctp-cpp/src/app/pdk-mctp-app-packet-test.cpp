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
#include "pdk-mctp-app-packet-plat.h"
#include "ubs/unittest.hpp"

using namespace ubs::unittest;
using namespace pdk::mctp;

UBS_TEST(MctpPacket, SizeTest)
{
    app::Packet pkt{};
    ensure::is_eq(sizeof(pkt.hdr), 4);
    ensure::is_eq(sizeof(pkt.msg), platforms::TransmitUnit);
    if (platforms::PrivHeaderSize != 0) {
        ensure::is_eq(sizeof(pkt.priv), platforms::PrivHeaderSize);
        ensure::is_eq(sizeof(pkt), platforms::PrivHeaderSize + app::PktBufDataLen);
    }
};

UBS_TEST(MctpPacket, SetAndGetPacketInterface)
{
    app::Packet pkt{};
    ensure::is_eq(platforms::get_packet_interface(pkt), 0);
    platforms::set_packet_interface(pkt, 1);
    ensure::is_eq(platforms::get_packet_interface(pkt), 1);

    platforms::set_packet_interface(pkt.priv, 1);
    ensure::is_eq(platforms::get_packet_interface(pkt.priv), 1);
};

UBS_TEST(MctpPacket, SetAndGetPacketLength)
{
    app::Packet pkt{};
    ensure::is_eq(platforms::get_packet_length(pkt), 0);
    platforms::set_packet_length(pkt, 68);
    ensure::is_eq(platforms::get_packet_length(pkt), 68);

    platforms::set_packet_length(pkt.priv, 70);
    ensure::is_eq(platforms::get_packet_length(pkt.priv), 70);
};

UBS_TEST(MctpPacket, PacketConversion)
{
    app::Packet pkt{};
    platforms::set_packet_length(pkt, 68);
    platforms::set_packet_interface(pkt, 1);
    auto        pkt_array   = pkt.to_span();
    app::Packet compare_pkt = app::Packet::from(pkt_array);
    ensure::is_eq(platforms::get_packet_length(compare_pkt), 68);
    ensure::is_eq(platforms::get_packet_interface(compare_pkt), 1);
    ensure::is_eq(pkt_array.size(), app::privSize + app::hdrSize + app::msgSize);
};