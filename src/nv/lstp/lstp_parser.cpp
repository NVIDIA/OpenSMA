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
        if ((hdr.channel_id == 0)
            && (hdr.cmd_status_code & LstpSpiCmdMask)
                   <= static_cast<uint8_t>(LstpSpiCommand::End)) {
            channel_type = LstpChannelType::Spi;
        }
    }

    return channel_type;
}

}  // namespace nv::lstp
