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

#include "nv/spi/flashrom.h"

#include "nv/bootloader.h"
#include "nv/nv.h"
#include NV_IPC_CONFIG_H
#include "nv/lstp/lstp_common.h"
#include "sys/ipc/supervisor.h"
#include "nv/ctimer/ctimer.h"
#include "nv/mctp/driver.h"
#include "nv/usb/task.h"
#include "nv/logger/log.h"

#include <cstdint>
#include <cstring>
#include <limits>

using namespace nv::spi;

namespace {
// SPI configuration response values
constexpr uint8_t CONFIG_RESPONSE_LEN = 0x0c;

// Bit manipulation constants
constexpr uint32_t BYTE1_SHIFT = 8;
constexpr uint32_t BYTE2_SHIFT = 16;
constexpr uint32_t BYTE3_SHIFT = 24;
constexpr uint16_t LSB_MASK    = 0xff;
constexpr uint16_t MSB_MASK    = 0xff00;
}  // namespace

Flashrom::Flashrom(sys::spi::EdmaDriver& driver) : _driver(driver) {}

void Flashrom::bind(nv::spi::Flexcomm flexcomm,
                    nv::ipc::EventId  event_id,
                    uint8_t           cs0_port_id,
                    uint8_t           cs0_pin_id,
                    uint8_t           cs1_port_id,
                    uint8_t           cs1_pin_id)
{
    _cs0_port_id = cs0_port_id;
    _cs0_pin_id  = cs0_pin_id;
    _cs1_port_id = cs1_port_id;
    _cs1_pin_id  = cs1_pin_id;
    _driver.bind(flexcomm, event_id);
}

void Flashrom::init()
{
    _driver.init();

    if constexpr (nv::lstp::EnableSpi) {
        constexpr auto spi_ch  = nv::lstp::GetFirstChannelId(nv::lstp::LstpChannels,
                                                            nv::lstp::LstpChannelType::Spi);
        const auto&    spi_cfg = std::get<nv::lstp::LstpSpiChannelConfig>(
            std::get<1>(nv::lstp::LstpChannels.at(spi_ch)));
        _spi_speed = spi_cfg.freq_hz;
    }
}

