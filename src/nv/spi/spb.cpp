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
/*----------------------------------------------------------------------------------------------
Function programming logic refers to glacierapp
----------------------------------------------------------------------------------------------*/

#include "nv/spi/spb.h"
#include "nv/spi/task.h"
#include "nv/spi/utils.h"
#include "sys/spi/spi.h"
#include "sys/ctimer/ctimer.h"

using namespace nv::spi;

#include <cstring>

Spb::Spb(sys::spi::Driver& driver, nv::ipc::Event& event, nv::mctp::Client client)
: _driver(driver)
, _event(event)
, _client(client)
{}

bool Spb::fill_spi_pkt_from_mctp_pkt(Buffer& buffer, uint8_t& len)
{
    mctp::PrivateHeader priv{};
    memcpy(&priv, buffer.data(), sizeof(mctp::PrivateHeader));

    if (priv.packet_length > BufferSize - sizeof(mctp::PrivateHeader)) {
        return false;
    }

    MctpBinding spi_hdr{};
    spi_hdr.msg_code  = MsgCode::MCTP;
    spi_hdr.data_size = (uint8_t)(priv.packet_length);
    memcpy(buffer.data(), &spi_hdr, sizeof(MctpBinding));

    len = spi_hdr.data_size + sizeof(MctpBinding);
    return true;
}

bool Spb::fill_mctp_pkt_from_spi_pkt(Buffer& buffer)
{
    MctpBinding spi_hdr{};
    memcpy(&spi_hdr, buffer.data(), sizeof(MctpBinding));

    if (spi_hdr.msg_code != MsgCode::MCTP
        || spi_hdr.data_size > BufferSize - sizeof(MctpBinding)) {
        return false;
    }

    mctp::PrivateHeader priv{};
    priv.packet_length = spi_hdr.data_size;

    priv.packet_interface = static_cast<uint16_t>(_client) > UINT8_MAX
                              ? 0
                              : nv::common::to_underlying(_client);

    memcpy(buffer.data(), &priv, sizeof(mctp::PrivateHeader));

    return true;
}

SpbStatus Spb::sreg_check_status_reg([[maybe_unused]] uint16_t reg)
{
    // TODO: If needed any additional validation
    return SpbStatus::Ok;
}

SpbStatus Spb::wait_for_sreg_not_busy()
{
    auto StartTimeStamp = nv::ctimer::Driver::read_ticks();

    // SregTransBusy Bit 5 (Expect 0)
    while ((cmd_poll_low() & SregTransBusy) != 0) {  // sreg trans not complete
        auto CurrentTimeStamp = nv::ctimer::Driver::read_ticks();
        if (sys::ctimer::Driver::get_counter_difference(StartTimeStamp, CurrentTimeStamp)
            > PollCmdTimeout) {
            spi::Task::timeout_log(Timeout::SregNotBusy);
            return SpbStatus::PollCmdTimeout;
        }
    }
    return SpbStatus::Ok;
}

SpbStatus Spb::wait_for_memory_write_busy_and_rx_fifo_empty()
{
    auto StartTimeStamp = nv::ctimer::Driver::read_ticks();

    // RxFifoEmpty Bit 8 (Expect 1), MemoryWriteBusy Bit 3 (Expect 0)
    // TODO : Check understanding,
    // In glacierapp, while condition : cmd_poll_all() & 0x108
    while ((cmd_poll_low() & (RxFifoEmpty | MemoryWriteBusy)) != RxFifoEmpty) {
        auto CurrentTimeStamp = nv::ctimer::Driver::read_ticks();
        if (sys::ctimer::Driver::get_counter_difference(StartTimeStamp, CurrentTimeStamp)
            > PollCmdTimeout) {
            spi::Task::timeout_log(Timeout::MemoryWriteBusyRxFifoEmpty);
            return SpbStatus::PollCmdTimeout;
        }
    }
    return SpbStatus::Ok;
}

