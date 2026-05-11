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

#include "nv/ssif/ssif.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <span>

#include "nv/bootloader.h"
#include "nv/nv.h"
#include "nv/gpio/driver.h"
#include "nv/i2c/helper.h"
#include "nv/logger/log.h"

using namespace nv::ssif;

uint8_t SmbusWriteBlock::calculate_pec(uint8_t address)
{
    uint8_t                  addr_byte = (address << 1);
    const std::span<uint8_t> addr_item(&addr_byte, sizeof(addr_byte));
    uint8_t                  pec = nv::i2c::crc8(addr_item);

    auto*                    raw = std::bit_cast<uint8_t*>(&cmd);
    const std::span<uint8_t> cmd_item(raw, sizeof(cmd) + sizeof(size) + size);
    pec = nv::i2c::crc8(pec, cmd_item);
    return pec;
}

uint8_t SmbusReadBlock::calculate_pec(uint8_t addr, uint8_t rx_cmd)
{
    addr <<= 1;
    std::array<uint8_t, 3> addr_cmd{addr,                             // Write phase address
                                    rx_cmd,                           // Write phase command
                                    static_cast<uint8_t>(addr | 1)};  // Read phase address

    const std::span<uint8_t> addr_cmd_item(addr_cmd);
    uint8_t                  pec = nv::i2c::crc8(addr_cmd_item);

    auto*                    raw = std::bit_cast<uint8_t*>(&size);
    const std::span<uint8_t> data_item(raw, sizeof(size) + size);
    pec = nv::i2c::crc8(pec, data_item);

    return pec;
}

void Ssif::bind(nv::i2c::Port      i2c_bus,
                nv::gpio::GpioPort alert_port_id,
                nv::gpio::GpioPin  alert_pin_id)
{
    _alert_port_id = alert_port_id;
    _alert_pin_id  = alert_pin_id;

    const std::array<uint8_t, sys::i2c::NumI2cTargetAddresses> target_addresses{BmcAddress,
                                                                                AraAddress};
    _driver.bind(i2c_bus, target_addresses, this);
}

void Ssif::start()
{
    _driver.start();
}

void Ssif::peripheral_recovery()
{
    _driver.peripheral_recovery();
}

std::span<uint8_t> Ssif::get_tx_buffer()
{
    return {_buffer.data(), std::min(sizeof(Packet), _buffer.size())};
}

bool Ssif::handle_tx()
{
    auto&        pkt  = Packet::from(_buffer);
    const size_t size = (pkt.hdr.len_msb << 8) | pkt.hdr.len_lsb;
    if (size <= sizeof(pkt.ipmi_data)) {
        _tx_size   = size;
        _tx_offset = 0;
        nv::gpio::Driver::write(_alert_port_id, _alert_pin_id, 0U);
        return true;
    }
    return false;
}

void Ssif::handle_rx()
{
    lstp::LstpRouter::send_ipmi(_buffer, _rx_size);
    _rx_size   = 0;
    _rx_offset = 0;
}

bool Ssif::i2c_ack_callback(uint8_t& address, bool is_read, void* this_ssif_instance)
{
    bool ack_nack = false;

    switch (address) {
        case BmcAddress:
            ack_nack = static_cast<Ssif*>(this_ssif_instance)->i2c_ack_bmc(is_read);
            break;
        case AraAddress:
            ack_nack = static_cast<Ssif*>(this_ssif_instance)->i2c_ack_ara(is_read);
            break;
        default: break;
    }

    return ack_nack;
}

void Ssif::i2c_callback(uint8_t&                  address,
                        bool                      is_read,
                        sys::i2c::I2cSlaveBuffer& i2c_buffer,
                        size_t&                   i2c_size,
                        void*                     this_ssif_instance,
                        bool /*new_transaction*/)
{
    if (is_read) {
        // NOLINTNEXTLINE: no way around this without function pointers
        static_cast<Ssif*>(this_ssif_instance)->i2c_read(address, i2c_buffer, i2c_size);
    }
    else {
        // NOLINTNEXTLINE: no way around this without function pointers
        static_cast<Ssif*>(this_ssif_instance)->i2c_write(address, i2c_buffer, i2c_size);
    }
}

bool Ssif::i2c_ack_bmc(bool is_read)
{
    bool       ack_nack     = false;
    const bool txrx_pending = (_event.bits().value() & (RxReady | TxReady)) != 0;
    if (txrx_pending) {
        // Task is processing RX or TX data
        // Per SSIF spec we are allowed to NACK when busy
        return false;
    }

    if (is_read) {
        switch (_rx_cmd) {
            case ReadStart: {
                if (_tx_size > 0) {
                    ack_nack = true;
                }
                break;
            }
            case ReadMulti: {
                if ((_tx_size > MaxPartSize) && (_tx_offset > 0)) {
                    ack_nack = true;
                }
                else {
                    nv::logger::Logger::add_from_isr(
                        nv::logger::Event::SsifTxUnexpectedReadMulti.unique_id,
                        nv::logger::Level::Warning,
                        {static_cast<uint8_t>(_tx_offset), static_cast<uint8_t>(_tx_size)});
                }
                break;
            }
            default:
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::SsifTxUnexpectedCommand.unique_id,
                    nv::logger::Level::Warning,
                    {_rx_cmd});
                break;
        }
    }
    else {
        // Write address phase - data bytes are always ACKed (simplification)
        ack_nack = true;
    }

    return ack_nack;
}