void Flashrom::handle_tx(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& msg)
{
    auto           msgHdr  = FlashromMsgHdr_from(msg);
    auto           msgData = FlashromMsgData_from(msg);
    const uint16_t msg_len = msgHdr->len_lsb | msgHdr->len_msb << 8;

    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> rx_data{};
    const std::span<uint8_t> sbuf(msg.data() + SPI_HEADER_LEN, SPI_MAX_DATA_LEN);
    const std::span<uint8_t> rbuf(rx_data.data() + SPI_HEADER_LEN, SPI_MAX_DATA_LEN);

    // spi config
    if ((msgHdr->cmdCode & CMD_CODE_MASK) == FlashromCmdCode::SPI_CMD_CONFIG) {
        // deasserted cs
        nv::gpio::Driver::write(_cs0_port_id, _cs0_pin_id, 1);
        nv::gpio::Driver::write(_cs1_port_id, _cs1_pin_id, 1);

        auto rxHdr       = FlashromMsgHdr_from(rx_data);
        rxHdr->channelId = 0x01;
        rxHdr->cmdCode   = FlashromCmdCode::SPI_CMD_SUCCESS_RSP;
        rxHdr->len_lsb   = CONFIG_RESPONSE_LEN;
        rxHdr->len_msb   = 0x00;
        rbuf[0]          = static_cast<uint8_t>(_spi_speed);
        rbuf[1]          = static_cast<uint8_t>(_spi_speed >> BYTE1_SHIFT);
        rbuf[2]          = static_cast<uint8_t>(_spi_speed >> BYTE2_SHIFT);
        rbuf[3]          = static_cast<uint8_t>(_spi_speed >> BYTE3_SHIFT);

        std::span<uint8_t> item(rx_data.data(), rx_data.size());
        nv::usb::Task::to_usbLstp(item);
        return;
    }

    // embedded cs ctrl cmd
    // assert CS
    if (msgHdr->cmdCode & SPI_CS_ASSERT) {
        if ((msgHdr->cmdCode & SPI_CS_MASK) == SPI_CS0) {
            nv::gpio::Driver::write(_cs0_port_id, _cs0_pin_id, 0);
        }
        else {
            nv::gpio::Driver::write(_cs1_port_id, _cs1_pin_id, 0);
        }
    }

    if ((msgHdr->cmdCode & CMD_CODE_MASK) == FlashromCmdCode::SPI_CMD_WRITE) {
        const uint32_t tx_len = msg_len;
        _driver.sendRecv(tx_len, sbuf, tx_len, rbuf);

        auto rxHdr = FlashromMsgHdr_from(rx_data);

        // send back response
        rxHdr->channelId = 0x01;
        rxHdr->cmdCode   = FlashromCmdCode::SPI_CMD_SUCCESS_RSP;
        rxHdr->len_lsb   = 1;
        rxHdr->len_msb   = 0;
        rbuf[0]          = 0;

        std::span<uint8_t> item(rx_data.data(), rx_data.size());
        nv::usb::Task::to_usbLstp(item);
    }

    if ((msgHdr->cmdCode & CMD_CODE_MASK) == FlashromCmdCode::SPI_CMD_READ) {
        uint32_t readcnt  = 0;
        uint32_t ptr      = 0;
        readcnt           = msgData[0];
        readcnt          |= msgData[1] << BYTE1_SHIFT;
        readcnt          |= msgData[2] << BYTE2_SHIFT;
        readcnt          |= msgData[3] << BYTE3_SHIFT;

        uint32_t recv_len = 0;
        while (ptr < readcnt) {
            recv_len = readcnt - ptr;
            if (SPI_MAX_DATA_LEN < recv_len) {
                recv_len = SPI_MAX_DATA_LEN;
            }

            _driver.sendRecv(recv_len, sbuf, recv_len, rbuf);

            // Safety check to prevent unsigned wrap (should never happen due to loop logic)
            // coverity[cert_int30_c_violation : FALSE]
            if (recv_len > (std::numeric_limits<uint32_t>::max() - ptr)) {
                break;  // Prevent overflow
            }
            ptr              += recv_len;
            auto rxHdr        = FlashromMsgHdr_from(rx_data);
            rxHdr->channelId  = 0x01;
            rxHdr->cmdCode    = FlashromCmdCode::SPI_CMD_SUCCESS_RSP;
            rxHdr->len_lsb    = recv_len & LSB_MASK;
            rxHdr->len_msb    = (recv_len & MSB_MASK) >> 8;
            std::span<uint8_t> item(rx_data.data(), rx_data.size());

            auto res = nv::usb::Task::to_usbLstp(item);
            if (res != usb::Status::Ok) {
                nv::gpio::Driver::write(_cs0_port_id, _cs0_pin_id, 1);
                nv::gpio::Driver::write(_cs1_port_id, _cs1_pin_id, 1);
                return;
            }
        }
    }

    if ((msgHdr->cmdCode & CMD_CODE_MASK) == FlashromCmdCode::SPI_CMD_WRITE_READ) {
        _driver.sendRecv(msg_len, sbuf, msg_len, rbuf);

        // send back response
        auto rxHdr       = FlashromMsgHdr_from(rx_data);
        rxHdr->channelId = 0x01;
        rxHdr->cmdCode   = FlashromCmdCode::SPI_CMD_SUCCESS_RSP;
        rxHdr->len_lsb   = msgHdr->len_lsb;
        rxHdr->len_msb   = msgHdr->len_msb;
        std::span<uint8_t> item(rx_data.data(), rx_data.size());

        nv::usb::Task::to_usbLstp(item);
    }

    if ((msgHdr->cmdCode & CMD_CODE_MASK) == FlashromCmdCode::SPI_CMD_POSTED_WRITE) {
        _driver.sendRecv(msg_len, sbuf, msg_len, rbuf);
    }

    // embedded cs ctrl cmd
    // de-assert CS
    if (msgHdr->cmdCode & SPI_CS_DEASSERT) {
        if ((msgHdr->cmdCode & SPI_CS_MASK) == SPI_CS0) {
            nv::gpio::Driver::write(_cs0_port_id, _cs0_pin_id, 1);
        }
        else {
            nv::gpio::Driver::write(_cs1_port_id, _cs1_pin_id, 1);
        }
    }
}