SpbStatus Spb::wait_for_tx_fifo_not_empty()
{
    auto StartTimeStamp = nv::ctimer::Driver::read_ticks();

    // TxFifoEmpty Bit 10 (Expect 0)
    while ((cmd_poll_low() & TxFifoEmpty) != 0) {
        auto CurrentTimeStamp = nv::ctimer::Driver::read_ticks();
        if (sys::ctimer::Driver::get_counter_difference(StartTimeStamp, CurrentTimeStamp)
            > PollCmdTimeout) {
            spi::Task::timeout_log(Timeout::TxFifoNotEmpty);
            return SpbStatus::PollCmdTimeout;
        }
    }
    return SpbStatus::Ok;
}

// Wait for the event set by interrupt pin
SpbStatus Spb::wait_for_ec2spim_mbx_written(uint32_t& ec2spimb)
{
    auto status = SpbStatus::Ok;
    auto wait   = EventBits::MbxEvent;
    auto event  = _event.wait(wait, false, false, EventWaitTimeout);
    if (event.value() & EventBits::MbxEvent) {
        auto event_status = _event.clear(EventBits::MbxEvent);
        if (event_status != ipc::Event::Status::Ok) {
            return SpbStatus::EventClearFail;
        }
    }
    else {
        spi::Task::timeout_log(Timeout::MbxEvent);
        return SpbStatus::MbxEventTimeout;
    }

    status = mailbox_read(ec2spimb);

    if (status != SpbStatus::Ok) {
        return status;
    }

    return SpbStatus::Ok;
}

uint16_t Spb::cmd_poll_low()
{
    const uint8_t send_len = CmdLen;
    const uint8_t recv_len = TarCycle + sizeof(uint16_t);

    std::array<uint8_t, send_len> sbuf{};
    std::array<uint8_t, recv_len> rbuf{};

    sbuf.at(0) = CMD_POLL_LOW;
    _driver.sendRecv(send_len, sbuf, recv_len, rbuf);

    return buf_to_u16(rbuf, TarCycle);
}

uint32_t Spb::cmd_poll_all()
{
    const uint8_t send_len = CmdLen;
    const uint8_t recv_len = TarCycle + RegLen;

    std::array<uint8_t, send_len> sbuf{};
    std::array<uint8_t, recv_len> rbuf{};

    sbuf.at(0) = CMD_POLL_ALL;
    _driver.sendRecv(send_len, sbuf, recv_len, rbuf);

    return buf_to_u32(rbuf, TarCycle);
}

SpbStatus Spb::mailbox_write(uint32_t value)
{
    return sreg_write_32(Master2EcMBX, value);
}

SpbStatus Spb::mailbox_read(uint32_t& value)
{
    return sreg_read_32(Ec2MasterMBX, value);
}

SpbStatus Spb::clear_memory_write_done()
{
    return sreg_write_8(SlaveStatus, MemoryWriteDone);
}

SpbStatus Spb::clear_memory_read_done()
{
    return sreg_write_8(SlaveStatus, MemoryReadDone);
}

SpbStatus Spb::sreg_write_8(uint16_t addr, uint8_t value)
{
    auto status = wait_for_sreg_not_busy();

    if (status != SpbStatus::Ok) {
        return status;
    }

    const uint8_t send_len = CmdLen + AddrLen + sizeof(value);
    const uint8_t recv_len = TarWaitCycle + StatusLen;

    std::array<uint8_t, send_len> sbuf{};
    std::array<uint8_t, recv_len> rbuf{};

    sbuf.at(0) = CMD_SREG_W8;
    u16_to_buf(sbuf, addr, CmdLen);
    sbuf.at(CmdLen + AddrLen) = value;

    _driver.sendRecv(send_len, sbuf, recv_len, rbuf);

    return sreg_check_status_reg(buf_to_u16(rbuf, TarWaitCycle));
}