bool Ssif::i2c_ack_ara(bool is_read)
{
    if (!is_read) {
        return false;
    }

    uint8_t alert = 0;
    nv::gpio::Driver::read(_alert_port_id, _alert_pin_id, alert);

    if (alert == 0) {
        nv::gpio::Driver::write(_alert_port_id, _alert_pin_id, 1U);
        return true;
    }

    return false;
}

void Ssif::i2c_write(uint8_t& address, sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size)
{
    if (i2c_size == 0) {
        // Relying on i2c_slave driver to only update i2c_size on stop or repeated start
        return;
    }

    if (address != BmcAddress) {
        // Should never happen, already filtered in i2c_slave driver
        return;
    }

    auto& rx = SmbusWriteBlock::from(i2c_buffer.data());
    switch (rx.cmd) {
        case WriteSingle:
        case WriteMultiStart:
        case WriteMultiMiddle:
        case WriteMultiEnd:
        case ReadRetry:
            // SMBus Block Write
            smbus_block_write(i2c_buffer, i2c_size);
            break;
        case ReadStart:
        case ReadMulti:
            // SMBus Block Read
            // Expecting repeated start followed by read phase, cache written command
            _rx_cmd = rx.cmd;
            break;
        default:
            nv::logger::Logger::add_from_isr(nv::logger::Event::SsifRxUnexpectedCmd.unique_id,
                                             nv::logger::Level::Warning,
                                             {rx.cmd});
            break;
    }
}

void Ssif::i2c_read(uint8_t& address, sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size)
{
    switch (address) {
        case BmcAddress: smbus_block_read(i2c_buffer, i2c_size); break;
        case AraAddress: smbus_ara_read(i2c_buffer, i2c_size); break;
        default        : break;
    }
}

void Ssif::smbus_block_write(sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size)
{
    const bool txrx_pending = (_event.bits().value() & (TxReady | RxReady)) != 0;
    if (txrx_pending) {
        // Should not happen in practice as we already NACKed the byte
        // If we end up here the write data will be discarded
        return;
    }

    auto& rx = SmbusWriteBlock::from(i2c_buffer.data());
    if ((i2c_size == 0) || (rx.size == 0) || (rx.size > MaxPartSize)) {
        nv::logger::Logger::add_from_isr(nv::logger::Event::SsifRxInvalidSize.unique_id,
                                         nv::logger::Level::Warning,
                                         {static_cast<uint8_t>(i2c_size), rx.size});
        return;
    }

    if (i2c_size > rx.size + sizeof(rx.cmd) + sizeof(rx.size)) {
        // PEC calculated in ISR context as it affects ISR-managed state
        // Ex: If PEC is invalid, we will ignore Read Retry request
        if (rx.calculate_pec(BmcAddress) != rx.get_pec()) {
            // Bad write PEC, discarding write data until next write start
            _rx_offset = 0;
            nv::logger::Logger::add_from_isr(
                nv::logger::Event::SsifRxInvalidPec.unique_id, nv::logger::Level::Warning, {});
            return;
        }
    }
    else if (i2c_size != rx.size + sizeof(rx.cmd) + sizeof(rx.size)) {
        nv::logger::Logger::add_from_isr(nv::logger::Event::SsifRxInvalidSize.unique_id,
                                         nv::logger::Level::Warning,
                                         {static_cast<uint8_t>(i2c_size), rx.size});
        return;
    }

    auto& pkt = Packet::from(_buffer);
    switch (rx.cmd) {
        case WriteSingle: {
            std::copy_n(rx.data.begin(), rx.size, pkt.ipmi_data.begin());
            _rx_size = rx.size;
            _tx_size = 0;
            nv::gpio::Driver::write(_alert_port_id, _alert_pin_id, 1U);
            _event.set(RxReady);
            break;
        }
        case WriteMultiStart: {
            if (rx.size == MaxPartSize) {
                std::copy_n(rx.data.begin(), rx.size, pkt.ipmi_data.begin());
                _rx_offset = rx.size;
                _tx_size   = 0;
                nv::gpio::Driver::write(_alert_port_id, _alert_pin_id, 1U);
            }
            else {
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::SsifRxUnexpectedWriteMulti.unique_id,
                    nv::logger::Level::Warning,
                    {rx.cmd, rx.size, static_cast<uint8_t>(_rx_offset)});
            }
            break;
        }
        case WriteMultiMiddle: {
            if ((rx.size == MaxPartSize) && (_rx_offset > 0)
                && (_rx_offset + rx.size <= MaxPayloadSize)) {
                std::copy_n(rx.data.begin(), rx.size, pkt.ipmi_data.begin() + _rx_offset);
                _rx_offset += MaxPartSize;
            }
            else {
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::SsifRxUnexpectedWriteMulti.unique_id,
                    nv::logger::Level::Warning,
                    {rx.cmd, rx.size, static_cast<uint8_t>(_rx_offset)});
                _rx_offset = 0;
            }
            break;
        }
        case WriteMultiEnd: {
            if ((_rx_offset > 0) && (_rx_offset + rx.size <= MaxPayloadSize)) {
                std::copy_n(rx.data.begin(), rx.size, pkt.ipmi_data.begin() + _rx_offset);
                _rx_size = _rx_offset + rx.size;
                _event.set(RxReady);
            }
            else {
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::SsifRxUnexpectedWriteMulti.unique_id,
                    nv::logger::Level::Warning,
                    {rx.cmd, rx.size, static_cast<uint8_t>(_rx_offset)});
            }
            _rx_offset = 0;
            break;
        }
        case ReadRetry: {
            uint8_t block_num = rx.data[0];
            if ((block_num == LastReadBlock) && (_tx_size > MaxPartSize)) {
                block_num = (_tx_size - StartPartSize) / (MaxPartSize - RemPartSize) - 1;
            }

            const size_t offset = (MaxPartSize - StartPartSize)
                                + block_num * (MaxPartSize - RemPartSize);
            if (offset < _tx_size) {
                _tx_offset = offset;
            }
            break;
        }
        default:
            // Should never get here, filtered in i2c_write
            break;
    }
}

