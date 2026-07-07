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

#include "app/pdk-mctp-app-enums.h"
#include "app/pdk-mctp-app-packet.h"
#include "pdk-mctp-platforms-config.h"
#include "pdk-mctp-platforms-enums.h"
#include "pdk-mctp-platforms-nsm-packet.h"
#include "pdk-mctp-platforms-router.h"

namespace pdk::mctp::app {

constexpr uint8_t EndpointTypeShift = 4;

class Control
{
public:
    struct [[gnu::packed]] PktReq : TransportHeader
    {
        MsgType msg_type : 8;

        uint8_t instance_id : 5;
        uint8_t rsvd0       : 1;
        uint8_t d           : 1;
        uint8_t rq          : 1;

        Cmd     command_code;
        uint8_t data[1];

        static auto&       from(Packet& buf) { return *std::bit_cast<PktReq*>(&buf.hdr); }
        static const auto& from(const Packet& buf)
        {
            return *std::bit_cast<const PktReq*>(&buf.hdr);
        }
    };

    struct [[gnu::packed]] PktRes : TransportHeader
    {
        MsgType msg_type : 8;

        uint8_t instance_id : 5;
        uint8_t rsvd0       : 1;
        uint8_t d           : 1;
        uint8_t rq          : 1;

        Cmd              command_code;
        platforms::Ccode completion_code;
        uint8_t          data[1];

        static PktRes&       from(Packet& buf) { return *std::bit_cast<PktRes*>(&buf.hdr); }
        static const PktRes& from(const Packet& buf)
        {
            return *std::bit_cast<const PktRes*>(&buf.hdr);
        }
    };

    Control() = default;

    bool remove_routing_entry(uint8_t index);
    bool add_routing_entry(uint8_t index);

    platforms::Interface get_routing_client(uint8_t index);
    static uint8_t       get_routing_index(platforms::Interface client);

    // To support 4 additional eid for routing info update command
    using RoutingMap = std::array<platforms::ShardRoutingTable, platforms::RoutingTableSize>;

    inline RoutingMap&       routing_map() { return _routing_map; }
    inline const RoutingMap& routing_map() const { return _routing_map; }
    inline const uint8_t&    get_start_eid() const { return _start_eid; }
    inline const uint8_t&    get_allocate_src_eid() const { return _allocate_src_eid; }
    inline const uint8_t&    get_us_i2c_eid() const { return _us_i2c_eid; }
    inline const uint8_t&    get_additional_eid_count() const { return _additional_eid_count; }
    inline const uint8_t&    get_additional_address(uint8_t index) const
    {
        return _additional_address.at(index);
    }
    inline const bool& is_enumerated() const { return _is_enumerated; }

    void fill_packet_header(const Packet& rx, Packet& tx) const;
    void fill_control_msg_header(const Packet& rx, Packet& tx) const;
    void fill_error_packet(platforms::Ccode code, const Packet& rx, Packet& tx) const;
    void on_gen_set_endpoint_id(Packet& tx, uint8_t eid, SetEndpoint sub_code) const;
    void on_gen_notify_discovery(Packet& tx) const;

    inline platforms::RoutingTable&       router() { return _router; }
    inline const platforms::RoutingTable& router() const { return _router; }

    void update_eid(Packet& tx, uint8_t client) const;
    void update_eid(platforms::NsmPacket& tx, uint8_t client) const;

    static PacketType get_packet_type(const Packet& buf);

    constexpr static uint8_t HeaderSizeResponse = 4;
    constexpr static uint8_t HeaderSizeRequest  = 3;

    constexpr static uint8_t SetEidReqSize    = 0x09;
    constexpr static uint8_t NotifyDisReqSize = 0x07;

protected:
    constexpr static uint8_t EidPoolSize        = platforms::EidPoolSize;
    constexpr static uint8_t RoutingEntryNum    = 1;
    constexpr static uint8_t EidSize            = 1;
    constexpr static uint8_t NoNextEntry        = 0xFF;
    constexpr static uint8_t PhyAddrSize        = 0x1;
    constexpr static uint8_t PortNum            = 0x0;
    constexpr static uint8_t InvalidPortNum     = 0xFF;
    constexpr static uint8_t OneByteMask        = 0xFF;
    constexpr static uint8_t VendorIdFormatPci  = 0x00;
    constexpr static uint8_t VendorIdFormatIana = 0x01;
    constexpr static uint8_t VendorIanaIdByte0  = 0x00;
    constexpr static uint8_t VendorIanaIdByte1  = 0x00;
    constexpr static uint8_t VendorIanaIdByte2  = 0x16;
    constexpr static uint8_t VendorIanaIdByte3  = 0x47;
    constexpr static uint8_t VendorPciIdByte0   = 0x10;
    constexpr static uint8_t VendorPciIdByte1   = 0xDE;
    constexpr static uint8_t NoMoreCapability   = 0xFF;

    void on_get_endpoint_id(const Packet& rx, Packet& tx) const;
    void on_get_endpoint_uuid(const Packet& rx, Packet& tx) const;
    void on_get_mctp_version_support(const Packet& rx, Packet& tx) const;
    void on_get_msg_type_support(const Packet& rx, Packet& tx) const;
    void on_get_vendor_msg_support(const Packet& rx, Packet& tx) const;
    void on_allocate_endpoint_id(const Packet& rx, Packet& tx);
    void on_routing_info_update(const Packet& rx, Packet& tx);
    void on_prepare_endpoint_discovery(const Packet& rx, Packet& tx) const;
    void on_endpoint_discovery(const Packet& rx, Packet& tx) const;
    void on_discovery_notify(const Packet& rx, Packet& tx) const;

    uint8_t _allocate_eid_bus = static_cast<uint8_t>(platforms::Interface::End);
    uint8_t _num_enumerate_eid{};
    uint8_t _start_eid{};
    uint8_t _allocate_src_eid{};
    uint8_t _us_i2c_eid{};
    uint8_t _additional_eid_count{};
    std::array<uint8_t, platforms::RoutingInfoUpdateSize> _additional_address{};
    bool                                                  _is_enumerated{};

    platforms::RoutingTable _router{};
    RoutingMap              _routing_map{};
};
}  // namespace pdk::mctp::app
