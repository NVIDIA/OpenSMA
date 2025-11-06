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
#include <span>

#include "nv/spi/port.h"
#include "nv/spi/spb.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/gpio/common.h"
#include "sys/spi/spi_edma.h"
#include "nv/spi/common.h"
using namespace std::chrono_literals;

namespace nv::spi {

enum FlashromCmdCode
{
    SPI_CMD_CONFIG       = 0x0,
    SPI_CMD_READ         = 0x1,
    SPI_CMD_WRITE        = 0x2,
    SPI_CMD_WRITE_READ   = 0x3,
    SPI_CMD_POSTED_WRITE = 0x4,
    SPI_CMD_END          = 0xf
};

struct [[gnu::packed]] FlashromMsgHdr
{
    uint8_t channelId;
    uint8_t cmdCode;
    uint8_t len_lsb;
    uint8_t len_msb;
};

inline FlashromMsgHdr* FlashromMsgHdr_from(std::array<uint8_t, nv::ipc::UsbSpiMsgSize>& arr)
{
    return reinterpret_cast<FlashromMsgHdr*>(arr.data());
}

inline uint8_t* FlashromMsgData_from(std::array<uint8_t, nv::ipc::UsbSpiMsgSize>& arr)
{
    return arr.data() + sizeof(FlashromMsgHdr);
}

class Flashrom
{
public:
    constexpr static uint8_t  SPI_HEADER_LEN   = 4;
    constexpr static uint16_t SPI_PACKET_SIZE  = 512;
    constexpr static uint16_t SPI_MAX_DATA_LEN = (SPI_PACKET_SIZE - SPI_HEADER_LEN);

    // Command code mask for extracting the lower 4 bits
    constexpr static uint8_t CMD_CODE_MASK   = 0x0f;
    constexpr static uint8_t SPI_CS_ASSERT   = 0x20;
    constexpr static uint8_t SPI_CS_DEASSERT = 0x10;
    constexpr static uint8_t SPI_CS_MASK     = 0xc0;
    constexpr static uint8_t SPI_CS0         = 0x00;
    constexpr static uint8_t SPI_CS1         = 0x40;
    constexpr static uint8_t SPI_CS2         = 0x80;
    constexpr static uint8_t SPI_CS3         = 0xC0;

    sys::spi::EdmaDriver _driver;

    Flashrom(sys::spi::EdmaDriver& driver);

    void handle_tx(std::array<uint8_t, nv::ipc::UsbSpiMsgSize>& buffer);
    void bind(nv::spi::Flexcomm flexcomm,
              nv::ipc::EventId  event_id,
              uint8_t           cs0_port_id,
              uint8_t           cs0_pin_id,
              uint8_t           cs1_port_id,
              uint8_t           cs1_pin_id);
    void init();

private:
    uint8_t _cs0_port_id{0};
    uint8_t _cs0_pin_id{0};
    uint8_t _cs1_port_id{0};
    uint8_t _cs1_pin_id{0};
};
}  // namespace nv::spi