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

#include "nv/i2c/common.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"

namespace nv::usb {

class HidSmb
{
public:
    HidSmb();
    constexpr static uint32_t I2cDataBufferSize = 512;
    constexpr static uint32_t BufferSize        = 64;

    using I2cDataBuffer = std::array<uint8_t, I2cDataBufferSize>;
    using Buffer        = std::array<uint8_t, BufferSize>;

    struct [[gnu::packed]] Request
    {
        uint16_t          read_length;
        i2c::I2cStatus    result;
        uint8_t           src_id;
        uint8_t           rsvd;
        i2c::I2cHidBuffer read_buffer;
    };

    void receive(Buffer& tx_buffer, Buffer& rx_buffer, bool& is_tx_send);

protected:
    constexpr static uint32_t MaxDataSize    = 61;
    constexpr static uint32_t MaxAddrSize    = 16;
    constexpr static uint32_t MaxPayLoadSize = 63;

    enum class State
    {
        Begin = 0,
        Idle  = Begin,
        Read,
        Write,
        End,
    };

    enum class ReportId : uint8_t
    {
        DataReadReq       = 0x10,
        DataWriteReadReq  = 0x11,
        DataReadForceSend = 0x12,
        DataReadRes       = 0x13,
        DataWrite         = 0x14,
        TransferStatusReq = 0x15,
        TransferStatusRes = 0x16,
        CancelTransfer    = 0x17,
    };

    enum class Status0 : uint8_t
    {
        Idle              = 0x0,
        Busy              = 0x1,
        Complete          = 0x2,
        CompleteWithError = 0x3,
    };

    enum class Status1 : uint8_t
    {
        // if status = 1
        Acked           = 0x0,
        Nacked          = 0x1,
        ReadInProgress  = 0x2,
        WriteInProgress = 0x3,

        // if status = 2,3
        TimeoutNacked   = 0x0,
        Scllowtimeout   = 0x1,
        ArbitrationLost = 0x2,
        ReadIncomplete  = 0x3,
        WriteIncomplete = 0x4,
        Succeeded       = 0x5,
    };

    struct [[gnu::packed]] HidReportPkt
    {
        ReportId                            report_id;
        std::array<uint8_t, MaxPayLoadSize> payload;
    };
    static HidReportPkt& hid_from(Buffer& arr)
    {
        return *std::bit_cast<HidReportPkt*>(&arr[0]);
    }

    // hid 0x10
    struct [[gnu::packed]] DataReadReqPkt
    {
        ReportId hid_id;
        uint8_t  slave_addr;
        uint8_t  length_h;
        uint8_t  length_l;
    };
    static DataReadReqPkt& hid_10_from(Buffer& arr)
    {
        return *std::bit_cast<DataReadReqPkt*>(&arr[0]);
    }

    // hid 0x11
    struct [[gnu::packed]] DataWriteReadReqPkt
    {
        ReportId                         hid_id;
        uint8_t                          slave_addr;
        uint8_t                          length_h;
        uint8_t                          length_l;
        uint8_t                          tar_addr_len;
        std::array<uint8_t, MaxAddrSize> tar_addr;
    };
    static DataWriteReadReqPkt& hid_11_from(Buffer& arr)
    {
        return *std::bit_cast<DataWriteReadReqPkt*>(&arr[0]);
    }

    // hid 0x12
    struct [[gnu::packed]] DataReadForceSendPkt
    {
        ReportId hid_id;
        uint16_t length;
    };
    static DataReadForceSendPkt& hid_12_from(Buffer& arr)
    {
        return *std::bit_cast<DataReadForceSendPkt*>(&arr[0]);
    }

    // hid 0x13
    struct [[gnu::packed]] DataReadResPkt
    {
        ReportId                         hid_id;
        uint8_t                          status;
        uint8_t                          length;
        std::array<uint8_t, MaxDataSize> data;
    };
    static DataReadResPkt& hid_13_from(Buffer& arr)
    {
        return *std::bit_cast<DataReadResPkt*>(&arr[0]);
    }

    // hid 0x14
    struct [[gnu::packed]] DataWritePkt
    {
        ReportId                         hid_id;
        uint8_t                          slave_addr;
        uint8_t                          length;
        std::array<uint8_t, MaxDataSize> data;
    };
    static DataWritePkt& hid_14_from(Buffer& arr)
    {
        return *std::bit_cast<DataWritePkt*>(&arr[0]);
    }

    // hid 0x16
    struct [[gnu::packed]] TransferStatusResPkt
    {
        ReportId hid_id;
        Status0  status0;
        Status1  status1;
        uint8_t  retry_h;
        uint8_t  retry_l;
        uint8_t  bytes_rx_h;
        uint8_t  bytes_rx_l;
    };
    static TransferStatusResPkt& hid_16_from(Buffer& arr)
    {
        return *std::bit_cast<TransferStatusResPkt*>(&arr[0]);
    }

    struct I2cReadContext
    {
        I2cDataBuffer buffer{};
        size_t        bytes_sent{};

        void reset()
        {
            buffer     = {};
            bytes_sent = 0;
        }
    };

    State          _state{};
    uint16_t       read_length{};
    I2cReadContext _read_context{};
    ipc::Queue&    _queue;
};

}  // namespace nv::usb
