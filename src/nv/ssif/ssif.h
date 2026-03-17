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
#pragma once
#include "nv/ipc/event.h"
#include "sys/i2c/i2c_slave.h"
#include "nv/lstp/lstp_router.h"
#include "nv/common/literals.h"

namespace nv::ssif {

constexpr static size_t  MaxPartSize   = 32;
constexpr static size_t  StartPartSize = MaxPartSize - 2;
constexpr static size_t  RemPartSize   = MaxPartSize - 1;
constexpr static uint8_t LastReadBlock = 0xFF;

// Includes extra byte for PEC when size == MaxPartSize
using PartDataBuffer = std::array<uint8_t, MaxPartSize + 1>;

enum EventBits : nv::ipc::Event::Bits
{
    RxReady     = 1_bit,
    TxReady     = 2_bit,
    SlaveEnable = 3_bit
};

enum SmbusCommand : uint8_t
{
    // Block Write
    WriteSingle      = 0x02,
    WriteMultiStart  = 0x06,
    WriteMultiMiddle = 0x07,
    WriteMultiEnd    = 0x08,
    // Block Read
    ReadStart = 0x03,
    ReadMulti = 0x09,
    ReadRetry = 0x0A
};

struct [[gnu::packed]] SmbusWriteBlock
{
    uint8_t        cmd;
    uint8_t        size;
    PartDataBuffer data;

    static SmbusWriteBlock& from(uint8_t* buffer)
    {
        return *std::bit_cast<SmbusWriteBlock*>(buffer);
    }

    uint8_t get_pec(void) { return data[std::min<size_t>(size, MaxPartSize)]; }

    uint8_t calculate_pec(uint8_t address);
};

struct [[gnu::packed]] SmbusReadBlock
{
    uint8_t        size;
    PartDataBuffer data;

    static SmbusReadBlock& from(uint8_t* buffer)
    {
        return *std::bit_cast<SmbusReadBlock*>(buffer);
    }

    void set_pec(uint8_t pec) { data[std::min<size_t>(size, MaxPartSize)] = pec; }

    uint8_t calculate_pec(uint8_t address, uint8_t cmd);
};

struct [[gnu::packed]] SinglePartData
{
    std::array<uint8_t, MaxPartSize> data;

    static SinglePartData& from(PartDataBuffer& buffer)
    {
        static_assert(sizeof(SinglePartData) <= sizeof(PartDataBuffer));
        return *std::bit_cast<SinglePartData*>(buffer.data());
    }
};

struct [[gnu::packed]] StartPartData
{
    std::array<uint8_t, 2>             pattern;
    std::array<uint8_t, StartPartSize> data;

    static StartPartData& from(PartDataBuffer& buffer)
    {
        static_assert(sizeof(StartPartData) <= sizeof(PartDataBuffer));
        return *std::bit_cast<StartPartData*>(buffer.data());
    }
};

struct [[gnu::packed]] MiddlePartData
{
    uint8_t                          block_num;
    std::array<uint8_t, RemPartSize> data;

    static MiddlePartData& from(PartDataBuffer& buffer)
    {
        static_assert(sizeof(MiddlePartData) <= sizeof(PartDataBuffer));
        return *std::bit_cast<MiddlePartData*>(buffer.data());
    }
};

class Ssif
{
public:
    constexpr static size_t MaxPayloadSize = 254;

    using Buffer = std::array<uint8_t, nv::ipc::UsbLstpMsgSize>;

    struct [[gnu::packed]] Packet
    {
        nv::lstp::LstpHdr                   hdr;
        std::array<uint8_t, MaxPayloadSize> ipmi_data;

        static Packet& from(Buffer& buffer) { return *std::bit_cast<Packet*>(buffer.data()); }
    };

    Ssif(nv::ipc::Event& event) : _event(event) {}

    // Task interface
    void               bind(nv::i2c::Port      i2c_bus,
                            nv::gpio::GpioPort alert_port_id,
                            nv::gpio::GpioPin  alert_pin_id);
    void               start();
    std::span<uint8_t> get_tx_buffer();
    bool               handle_tx();
    void               handle_rx();

    // Callbacks for I2C slave events (ISR context)
    static bool i2c_ack_callback(uint8_t& address, bool is_read, void* this_ssif_instance);
    static void i2c_callback(uint8_t&                  address,
                             bool                      is_read,
                             sys::i2c::I2cSlaveBuffer& i2c_buffer,
                             size_t&                   i2c_size,
                             void*                     this_ssif_instance,
                             bool                      new_transaction);

private:
    constexpr static uint8_t BmcAddress = 0x10;
    constexpr static uint8_t AraAddress = 0x0c;

    nv::ipc::Event&                _event;
    sys::i2c::I2CSlaveDriver<Ssif> _driver;

    // Buffer for assembled IPMI messages (up to 254 bytes)
    // Since SSIF is a single threaded / half-duplex interface, single buffer is used for both
    // TX and RX To avoid excess copies, the buffer also contains the LSTP header and padding to
    // 512 bytes (needed to share one USB TX queue between SPI and SSIF)
    Buffer _buffer;

    // Received command code, used for write-read transfers
    uint8_t _rx_cmd;

    // Received message size, set once the message is fully assembled
    size_t _rx_size;

    // Received message bytes received so far
    // Increments after each block until message fully assembled
    size_t _rx_offset;

    // TX message size, updated on new USB message from BMC
    size_t _tx_size;

    // Current TX offsets, increments after each block
    // Can be reset using Read Start or Read Retry
    size_t _tx_offset;

    nv::i2c::Port  _i2c_bus;
    gpio::GpioPort _alert_port_id;
    gpio::GpioPin  _alert_pin_id;

    // Private helpers for I2C slave event handling (ISR context)
    bool i2c_ack_bmc(bool is_read);
    bool i2c_ack_ara(bool is_read);
    void i2c_write(uint8_t& address, sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size);
    void i2c_read(uint8_t& address, sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size);
    void smbus_block_write(sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size);
    void smbus_block_read(sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size);
    void smbus_ara_read(sys::i2c::I2cSlaveBuffer& i2c_buffer, size_t& i2c_size);
};
}  // namespace nv::ssif