SpbStatus Spb::sreg_write_32(uint16_t addr, uint32_t value)
{
    auto status = wait_for_sreg_not_busy();

    if (status != SpbStatus::Ok) {
        return status;
    }

    const uint8_t send_len = CmdLen + AddrLen + sizeof(value);
    const uint8_t recv_len = TarWaitCycle + StatusLen;

    std::array<uint8_t, send_len> sbuf{};
    std::array<uint8_t, recv_len> rbuf{};

    sbuf.at(0) = CMD_SREG_W32;
    u16_to_buf(sbuf, addr, CmdLen);
    u32_to_buf(sbuf, value, CmdLen + AddrLen);

    _driver.sendRecv(send_len, sbuf, recv_len, rbuf);

    return sreg_check_status_reg(buf_to_u16(rbuf, TarWaitCycle));
}

SpbStatus Spb::sreg_read_32(uint16_t addr, uint32_t& value)
{
    auto status = wait_for_sreg_not_busy();

    if (status != SpbStatus::Ok) {
        return status;
    }

    const uint8_t send_len = CmdLen + AddrLen;
    const uint8_t recv_len = TarWaitCycle + StatusLen + sizeof(value);

    std::array<uint8_t, send_len> sbuf{};
    std::array<uint8_t, recv_len> rbuf{};

    sbuf.at(0) = CMD_SREG_R32;
    u16_to_buf(sbuf, addr, CmdLen);

    _driver.sendRecv(send_len, sbuf, recv_len, rbuf);

    value = buf_to_u32(rbuf, TarWaitCycle + 2);

    return sreg_check_status_reg(buf_to_u16(rbuf, TarWaitCycle));
}

SpbStatus Spb::posted_write(uint16_t spb_offset, uint8_t len, Buffer& buffer)
{
    auto status = SpbStatus::Ok;

    uint8_t                        off       = 0;
    const uint8_t                  send_len1 = CmdLen + AddrLen;
    std::array<uint8_t, send_len1> sbuf1{};
    while (len >= 4) {
        uint8_t bytes = (len > MaxBytesPerTransaction) ? MaxBytesPerTransaction : len;

        bytes                           &= AlginmentMask4Bytes;
        const uint32_t calculate_offset  = spb_offset + off;
        const uint32_t cmd               = CMD_MEM_BLK_W1 + ((bytes >> 2) - 1);

        if ((calculate_offset > UINT16_MAX) || (cmd > UINT8_MAX)) {
            return SpbStatus::InvalidParameter;
        }
        sbuf1.at(0) = static_cast<uint8_t>(cmd);
        u16_to_buf(sbuf1, static_cast<uint16_t>(calculate_offset), CmdLen);

        status = wait_for_memory_write_busy_and_rx_fifo_empty();
        if (status != SpbStatus::Ok) {
            return status;
        }

        _driver.sendRecv(send_len1,
                         sbuf1,
                         0,
                         {},
                         static_cast<uint8_t>(sys::spi::Driver::ActionBitmap::CsPull0));
        _driver.sendRecv(
            bytes,
            std::span<uint8_t>(buffer.data() + off, bytes),
            0,
            {},
            static_cast<uint8_t>(sys::spi::Driver::ActionBitmap::CsPull1)
                | static_cast<uint8_t>(sys::spi::Driver::ActionBitmap::TxByteShift));

        status = clear_memory_write_done();

        if (status != SpbStatus::Ok) {
            return status;
        }

        // Will not happen
        if (off > UINT8_MAX - bytes) {
            off = UINT8_MAX;
        }
        else {
            off += bytes;
        }
        len -= bytes;
    }

    for (uint8_t i = 0; i < len; ++i) {
        // 1 for transferring 1 byte
        const uint8_t send_len = CmdLen + AddrLen + 1;

        std::array<uint8_t, send_len> sbuf{};

        const uint32_t calculate_offset = spb_offset + off + i;

        if ((calculate_offset > UINT16_MAX)) {
            return SpbStatus::InvalidParameter;
        }

        sbuf.at(0) = CMD_MEM_W8;
        u16_to_buf(sbuf, static_cast<uint16_t>(calculate_offset), CmdLen);
        sbuf.at(CmdLen + AddrLen) = buffer.at(off + i);

        status = wait_for_memory_write_busy_and_rx_fifo_empty();
        if (status != SpbStatus::Ok) {
            return status;
        }

        _driver.sendRecv(send_len, sbuf, 0, {});

        status = clear_memory_write_done();

        if (status != SpbStatus::Ok) {
            return status;
        }
    }

    return status;
}

