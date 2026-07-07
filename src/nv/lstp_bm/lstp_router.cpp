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
#include "nv/spi/instance_bm.h"

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

LstpStatus LstpRouter::receive_spi(std::span<uint8_t>& buffer)
{
    static_assert(nv::lstp::LstpSpiMaxCs == nv::spi::bm::SpiMaxCsPins,
                  "LstpSpiMaxCs and nv::spi::bm::SpiMaxCsPins must match");

    // LSTP allows only one outstanding request per channel, so the SPI port
    // should never be busy here. If the host violates that, silently drop the
    // packet rather than racing a Busy response against the real in-flight
    // response.
    auto&      req_hdr = from<LstpHdr>(buffer.data());
    const auto port = static_cast<nv::spi::Port>(LstpChannels.at(req_hdr.channel_id).info.id);
    if (nv::spi::bm::SpiManager::is_busy(port)) {
        return LstpStatus::SilentDrop;
    }

    auto result = LstpParser::parse_spi_request(buffer);
    if (!result) {
        return result.error();
    }

    auto&                  req        = result.value();
    nv::spi::bm::SpiStatus spi_status = nv::spi::bm::SpiManager::transfer(
        port, req.cs, req.read_length, req.write_data, req.flags);

    return LstpStatus_from(spi_status);
}

void LstpRouter::send_spi(nv::spi::Port port)
{
    uint8_t channel_id;
    for (channel_id = static_cast<uint8_t>(LstpChannelType::Management);
         channel_id < LstpNumChannels;
         channel_id++) {
        const auto& channel_info = LstpChannels.at(channel_id).info;
        if ((channel_info.type == LstpChannelType::Spi)
            && (static_cast<nv::spi::Port>(channel_info.id) == port)) {
            break;
        }
    }
    if (channel_id >= LstpNumChannels) {
        return;
    }

    nv::spi::bm::SpiResult& result   = nv::spi::bm::SpiManager::get_result(port);
    const LstpStatus        status   = LstpStatus_from(result.status);
    const uint16_t          read_len = static_cast<uint16_t>(result.read_length);

    auto& tx_buf             = g_lstp.tx_buffers[channel_id];
    auto& resp_hdr           = from<LstpHdr>(tx_buf.buffer);
    resp_hdr.channel_id      = channel_id;
    resp_hdr.cmd_status_code = static_cast<uint8_t>(status) | LstpResponseBit;

    if ((status == LstpStatus::Success) && (read_len > 0) && (read_len <= LstpMaxPayloadSize)) {
        resp_hdr.len_lsb = read_len & LSB_MASK;
        resp_hdr.len_msb = read_len >> BYTE1_SHIFT;
        tx_buf.length    = sizeof(LstpHdr) + read_len;
        memcpy(tx_buf.buffer + sizeof(LstpHdr), result.buffer.data(), read_len);
    }
    else {
        resp_hdr.len_lsb = 0;
        resp_hdr.len_msb = 0;
        tx_buf.length    = sizeof(LstpHdr);
    }

    USB_SET_LSTP_TX_PENDING(g_lstp, channel_id);
}

LstpStatus LstpRouter::receive_i2c([[maybe_unused]] std::span<uint8_t>& buffer)
{
    return LstpStatus::NotSupported;
}

}  // namespace nv::lstp::bm