void Ssif::smbus_block_read(sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size)
{
    const bool txrx_pending = (_event.bits().value() & (TxReady | RxReady)) != 0;
    if (txrx_pending) {
        // Should not happen in practice as we already NACKed the read address phase
        // kLPI2C_SlaveTransmitEvent is emited in the same interrupt as kLPI2C_TransmitAckEvent
        // so there is no chance of txrx_pending state changing between the two events
        i2c_size = i2c_buffer.size();
        return;
    }

    static_assert(sizeof(SmbusReadBlock) <= sizeof(i2c_buffer));
    auto& tx  = SmbusReadBlock::from(i2c_buffer.data());
    auto& pkt = Packet::from(_buffer);
    switch (_rx_cmd) {
        case ReadStart: {
            if (_tx_size > MaxPayloadSize) {
                // Corrupted state, should never happen
                _tx_size = 0;
            }
            else if (_tx_size > MaxPartSize) {
                auto& part      = StartPartData::from(tx.data);
                tx.size         = MaxPartSize;
                part.pattern[0] = 0x00;
                part.pattern[1] = 0x01;
                std::copy_n(pkt.ipmi_data.begin(), StartPartSize, part.data.begin());
                _tx_offset = StartPartSize;
            }
            else if (_tx_size > 0) {
                auto& part = SinglePartData::from(tx.data);
                tx.size    = _tx_size;
                std::copy_n(pkt.ipmi_data.begin(), _tx_size, part.data.begin());
                _tx_offset = _tx_size;
            }

            if (_tx_size > 0) {
                tx.set_pec(tx.calculate_pec(BmcAddress, ReadStart));
                nv::gpio::Driver::write(_alert_port_id, _alert_pin_id, 1U);
            }
            break;
        }
        case ReadMulti: {
            auto& part = MiddlePartData::from(tx.data);
            if ((_tx_size > MaxPartSize) && (_tx_size <= MaxPayloadSize) && (_tx_offset > 0)
                && (_tx_offset < _tx_size)) {
                const size_t rem_size = _tx_size - _tx_offset;
                if (rem_size <= RemPartSize) {
                    tx.size        = rem_size + 1;
                    part.block_num = LastReadBlock;
                }
                else {
                    tx.size        = MaxPartSize;
                    part.block_num = (_tx_offset - StartPartSize) / RemPartSize;
                }

                std::copy_n(pkt.ipmi_data.begin() + _tx_offset,
                            tx.size - sizeof(part.block_num),
                            part.data.begin());
                _tx_offset += tx.size - sizeof(part.block_num);

                tx.set_pec(tx.calculate_pec(BmcAddress, ReadMulti));
            }
            else {
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::SsifTxUnexpectedReadMulti.unique_id,
                    nv::logger::Level::Warning,
                    {static_cast<uint8_t>(_tx_offset), static_cast<uint8_t>(_tx_size)});
            }
            break;
        }
        default: {
            break;
        }
    }

    i2c_size = i2c_buffer.size();
}

void Ssif::smbus_ara_read(sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size)
{
    const std::array<uint8_t, 2>   data{(AraAddress << 1) | 1, BmcAddress << 1};
    const std::span<const uint8_t> data_item(data);

    i2c_buffer[0] = (BmcAddress << 1);
    i2c_buffer[1] = nv::i2c::crc8(data_item);
    i2c_size      = 2;
}