SpbStatus Spb::posted_read_helper(
    uint8_t cmd1, uint8_t cmd2, uint16_t addr, uint8_t len, std::span<uint8_t> read_buffer)
{
    auto status = SpbStatus::Ok;

    const uint8_t                  send_len1 = CmdLen + AddrLen;
    std::array<uint8_t, send_len1> sbuf1{};

    // Send post read command
    sbuf1.at(0) = cmd1;
    u16_to_buf(sbuf1, addr, CmdLen);

    _driver.sendRecv(send_len1, sbuf1, 0, {});

    status = wait_for_tx_fifo_not_empty();
    if (status != SpbStatus::Ok) {
        return status;
    }

    const uint8_t send_len2 = CmdLen;
    const uint8_t recv_len2 = TarCycle + StatusLen;

    std::array<uint8_t, send_len2> sbuf2{};
    std::array<uint8_t, recv_len2> rbuf2{};

    sbuf2.at(0) = cmd2;

    _driver.sendRecv(send_len2,
                     sbuf2,
                     recv_len2,
                     rbuf2,
                     static_cast<uint8_t>(sys::spi::Driver::ActionBitmap::CsPull0));

    if ((rbuf2[TarCycle + 1] & PostedReadSuccessStatus) == 0) {
        auto StartTimeStamp = nv::ctimer::Driver::read_ticks();

        while (true) {
            auto CurrentTimeStamp = nv::ctimer::Driver::read_ticks();

            if (sys::ctimer::Driver::get_counter_difference(StartTimeStamp, CurrentTimeStamp)
                > PollCmdTimeout) {
                spi::Task::timeout_log(Timeout::PostedRead);
                return SpbStatus::PostedReadError;
            }

            const uint8_t recv_len3 = StatusLen;

            std::array<uint8_t, recv_len3> rbuf3{};

            _driver.sendRecv(0,
                             {},
                             recv_len3,
                             rbuf3,
                             static_cast<uint8_t>(sys::spi::Driver::ActionBitmap::None));

            if ((rbuf3[1] & PostedReadSuccessStatus) != 0) {
                break;
            }
        }
    }

    _driver.sendRecv(0,
                     {},
                     len,
                     read_buffer,
                     static_cast<uint8_t>(sys::spi::Driver::ActionBitmap::CsPull1)
                         | static_cast<uint8_t>(sys::spi::Driver::ActionBitmap::RxByteShift));
    status = clear_memory_read_done();
    if (status != SpbStatus::Ok) {
        return status;
    }

    return status;
}

