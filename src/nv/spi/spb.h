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
#include "nv/spi/common.h"
#include "sys/spi/spi.h"
#include "nv/ipc/event.h"
using namespace std::chrono_literals;
namespace nv::spi {
constexpr uint8_t MaxBytesPerTransaction = 32;
constexpr uint8_t AlginmentMask4Bytes    = 0xFC;

constexpr uint16_t SpbMasterWriteOffset = 0x0000;
constexpr uint16_t SpbMasterReadOffset  = 0x8000;

// TODO : Decide if needed
constexpr uint32_t PollObfLimit = 1000;

// TODO : Decide what time is suitable
constexpr uint32_t PollCmdTimeout = 100000;
// TODO : Decide what time is suitable
constexpr auto EventWaitTimeout = 500ms;

constexpr uint8_t PostedReadSuccessStatus = 0x02;
// TODO : Decide use retry time or time period
constexpr uint8_t WaitAckTryTime = 5;

// Len for spi
constexpr uint8_t CmdLen       = 1;
constexpr uint8_t RegLen       = 4;
constexpr uint8_t StatusLen    = 2;
constexpr uint8_t AddrLen      = 2;
constexpr uint8_t WaitCycle    = 0;
constexpr uint8_t TarCycle     = 1;  // 1 in single, 4 in quad
constexpr uint8_t TarWaitCycle = WaitCycle + TarCycle;

using SpbTransBuffer = std::array<uint8_t, MaxBytesPerTransaction>;

enum class SpbStatus : uint32_t
{
    Ok,
    PollCmdTimeout,
    MbxEventTimeout,
    PostedReadError,
    PostedWriteError,
    WaitAckError,
    WaitTxAck0Error,
    WaitTxAck1Error,
    WaitRxAckError,
    WaitResetAckError,
    WaitLenError,
    WriteLenError,
    GetEcMsgAvailNotAck,
    GetEcMsgAvailNotLen,
    GetAckNotEcMsgAvail,
    // mailbox_write error
    RequestWriteError,
    ReadyToReadError,
    FinishedReadError,
    RequestResetError,
    // Packet rewrite error
    SendInvalidMctpPkt,
    RecvInvalidSpiPkt,
    // Spi event error
    SetRxEventFail,
    EventClearFail,

    InterruptHandleError,
    MbxReadFailInTask,
    // Likely not happen
    InvalidParameter,
};

enum class Timeout : uint8_t
{
    SregNotBusy,
    MemoryWriteBusyRxFifoEmpty,
    TxFifoNotEmpty,
    MbxEvent,
    PostedRead,
};

class Spb
{
public:
    sys::spi::Driver& _driver;

    Spb(sys::spi::Driver& driver, nv::ipc::Event& event, nv::mctp::Client client);

    bool fill_spi_pkt_from_mctp_pkt(Buffer& buffer, uint8_t& len);
    bool fill_mctp_pkt_from_spi_pkt(Buffer& buffer);

    uint16_t cmd_poll_low();
    uint32_t cmd_poll_all();

    SpbStatus wait_for_sreg_not_busy();
    SpbStatus wait_for_memory_write_busy_and_rx_fifo_empty();
    SpbStatus wait_for_tx_fifo_not_empty();
    SpbStatus wait_for_ec2spim_mbx_written(uint32_t& ec2spimb);

    SpbStatus mailbox_write(uint32_t value);
    SpbStatus mailbox_read(uint32_t& value);

    SpbStatus clear_memory_write_done();
    SpbStatus clear_memory_read_done();

    SpbStatus sreg_write_8(uint16_t addr, uint8_t value);
    SpbStatus sreg_write_32(uint16_t addr, uint32_t value);
    SpbStatus sreg_read_32(uint16_t addr, uint32_t& value);

    SpbStatus sreg_check_status_reg(uint16_t reg);

    SpbStatus wait_for_ack();
    SpbStatus wait_for_length(uint8_t& value);

    SpbStatus posted_write(uint16_t spb_offset, uint8_t len, Buffer& buffer);
    SpbStatus posted_read_helper(
        uint8_t cmd1, uint8_t cmd2, uint16_t addr, uint8_t len, std::span<uint8_t> read_buffer);
    SpbStatus posted_read(uint16_t spb_offset, uint8_t len, Buffer& buffer);

    SpbStatus spi_mctp_send(Buffer& buffer);
    SpbStatus spi_mctp_recv(Buffer& buffer);
    SpbStatus reset();

private:
    nv::ipc::Event&  _event;
    nv::mctp::Client _client;
};
}  // namespace nv::spi