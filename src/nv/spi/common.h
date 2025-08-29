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
#include <cstddef>
#include "nv/common/utils.h"
#include "nv/mctp/interface.h"

namespace nv::spi {
constexpr uint8_t         ByteShift1 = 8;
constexpr uint8_t         ByteShift2 = 16;
constexpr uint8_t         ByteShift3 = 24;
static constexpr uint32_t BufferSize = 72;
using Buffer                         = std::array<uint8_t, BufferSize>;

enum class MsgCode : uint8_t
{
    GetInfo = 0x01,
    MCTP    = 0x02,
    SetCfg  = 0x03,
    Error   = 0xFF
};

struct [[gnu::packed]] MctpBinding
{
    MsgCode msg_code;
    uint8_t data_size;  // excluding spi header (= MCTP transimission unit size + 4 (MCTP header
                        // size))
    uint8_t rsvd0;
    uint8_t rsvd1;
};

struct [[gnu::packed]] Packet
{
    MctpBinding                                                     spi_hdr;   ///< spi header
    mctp::Header                                                    mctp_hdr;  ///< mctp header
    std::array<uint8_t, mctp::PktBufDataLen - sizeof(mctp::Header)> msg;  ///< mctp message body
};

enum class MasterDevice : uint8_t
{
    Begin,
    Spi0 = Begin,
    Spi1,
    Spi2,
    End
};

enum class SlaveDevice : uint8_t
{
    Begin,
    Glacier = Begin,
    End
};

enum Cmd
{
    CMD_SREG_W8 = 0x9,
    CMD_SREG_W16,
    CMD_SREG_W32,

    CMD_SREG_R8 = 0xD,
    CMD_SREG_R16,
    CMD_SREG_R32,

    CMD_MEM_W8 = 0x21,
    CMD_MEM_W16,
    CMD_MEM_W32,

    CMD_MEM_R8 = 0x25,
    CMD_MEM_R16,
    CMD_MEM_R32,

    CMD_RD_SNGL_FIFO8  = 0x28,
    CMD_RD_SNGL_FIFO16 = 0x29,
    CMD_RD_SNGL_FIFO32 = 0x2b,

    CMD_POLL_LOW  = 0x2C,
    CMD_POLL_HIGH = 0x2D,
    CMD_POLL_ALL  = 0x2F,

    CMD_MEM_BLK_W1 = 0x80,

    CMD_MEM_BLK_R1   = 0xA0,
    CMD_RD_BLK_FIFO1 = 0xC0,

    CMD_RD_SNGL_FIFO8_FSR  = 0x68,
    CMD_RD_SNGL_FIFO16_FSR = 0x69,
    CMD_RD_SNGL_FIFO32_FSR = 0x6B,
    CMD_BLK_RD_FIFO_FSR    = 0xE0,
};

enum RegOffset
{
    CommCfg        = 0x00,
    SlaveStatus    = 0x04,
    EcStatus       = 0x08,
    InterrupEnable = 0x0C,
    Master2EcMBX   = 0x44,
    Ec2MasterMBX   = 0x48,
};

enum SlaveStatus
{
    MemoryWriteDone = 0x0001,
    MemoryReadDone  = 0x0002,
    MemoryWriteBusy = 0x0008,
    MemoryReadBusy  = 0x0010,
    SregTransBusy   = 0x0020,
    RxFifoEmpty     = 0x0100,
    TxFifoEmpty     = 0x0400,
    ObgFlag         = 0x8000,
};

enum MailboxCmd
{
    EC_ACK           = 0x01000000,
    AP_REQUEST_WRITE = 0x02000000,
    AP_READY_TO_READ = 0x03000000,
    AP_FINISHED_READ = 0x04000000,
    AP_REQUEST_RESET = 0x05000000,
    EC_MSG_AVAILABLE = 0x10000000,
};

enum class RequestType : uint32_t
{
    MctpRx,
    MctpTx,
    MctpUpdateRoutingTable,
};

enum EventBits : uint32_t
{
    MbxEvent                       = 0x01,
    TxEvent                        = 0x02,
    RxEvent                        = 0x04,
    RoutingTableEvent              = 0x08,
    GetEcMsgAvailInWaitForAckEvent = 0x10,
};

}  // namespace nv::spi