SpbStatus Spb::posted_read(uint16_t spb_offset, uint8_t len, Buffer& buffer)
{
    auto status = SpbStatus::Ok;

    std::array<uint8_t, 4> rbuf{};
    uint8_t                off = 0;
    while (len >= 4) {
        uint8_t bytes = (len > MaxBytesPerTransaction) ? MaxBytesPerTransaction : len;
        if (bytes >= 4) {
            bytes &= AlginmentMask4Bytes;

            const uint32_t calculate_offset = spb_offset + off;
            const uint32_t cmd1             = CMD_MEM_BLK_R1 + ((bytes >> 2) - 1);
            const uint32_t cmd2             = CMD_BLK_RD_FIFO_FSR + ((bytes >> 2) - 1);

            if ((calculate_offset > UINT16_MAX) || (cmd1 > UINT8_MAX) || (cmd2 > UINT8_MAX)) {
                return SpbStatus::InvalidParameter;
            }

            status = posted_read_helper(static_cast<uint8_t>(cmd1),
                                        static_cast<uint8_t>(cmd2),
                                        static_cast<uint16_t>(calculate_offset),
                                        bytes,
                                        std::span<uint8_t>(buffer.data() + off, bytes));
            if (status != SpbStatus::Ok) {
                return status;
            }
        }
        // Will not happen
        if (off > UINT8_MAX - bytes) {
            off = UINT8_MAX;
        }
        else {
            off += bytes;
        }
        len -= bytes;
    }

    if (len > 0) {
        const uint8_t bytes = 4;

        const uint32_t calculate_offset = spb_offset + off;

        if ((calculate_offset > UINT16_MAX)) {
            return SpbStatus::InvalidParameter;
        }

        status = posted_read_helper(CMD_MEM_BLK_R1,
                                    CMD_BLK_RD_FIFO_FSR,
                                    static_cast<uint16_t>(calculate_offset),
                                    bytes,
                                    rbuf);
        if (status != SpbStatus::Ok) {
            return status;
        }

        for (uint8_t i = 0; i < len; i++) {
            buffer.at(off + i) = rbuf.at(i);
        }
    }
    return status;
}

SpbStatus Spb::wait_for_ack()
{
    auto     status   = SpbStatus::Ok;
    uint32_t ec2spimb = 0;

    // 1st Sequence
    status = wait_for_ec2spim_mbx_written(ec2spimb);

    if (status != SpbStatus::Ok) {
        return status;
    }

    if ((ec2spimb & EC_MSG_AVAILABLE) && (ec2spimb & EC_ACK)) {
        // Set RxEvent to handle rx
        auto event_status = _event.set(EventBits::RxEvent, false);
        if (event_status != nv::ipc::Event::Status::Ok) {
            return SpbStatus::SetRxEventFail;
        }
        return SpbStatus::Ok;
    }

    else if (ec2spimb & EC_ACK) {
        return SpbStatus::Ok;
    }

    else if (ec2spimb & EC_MSG_AVAILABLE) {
        spi::Task::middle_error_log(SpbStatus::GetEcMsgAvailNotAck);
        // Set RxEvent to handle rx
        auto event_status = _event.set(EventBits::RxEvent, false);
        if (event_status != nv::ipc::Event::Status::Ok) {
            return SpbStatus::SetRxEventFail;
        }
        // no return -> keep waiting for ack
    }
    // Should not happen (slave should not pull interrupt pin if no data in mbx)
    else {
        return SpbStatus::WaitAckError;
    }

    // 2nd Sequence (Case of GetEcMsgAvailNotAck in 1st sequence)

    status = wait_for_ec2spim_mbx_written(ec2spimb);
    // return SpbStatus::GetEcMsgAvailNotAck indicates there is EC_MSG_AVAIL
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::GetEcMsgAvailNotAck;
    }

    // Should not happen (slave should not send EC_MSG_AVAIL twice in sequence)
    if ((ec2spimb & EC_MSG_AVAILABLE) && (ec2spimb & EC_ACK)) {
        // Set RxEvent to handle rx
        auto event_status = _event.set(EventBits::RxEvent, false);
        if (event_status != nv::ipc::Event::Status::Ok) {
            return SpbStatus::SetRxEventFail;
        }
        return SpbStatus::Ok;
    }
    else if (ec2spimb & EC_ACK) {
        return SpbStatus::Ok;
    }

    // Should not happen (slave should not send EC_MSG_AVAIL twice in sequence)
    else if (ec2spimb & EC_MSG_AVAILABLE) {
        spi::Task::middle_error_log(SpbStatus::GetEcMsgAvailNotAck);
        // Set RxEvent to handle rx
        auto event_status = _event.set(EventBits::RxEvent, false);
        if (event_status != nv::ipc::Event::Status::Ok) {
            return SpbStatus::SetRxEventFail;
        }
        return SpbStatus::GetEcMsgAvailNotAck;
    }
    // Should not happen (slave should not pull interrupt pin if no data in mbx)
    else {
        // TODO : decide return GetEcMsgAvailNotAck or WaitAckError?
        return SpbStatus::WaitAckError;
    }
    // No any EC_ACK
    return SpbStatus::WaitAckError;
}

