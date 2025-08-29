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

#include "app/pdk-mctp-app-enums.h"
#include "app/pdk-mctp-app-packet.h"
#include "pdk-mctp-platforms-control.h"
#include "pdk-mctp-platforms-enums.h"

namespace pdk::mctp::app {

constexpr uint32_t dmtf_iana_le = 0x47160000;
constexpr uint32_t dmtf_iana_be = 0x00001647;

struct [[gnu::packed]] VendorPktReq : app::TransportHeader
{
    app::MsgType msg_type;

    uint32_t iana;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t           vendor_msg_type;
    platforms::VdmCmd command_code;
    uint8_t           msg_version;
    uint8_t           data[1];

    static VendorPktReq& from(app::Packet& buf)
    {
        return *std::bit_cast<VendorPktReq*>(&buf.hdr);
    }

    static const VendorPktReq& from(const app::Packet& buf)
    {
        return *std::bit_cast<const VendorPktReq*>(&buf.hdr);
    }
};

struct [[gnu::packed]] VendorPktRes : app::TransportHeader
{
    app::MsgType msg_type;

    uint32_t iana;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t           vendor_msg_type;
    platforms::VdmCmd command_code;
    uint8_t           msg_version;
    platforms::Ccode  completion_code;
    uint8_t           data[1];

    static VendorPktRes& from(app::Packet& buf)
    {
        return *std::bit_cast<VendorPktRes*>(&buf.hdr);
    }

    static const VendorPktRes& from(const app::Packet& buf)
    {
        return *std::bit_cast<const VendorPktRes*>(&buf.hdr);
    }
};

class Vendor
{
public:
    Vendor(platforms::Control& ctl) : _ctl(ctl) {}
    static PacketType get_vendor_packet_type(const Packet& buf);

protected:
    void unsupported_command(const Packet& rx, Packet& tx) const;
    void fill_vendor_msg_header(const Packet& rx, Packet& tx) const;
    void fill_error_packet(platforms::Ccode code, const Packet& rx, Packet& tx) const;
    void fill_packet_header(const Packet& rx, Packet& tx) const;

    constexpr static uint8_t HeaderSizeResponse = 10;

    platforms::Control& _ctl;
};
}  // namespace pdk::mctp::app
