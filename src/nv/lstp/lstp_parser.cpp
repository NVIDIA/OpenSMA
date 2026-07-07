/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include NV_IPC_CONFIG_H

#include "nv/lstp/lstp_parser.h"

namespace nv::lstp {

LstpStatus LstpParser::validate_request(std::span<uint8_t>& req_buffer, size_t req_size)
{
    if (req_size > LstpMsgSize) {
        return LstpStatus::SilentDrop;
    }

    if (req_size < sizeof(LstpHdr)) {
        return LstpStatus::SilentDrop;
    }

    auto& hdr = from<LstpHdr>(req_buffer.data());

    const size_t pkt_size = (static_cast<size_t>(hdr.len_lsb)
                             | (static_cast<size_t>(hdr.len_msb) << BYTE1_SHIFT))
                          + sizeof(LstpHdr);
    if (pkt_size > LstpMsgSize) {
        return LstpStatus::NotSupported;
    }
    else if (pkt_size > req_size) {
        return LstpStatus::Error;
    }

    if (hdr.channel_id >= LstpNumChannels) {
        return LstpStatus::SilentDrop;
    }

    return LstpStatus::Success;
}

LstpChannelType LstpParser::parse_channel_type(std::span<uint8_t>& req_buffer)
{
    auto& hdr = from<LstpHdr>(req_buffer.data());

    // Flashrom (NV_SMA_SPI) may use channel ID = 0 in the request packet.
    // Preserve that alias before routing by configured channel type.
    auto channel_type = LstpChannels.at(hdr.channel_id).info.type;
    if constexpr (EnableSpi) {
        static_assert(static_cast<uint8_t>(LstpManagementCommand::Start)
                      > static_cast<uint8_t>(LstpSpiCommand::End));
        static_assert(static_cast<uint8_t>(LstpManagementCommand::End) <= LstpSpiCmdMask);
        constexpr uint8_t kFirstSpiChannel = GetFirstChannelId(LstpChannels,
                                                               LstpChannelType::Spi);
        if constexpr (kFirstSpiChannel < LstpNumChannels) {
            if ((hdr.channel_id == 0)
                && (hdr.cmd_status_code & LstpSpiCmdMask)
                       <= static_cast<uint8_t>(LstpSpiCommand::End)) {
                hdr.channel_id = kFirstSpiChannel;
                channel_type   = LstpChannelType::Spi;
            }
        }
    }

    return channel_type;
}

std::expected<LstpParser::SpiRequest, LstpStatus>
LstpParser::parse_spi_request(std::span<uint8_t>& req_buffer)
{
    auto&        req_hdr    = from<LstpHdr>(req_buffer.data());
    const size_t req_offset = sizeof(LstpHdr);

    const uint16_t payload_len = req_hdr.len_lsb | (req_hdr.len_msb << BYTE1_SHIFT);
    // TODO: Add multipacket support
    // NOLINTNEXTLINE: expected to not work if EnableLstp is false
    if (payload_len > LstpMaxPayloadSize) {
        return std::unexpected(LstpStatus::Error);
    }

    SpiRequest req{
        .channel_id  = req_hdr.channel_id,
        .cs          = (req_hdr.cmd_status_code & LstpSpiCommandFlags::CsSelect)
                         ? nv::spi::bm::CsPins::Cs1
                         : nv::spi::bm::CsPins::Cs0,
        .read_length = 0,
        .write_data  = {},
        .flags       = nv::spi::bm::NoFlag,
    };

    auto cmd = static_cast<LstpSpiCommand>(req_hdr.cmd_status_code & LstpSpiCmdMask);

    switch (cmd) {
        case LstpSpiCommand::Read: {
            if (req_offset + sizeof(LstpSpiReadRequest) > LstpMaxPayloadSize) {
                return std::unexpected(LstpStatus::Error);
            }
            if (payload_len != sizeof(LstpSpiReadRequest)) {
                return std::unexpected(LstpStatus::Error);
            }
            auto& read_req = from<LstpSpiReadRequest>(&req_buffer.data()[req_offset]);

            if (read_req.read_len > LstpMaxPayloadSize) {
                return std::unexpected(LstpStatus::Error);
            }

            req.read_length = read_req.read_len;
            break;
        }

        case LstpSpiCommand::Write: {
            req.write_data = std::span<uint8_t>(&req_buffer.data()[req_offset], payload_len);
            break;
        }
        case LstpSpiCommand::WriteRead: {
            req.read_length = static_cast<uint16_t>(payload_len);
            req.write_data  = std::span<uint8_t>(&req_buffer.data()[req_offset], payload_len);
            break;
        }

        case LstpSpiCommand::PostedWrite:
        default                         : return std::unexpected(LstpStatus::NotSupported);
    }

    if (req_hdr.cmd_status_code & LstpSpiCommandFlags::CsAssert) {
        req.flags |= nv::spi::bm::SpiFlags::CsAssert;
    }

    if (req_hdr.cmd_status_code & LstpSpiCommandFlags::CsDeassert) {
        req.flags |= nv::spi::bm::SpiFlags::CsDeassert;
    }

    // NOLINTNEXTLINE: expected to not work if EnableLstp is false
    if (req.read_length > LstpMaxPayloadSize) {
        return std::unexpected(LstpStatus::Error);
    }

    return req;
}

}  // namespace nv::lstp
