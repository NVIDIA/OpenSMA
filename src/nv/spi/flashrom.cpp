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
#include "nv/lstp/lstp_router.h"
#include "sys/ipc/supervisor.h"
#include "nv/ctimer/ctimer.h"
#include "nv/mctp/driver.h"
#include "nv/usb/task.h"
#include "nv/logger/log.h"

#include <cstdint>
#include <cstring>
#include <limits>

using namespace nv::spi;

Flashrom::Flashrom(sys::spi::EdmaDriver& driver)
: _driver(driver)
, _recipient_task_id(nv::ipc::TaskId::End)
{}

void Flashrom::bind(nv::spi::Flexcomm flexcomm,
                    nv::ipc::TaskId   recipient_task_id,
                    uint8_t           cs0_port_id,
                    uint8_t           cs0_pin_id,
                    uint8_t           cs1_port_id,
                    uint8_t           cs1_pin_id)
{
    _cs0_port_id = cs0_port_id;
    _cs0_pin_id  = cs0_pin_id;
    _cs1_port_id = cs1_port_id;
    _cs1_pin_id  = cs1_pin_id;

    _recipient_task_id = recipient_task_id;
    _driver.bind(flexcomm);
}

void Flashrom::init()
{
    _driver.init();

    if constexpr (nv::lstp::EnableSpi) {
        constexpr auto spi_ch  = nv::lstp::GetFirstChannelId(nv::lstp::LstpChannels,
                                                            nv::lstp::LstpChannelType::Spi);
        const auto&    spi_cfg = std::get<nv::lstp::LstpSpiChannelConfig>(
            nv::lstp::LstpChannels.at(spi_ch).config);
        _num_chip_selects = spi_cfg.channel_num_cs;
        _spi_speed        = spi_cfg.freq_hz;
    }
}