SpbStatus Spb::wait_for_length(uint8_t& value)
{
    auto     status   = SpbStatus::Ok;
    uint32_t ec2spimb = 0;

    status = wait_for_ec2spim_mbx_written(ec2spimb);
    if (status != SpbStatus::Ok) {
        return status;
    }

    // If getting EC_MSG_AVAILABLE during rx,
    // -> assume slave terminated the previous rx
    if (ec2spimb & EC_MSG_AVAILABLE) {
        // print error log
        spi::Task::middle_error_log(SpbStatus::GetEcMsgAvailNotLen);
        // Set RxEvent to handle rx
        auto event_status = _event.set(EventBits::RxEvent, false);
        if (event_status != nv::ipc::Event::Status::Ok) {
            return SpbStatus::SetRxEventFail;
        }
        return SpbStatus::GetEcMsgAvailNotLen;
    }

    // Should not happen (slave should not pull interrupt pin if no data in mbx)
    if ((ec2spimb & UINT8_MAX) == 0) {
        status = SpbStatus::WaitLenError;
    }

    value = ec2spimb & UINT8_MAX;

    return status;
}

SpbStatus Spb::spi_mctp_send(Buffer& buffer)
{
    uint8_t len = 0;
    if (fill_spi_pkt_from_mctp_pkt(buffer, len) == false) {
        return SpbStatus::SendInvalidMctpPkt;
    }
    auto status = SpbStatus::Ok;

    status = mailbox_write(AP_REQUEST_WRITE);
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::RequestWriteError;
    }
    status = wait_for_ack();
    // Assume ack is recevied if GetEcMsgAvailNotAck
    if ((status != SpbStatus::Ok) && (status != SpbStatus::GetEcMsgAvailNotAck)) {
        spi::Task::middle_error_log(status);
        return SpbStatus::WaitTxAck0Error;
    }

    status = posted_write(SpbMasterWriteOffset, len, buffer);
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::PostedWriteError;
    }

    status = mailbox_write(len);
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::WriteLenError;
    }

    status = wait_for_ack();
    // Assume ack is recevied if GetEcMsgAvailNotAck
    if ((status != SpbStatus::Ok) && (status != SpbStatus::GetEcMsgAvailNotAck)) {
        spi::Task::middle_error_log(status);
        return SpbStatus::WaitTxAck1Error;
    }

    return status;
}

SpbStatus Spb::spi_mctp_recv(Buffer& buffer)
{
    auto status = SpbStatus::Ok;

    status = mailbox_write(AP_READY_TO_READ);
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::ReadyToReadError;
    }

    uint8_t len = 0;
    status      = wait_for_length(len);
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::WaitLenError;
    }

    status = posted_read(SpbMasterReadOffset, len, buffer);
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::PostedReadError;
    }

    status = mailbox_write(AP_FINISHED_READ);
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::FinishedReadError;
    }

    status = wait_for_ack();
    // Assume ack is recevied if GetEcMsgAvailNotAck
    if ((status != SpbStatus::Ok) && (status != SpbStatus::GetEcMsgAvailNotAck)) {
        spi::Task::middle_error_log(status);
        return SpbStatus::WaitRxAckError;
    }

    if (fill_mctp_pkt_from_spi_pkt(buffer) == false) {
        return SpbStatus::RecvInvalidSpiPkt;
    }

    return status;
}

// To be called after a timeout
SpbStatus Spb::reset()
{
    auto status = SpbStatus::Ok;

    // initiate reset
    status = mailbox_write(AP_REQUEST_RESET);
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::RequestResetError;
    }

    status = wait_for_ack();
    if (status != SpbStatus::Ok) {
        spi::Task::middle_error_log(status);
        return SpbStatus::WaitResetAckError;
    }

    return status;
}