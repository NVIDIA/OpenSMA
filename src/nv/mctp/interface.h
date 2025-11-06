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
#include <cstdint>
#include <span>

#include "nv/nv.h"
#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-packet.h"
#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-enums.h"

namespace nv::mctp {

using PrivateHeader = pdk::mctp::platforms::PrivateHeader;
using Header        = pdk::mctp::app::TransportHeader;
using Packet        = pdk::mctp::app::Packet;
using Client        = pdk::mctp::platforms::Interface;

using pdk::mctp::app::PktBufDataLen;

constexpr auto NsmPktBufDataLen = 256;

enum EventBit : uint32_t
{
    None          = 0,
    UpI2cReceive  = common::bit(Client::UsI2c),
    UpUsbReceive  = common::bit(Client::UsUsb),
    DwI2c0Receive = common::bit(Client::DsI2c0),
    DwI2c1Receive = common::bit(Client::DsI2c1),
    DwI2c2Receive = common::bit(Client::DsI2c2),
    DwI2c3Receive = common::bit(Client::DsI2c3),
    DwI3c0Receive = common::bit(Client::DsI3c0),
    DwI3c1Receive = common::bit(Client::DsI3c1),
    PldmReceive   = common::bit(Client::Pldm),
    SpdmReceive   = common::bit(Client::Spdm),
    Spdm4kReceive = common::bit(Client::Spdm4K),
    Spi0Receive   = common::bit(Client::Spi0),
    Spi1Receive   = common::bit(Client::Spi1),
    Spi2Receive   = common::bit(Client::Spi2),
};

struct [[gnu::packed]] NsmPacket
{
    PrivateHeader                                          priv;  ///< private header
    Header                                                 hdr;   ///< mctp header
    std::array<uint8_t, NsmPktBufDataLen - sizeof(Header)> msg;   ///< mctp message body

    std::span<uint8_t> to_span() const
    {
        return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
    }

    static auto& from(std::span<uint8_t> data)
    {
        nv::always_assert(data.size() >= sizeof(Packet));
        return *std::bit_cast<Packet*>(data.data());
    };
};

// --- Compile time tests -----------------------------------------------------
static_assert(sizeof(Header) == 4, "Invalid Header size");
static_assert(sizeof(PrivateHeader) == 4, "Invalid PrivateHeader size");
static_assert(sizeof(Packet) == PktBufDataLen + sizeof(PrivateHeader), "Invalid Packet size");
}  // namespace nv::mctp