void Flashrom::handle_tx(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& msg)
{
    auto           msgHdr  = FlashromMsgHdr_from(msg);
    auto           msgData = FlashromMsgData_from(msg);
    const uint16_t msg_len = msgHdr->len_lsb | msgHdr->len_msb << 8;
    const uint8_t  command = msgHdr->cmdCode & CMD_CODE_MASK;

    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> rx_data{};
    const std::span<uint8_t> sbuf(msg.data() + SPI_HEADER_LEN, SPI_MAX_DATA_LEN);
    const std::span<uint8_t> rbuf(rx_data.data() + SPI_HEADER_LEN, SPI_MAX_DATA_LEN);

    if (command > FlashromCmdCode::SPI_CMD_POSTED_WRITE) {
        std::span<uint8_t> request(msg.data(), msg.size());
        nv::lstp::LstpRouter::send_error(request, nv::lstp::LstpStatus::NotSupported);
        return;
    }

    const uint8_t chip_select_index = (msgHdr->cmdCode & SPI_CS_MASK) >> SPI_CS_SHIFT;
    if (chip_select_index >= _num_chip_selects) {
        std::span<uint8_t> request(msg.data(), msg.size());
        nv::lstp::LstpRouter::send_error(request, nv::lstp::LstpStatus::NotSupported);
        return;
    }

    // embedded cs ctrl cmd
    // assert CS
    if (msgHdr->cmdCode & SPI_CS_ASSERT) {
        /* To avoid CS state desync with driver:
         *     1. If command asserts CS pin thats already asserted, toggles that pin
         *     2. Only allows one CS pin to be asserted at a time
         */
        deassert_all_cs();
        if (chip_select_index == 0) {
            nv::gpio::Driver::write(_cs0_port_id, _cs0_pin_id, 0);
        }
        else {
            nv::gpio::Driver::write(_cs1_port_id, _cs1_pin_id, 0);
        }
    }

    if (command == FlashromCmdCode::SPI_CMD_CONFIG) {
        deassert_all_cs();

        auto rxHdr       = FlashromMsgHdr_from(rx_data);
        rxHdr->channelId = msgHdr->channelId;
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
    else if (command == FlashromCmdCode::SPI_CMD_WRITE) {
        const uint32_t tx_len = msg_len;

        // SPI_INVALID_DATA: corrupt TX bytes before sending
        if (_error_injection_enabled && _error_injection_bus == 0
            && _error_injection_type == ErrorInjectionType::SPI_INVALID_DATA) {
            corrupt_spi_data(const_cast<uint8_t*>(sbuf.data()), tx_len);
        }

        const auto spi_status = _driver.sendRecv(
            _recipient_task_id, tx_len, sbuf, tx_len, rbuf);

        // SPI_TIMEOUT: clear RX buffer so caller sees all-zeroes (no valid response)
        if (_error_injection_enabled && _error_injection_bus == 0
            && _error_injection_type == ErrorInjectionType::SPI_TIMEOUT) {
            std::memset(const_cast<uint8_t*>(rbuf.data()), 0, rbuf.size());
        }

        auto rxHdr = FlashromMsgHdr_from(rx_data);

        // send back response
        rxHdr->channelId = msgHdr->channelId;
        rxHdr->cmdCode   = static_cast<uint8_t>(nv::lstp::LstpStatus_from(spi_status))
                       | nv::lstp::LstpResponseBit;
        rxHdr->len_lsb = 1;
        rxHdr->len_msb = 0;
        rbuf[0]        = 0;

        std::span<uint8_t> item(rx_data.data(), rx_data.size());

        // USB_QUEUE_FULL: skip sending the response back over USB
        if (_error_injection_enabled && _error_injection_bus == 0
            && _error_injection_type == ErrorInjectionType::USB_QUEUE_FULL) {
            return;
        }

        nv::usb::Task::to_usbLstp(item);
    }
    else if (command == FlashromCmdCode::SPI_CMD_READ) {
        if (msg_len < sizeof(nv::lstp::LstpSpiReadRequest)) {
            deassert_all_cs();
            std::span<uint8_t> request(msg.data(), msg.size());
            nv::lstp::LstpRouter::send_error(request, nv::lstp::LstpStatus::Error);
            return;
        }

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

            const auto spi_status = _driver.sendRecv(
                _recipient_task_id, recv_len, sbuf, recv_len, rbuf);

            // Safety check to prevent unsigned wrap (should never happen due to loop logic)
            // coverity[cert_int30_c_violation : FALSE]
            if (recv_len > (std::numeric_limits<uint32_t>::max() - ptr)) {
                break;  // Prevent overflow
            }
            ptr              += recv_len;
            auto rxHdr        = FlashromMsgHdr_from(rx_data);
            rxHdr->channelId  = msgHdr->channelId;
            rxHdr->cmdCode    = static_cast<uint8_t>(nv::lstp::LstpStatus_from(spi_status))
                           | nv::lstp::LstpResponseBit;
            rxHdr->len_lsb = recv_len & LSB_MASK;
            rxHdr->len_msb = (recv_len & MSB_MASK) >> 8;
            std::span<uint8_t> item(rx_data.data(), rx_data.size());

            auto res = nv::usb::Task::to_usbLstp(item);
            if (res != usb::Status::Ok) {
                deassert_all_cs();
                return;
            }
        }
    }

    else if (command == FlashromCmdCode::SPI_CMD_WRITE_READ) {
        // SPI_INVALID_DATA: corrupt TX bytes before sending
        if (_error_injection_enabled && _error_injection_bus == 0
            && _error_injection_type == ErrorInjectionType::SPI_INVALID_DATA) {
            corrupt_spi_data(const_cast<uint8_t*>(sbuf.data()), msg_len);
        }

        const auto spi_status = _driver.sendRecv(
            _recipient_task_id, msg_len, sbuf, msg_len, rbuf);

        // SPI_TIMEOUT: clear RX buffer so caller sees all-zeroes (no valid response)
        if (_error_injection_enabled && _error_injection_bus == 0
            && _error_injection_type == ErrorInjectionType::SPI_TIMEOUT) {
            std::memset(const_cast<uint8_t*>(rbuf.data()), 0, rbuf.size());
        }

        auto rxHdr       = FlashromMsgHdr_from(rx_data);
        rxHdr->channelId = msgHdr->channelId;
        rxHdr->cmdCode   = static_cast<uint8_t>(nv::lstp::LstpStatus_from(spi_status))
                       | nv::lstp::LstpResponseBit;
        rxHdr->len_lsb = msgHdr->len_lsb;
        rxHdr->len_msb = msgHdr->len_msb;
        std::span<uint8_t> item(rx_data.data(), rx_data.size());

        // USB_QUEUE_FULL: skip sending the response back over USB
        if (_error_injection_enabled && _error_injection_bus == 0
            && _error_injection_type == ErrorInjectionType::USB_QUEUE_FULL) {
            return;
        }

        nv::usb::Task::to_usbLstp(item);
    }
    else if (command == FlashromCmdCode::SPI_CMD_POSTED_WRITE) {
        _driver.sendRecv(_recipient_task_id, msg_len, sbuf, msg_len, rbuf);
    }

    // embedded cs ctrl cmd
    // de-assert CS
    if (msgHdr->cmdCode & SPI_CS_DEASSERT) {
        if (chip_select_index == 0) {
            nv::gpio::Driver::write(_cs0_port_id, _cs0_pin_id, 1);
        }
        else {
            nv::gpio::Driver::write(_cs1_port_id, _cs1_pin_id, 1);
        }
    }
}

void Flashrom::deassert_all_cs()
{
    nv::gpio::Driver::write(_cs0_port_id, _cs0_pin_id, 1);
    nv::gpio::Driver::write(_cs1_port_id, _cs1_pin_id, 1);
}
void Flashrom::set_error_injection(bool enabled, uint8_t bus, uint8_t type)
{
    _error_injection_enabled = enabled;
    _error_injection_bus     = bus;
    _error_injection_type    = type;
    _error_injection_counter = 0;

    nv::info("Flashrom: Error injection %s: bus=%d, type=%d\n",
             enabled ? "enabled" : "disabled",
             bus,
             type);
}

void Flashrom::clear_error_injection()
{
    nv::info("Flashrom: Clearing error injection, injected %d errors\n",
             _error_injection_counter);

    _error_injection_enabled = false;
    _error_injection_bus     = 0;
    _error_injection_type    = 0;
    _error_injection_counter = 0;
}

void Flashrom::corrupt_spi_data(uint8_t* data, size_t length)
{
    if (!data || length == 0) {
        return;
    }

    constexpr uint8_t corruption_pattern = 0xAA;
    const size_t      bytes_to_corrupt   = std::min(length, size_t(4));

    for (size_t i = 0; i < bytes_to_corrupt; ++i) {
        data[i] ^= corruption_pattern;
    }

    nv::info("Flashrom: Corrupted %zu bytes with pattern 0x%02X (total=%lu)\n",
             bytes_to_corrupt,
             corruption_pattern,
             ++_error_injection_counter);
}
