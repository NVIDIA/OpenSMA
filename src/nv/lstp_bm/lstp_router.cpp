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
#include "nv/lstp_bm/lstp_router.h"

// USB buffers and received lengths are owned by the bare-metal USB stack.
#include "usb_buffers.h"

static auto& g_lstp = *get_usb_lstp_bufs();

namespace nv::lstp::bm {

LstpStatus LstpRouter::receive(std::span<uint8_t>& buffer)
{
    auto channel_type = LstpParser::parse_channel_type(buffer);

    LstpStatus status = LstpStatus::NotSupported;
    switch (channel_type) {
        case LstpChannelType::Spi: {
            if constexpr (EnableSpi) {
                status = receive_spi(buffer);
            }
            break;
        }
        case LstpChannelType::I2c: {
            if constexpr (EnableI2c) {
                status = receive_i2c(buffer);
            }
            break;
        }
        default: break;
    }

    return status;
}

void LstpRouter::send_error(std::span<uint8_t>& buffer, LstpStatus status)
{
    auto& req_hdr = from<LstpHdr>(buffer.data());

    // LSTP follows request-response model so if channel TX flag is set, it means host sent a
    // request without waiting for response - protocol violation, acceptable to drop
    // Exception: GPIO interrupts - GPIO channel implementation should handle queueing
    if ((status != LstpStatus::Success) && (status != LstpStatus::SilentDrop)
        && (req_hdr.channel_id < nv::lstp::LstpNumChannels)
        && !USB_IS_LSTP_TX_PENDING(g_lstp, req_hdr.channel_id)
        && (g_lstp.tx_busy_ch_id != req_hdr.channel_id)) {
        auto& tx_buf             = g_lstp.tx_buffers[req_hdr.channel_id];
        auto& resp_hdr           = from<LstpHdr>(tx_buf.buffer);
        resp_hdr.channel_id      = req_hdr.channel_id;
        resp_hdr.cmd_status_code = static_cast<uint8_t>(status) | LstpResponseBit;
        resp_hdr.len_lsb         = 0;
        resp_hdr.len_msb         = 0;
        tx_buf.length            = sizeof(LstpHdr);
        USB_SET_LSTP_TX_PENDING(g_lstp, req_hdr.channel_id);
    }
}

LstpStatus LstpRouter::receive_spi([[maybe_unused]] std::span<uint8_t>& buffer)
{
    return LstpStatus::NotSupported;
}

LstpStatus LstpRouter::receive_i2c([[maybe_unused]] std::span<uint8_t>& buffer)
{
    return LstpStatus::NotSupported;
}

}  // namespace nv::lstp::bm
