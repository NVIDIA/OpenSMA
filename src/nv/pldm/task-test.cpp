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
#include <chrono>
#include <cstring>
#include <bit>
#include <thread>

#include "nv/ipc/queue.h"
#include "nv/ut/unittest.h"

#include "nv/ut/unittest.h"
#include "nv/pldm/task.h"
#include "corepdk/platforms/mcxn236/pldm-fd/src/pldm_wrap.h"
#include "nv/mctp/interface.h"
#include "nv/ipc/supervisor.h"
#include "nv/mctp/driver.h"

using namespace nv;
using namespace ut;
using namespace std::chrono_literals;
using namespace nv::pldm;


using arr8 = std::array<uint8_t, 72>;
using arr8_multi_recv = std::array<uint8_t, 256>;
using arr8_hdr_4k_recv = std::array<uint8_t, 32 + 4096>;

#define FIRMWARE 0xA
#define STR_TYPE_UNKNOWN 0x0
#define STR_TYPE_UTF16LE 0x4
#define PLDM_CC_SUCCESS 0x0
#define PLDM_CC_ERROR 0x1
#define PLDM_CC_ERR_INVALID_DATA 0x2
#define PLDM_CC_ERR_INVALID_LENGTH 0x3

#define PLDMFW_COMP_COMPATIBILITY_RESPONSE_CAN_BE_UPDATE 0x0
#define PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE 0x1

#define PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_CAN_BE_UPDATE 0x0
#define PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT 0x6
#define PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP 0x9

#define PLDMFW_REQUEST_FW_DATA_CMD 0x15
#define PLDMFW_REQUEST_FW_DATA_CMD_FW_OFFSET_IDX 12
#define PLDMFW_REQUEST_OP_CODE_IDX 11

class pldmbasetest : public ut::Fixture
{
public:
    void setup() override
    {
        auto& mctptask = nv::ipc::Supervisor::inst().task(ipc::TaskId::Mctp);
        // suspend mctp task
        mctptask.suspend();
        // clean up queue
        auto&                queue = ipc::Queue::make(ipc::QueueId::MctpPldmRequest);
        ipc::Queue::Item     item_recv(result.begin(), result.end());
        (void) queue.recv(item_recv, 500ms);

        recv_mctp_cmd();

    }

    void teardown() override
    {
        auto& mctptask = nv::ipc::Supervisor::inst().task(ipc::TaskId::Mctp);
        mctptask.resume();
    }

public:
    std::array<uint8_t, 256> result;

    uint32_t toDword(uint8_t* data) {
        return data[0] | (data[1]<<8UL) | (data[2]<<16UL) | (data[3]<<24UL);
    }
    uint16_t toWord(uint8_t* data) {
        return data[0] | (data[1]<<8UL);
    }
    void fromDword(uint32_t dw, uint8_t* data) {
        data[0] = dw & 0x0ff;
        data[1] = (dw>>8) & 0xff;
        data[2] = (dw>>16) & 0xff;
        data[3] = (dw>>24) & 0xff;
    }
    void fromWord(uint16_t dw, uint8_t* data) {
        data[0] = dw & 0x0ff;
        data[1] = (dw>>8) & 0xff;
    }

    template <typename T>
    void dumpWithCompareBuffers(const T* expect, size_t size1, const T* result) {

        for (size_t i=0; i<size1; i++) {
            if (expect[i] != result[i])
                nv::info("i = %d exp = %02x res = %02x", i, expect[i], result[i]);
        }
    }

    // Function template to print the elements of a std::array
    template <typename T, std::size_t N>
    void dumpBuffer(const std::array<T, N>& buffer) {
        nv::info("0x00: ");
        for (size_t i=0; i<16; i++) {
            if (i && (i % 16 == 0)) nv::info("\n0x%02x: ", i);
            nv::info("%02x ", buffer[i]);
        }
        nv::info("\n");
    }

    template <typename T, std::size_t N, std::size_t M>
    std::array<T, N> insert_into_array(const std::array<T, N>& arr, std::size_t pos, const std::array<T, M>& insert_arr) {
        std::array<T, N> new_arr;

        // Copy elements before the position
        std::copy(arr.begin(), arr.begin() + pos, new_arr.begin());

        // Insert the new array elements
        std::copy(insert_arr.begin(), insert_arr.end(), new_arr.begin() + pos);

        return new_arr;
    }

    bool sendRecv(arr8& send, const arr8& expect, bool compare=true, bool skipheaders=true, uint8_t iid=0) {
        //       Mctp priv ------------| MCTP -----------------| Pldm| RqD | Typ | Cmd...
        arr8 req{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00};
        //       Mctp priv ------------| MCTP -----------------| Pldm| RqD | Typ | Cmd...
        arr8 res{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};

        auto req_with_hdr = insert_into_array(req, 11, send);
        auto res_with_hdr = insert_into_array(res, 11, expect);

        req_with_hdr[9] = 0x80 | iid;
        res_with_hdr[9] = iid;
        //dumpBuffer(req);

        nv::mctp::Packet Tx_Pkt;
        memcpy(&Tx_Pkt.priv, req_with_hdr.data(), NV_PLDM_BASE_RX_QUEUE_SIZE);
        // @todo hard code
        Tx_Pkt.priv.packet_length = 64;
        auto status = nv::pldm::Task::pldm_tx(Tx_Pkt);
        if (status == nv::pldm::Status::Ok) {

            // receive
            ipc::Queue::Item     item_recv(result.begin(), result.end());
            auto&                queue = ipc::Queue::make(ipc::QueueId::MctpPldmRequest);

            recv_mctp_cmd();
            ensure::is_eq(queue.recv(item_recv, 500ms), ipc::Queue::Status::Ok);
            //dumpBuffer(result);

            auto ret = compare
                ? std::mismatch(res_with_hdr.begin() + (skipheaders ? 11 : 0),
                                res_with_hdr.end(),
                                result.begin() + (skipheaders ? 11 : 0)).first
                                == res_with_hdr.end()
                : true;

            if (!ret) {
                dumpWithCompareBuffers(res_with_hdr.data(), 72, result.data());
            }

            return ret;
        } else {
            nv::info("pldm_tx fail %d\n", status);
        }
        return false;
    }

    void recv_mctp_cmd() {
        nv::mctp::Driver::Command Cmd;
        auto          cmd_item  = ipc::Queue::Item(std::bit_cast<uint8_t*>(&Cmd),
                                                   sizeof(nv::mctp::Driver::Command));
        auto&         cmd_queue = ipc::Queue::make(ipc::QueueId::MctpCmd);

        (void) cmd_queue.recv(cmd_item, 1s);
    }


};

// Indices to _fixture.result skip RdQ,Type and start at Command byte
TEST_F(pldmbasetest, invalid)
{
    arr8 req{0xFF, 0xFF};
    arr8 res{0xFF, 0x05};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

TEST_F(pldmbasetest, set_tid)
{
    arr8 req{0x01, 0x10};
    arr8 res{0x01, 0x00};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

TEST_F(pldmbasetest, get_tid)
{
    arr8 req{0x02};
    arr8 res{0x02, 0x00, 0x10};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

TEST_F(pldmbasetest, get_pldm_version_1)
{
    arr8 req{0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
    arr8 res{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0xF0, 0xF0, 0xF1, 0xFB, 0x8F, 0x86, 0x4A};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

TEST_F(pldmbasetest, get_pldm_version_2)
{
    arr8 req{0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05};
    arr8 res{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0xF0, 0xF0, 0xF1, 0xFB, 0x8F, 0x86, 0x4A};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

/* INVALID_TRANSFER_OPERATION_FLAG */
TEST_F(pldmbasetest, get_pldm_version_negative_1)
{
    arr8 req{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    arr8 res{0x03, 0x81};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

/* INVALID_PLDM_TYPE_IN_REQUEST_DATA */
TEST_F(pldmbasetest, get_pldm_version_negative_2)
{
    arr8 req{0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF};
    arr8 res{0x03, 0x83};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

TEST_F(pldmbasetest, get_pldm_types)
{
    arr8 req{0x04};
    arr8 res{0x04, 0x00,
        0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    ensure::is_eq(fixture.sendRecv(req, res), true);
};

TEST_F(pldmbasetest, get_pldm_commands)
{
    arr8 req{0x05, 0x00, 0x00, 0xF0, 0xF0, 0xF1};
    arr8 res{
        0x05, 0x00,
        0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

/* INVALID_PLDM_TYPE_IN_REQUEST_DATA */
TEST_F(pldmbasetest, get_pldm_commands_negative_1)
{
    arr8 req{0x05, 0xFF, 0x00, 0xF0, 0xF0, 0xF1};
    arr8 res{0x05, 0x83};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

/* INVALID_PLDM_VERSION_IN_REQUEST_DATA */
TEST_F(pldmbasetest, get_pldm_commands_negative_2)
{
    arr8 req{0x05, 0x00, 0xF9, 0xF9, 0xF9, 0x00};
    arr8 res{0x05, 0x84};
    ensure::is_eq(fixture.sendRecv(req, res), true);
};

// pldm wrap test
class pldmtest : public ut::Fixture
{
public:
    void setup() override
    {
        auto& mctptask = nv::ipc::Supervisor::inst().task(ipc::TaskId::Mctp);
        // suspend mctp task
        mctptask.suspend();
        // clean up queue
        auto&                queue = ipc::Queue::make(ipc::QueueId::MctpPldmRequest);
        ipc::Queue::Item     item_recv(result.begin(), result.end());
        (void) queue.recv(item_recv, 500ms);

        recv_mctp_cmd();

    }

    void teardown() override
    {
        auto& mctptask = nv::ipc::Supervisor::inst().task(ipc::TaskId::Mctp);
        mctptask.resume();
    }

public:

    constexpr static uint16_t component_id = 0xff02;

    std::array<uint8_t, 256> result;
    uint8_t _rqd;

    uint32_t fw_image_size = 0x0;
    uint32_t remaining_fw_size = 0x0;

    uint32_t toDword(uint8_t* data) {
        return data[0] | (data[1]<<8UL) | (data[2]<<16UL) | (data[3]<<24UL);
    }
    uint16_t toWord(uint8_t* data) {
        return data[0] | (data[1]<<8UL);
    }
    void fromDword(uint32_t dw, uint8_t* data) {
        data[0] = dw & 0x0ff;
        data[1] = (dw>>8) & 0xff;
        data[2] = (dw>>16) & 0xff;
        data[3] = (dw>>24) & 0xff;
    }
    void fromWord(uint16_t dw, uint8_t* data) {
        data[0] = dw & 0x0ff;
        data[1] = (dw>>8) & 0xff;
    }

    nv::mctp::Packet& from(arr8_hdr_4k_recv& arr) { return *std::bit_cast<nv::mctp::Packet*>(&arr[0]); }

    std::array<uint8_t, 2> U8toAsciiArray(uint8_t byte) {
        std::array<uint8_t, 2> asciiArray;

        uint8_t tmp = byte;
        // within 0 .. 99
        tmp = tmp % 100;

        // Convert the byte to a decimal number and then to a 2-character string
        uint8_t tens = (tmp / 10) + '0';  // Get the tens digit and convert to ASCII
        uint8_t ones = (tmp % 10) + '0';  // Get the ones digit and convert to ASCII

        // Fill the array
        asciiArray[0] = tens;
        asciiArray[1] = ones;

        return asciiArray;
    }

    std::array<uint8_t, 4> U16toAsciiArray(uint16_t value) {
        std::array<uint8_t, 4> asciiArray;

        uint16_t tmp = value;

        // within 0 .. 9999
        tmp = tmp % 10000;

        // Convert each digit to its ASCII equivalent
        asciiArray[0] = (tmp / 1000) % 10 + '0';  // Thousands digit
        asciiArray[1] = (tmp / 100) % 10 + '0';   // Hundreds digit
        asciiArray[2] = (tmp / 10) % 10 + '0';    // Tens digit
        asciiArray[3] = tmp % 10 + '0';           // Ones digit

        return asciiArray;
    }

    void loadFirmware(uint32_t length) {

        //dumpBuffer(firmware);
    }

    void ignoreMessage() {
        recv({0x00}, false);
    }

    template <typename T>
    void dumpWithCompareBuffers(const T* expect, size_t size1, const T* result) {

        for (size_t i=11; i<size1; i++) {
            if (expect[i] != result[i]) {
                nv::info("i = %d exp = %02x res = %02x mismatch!!", i - 11, expect[i], result[i]);
            }
            else {
                nv::info("i = %d exp = %02x res = %02x", i - 11, expect[i], result[i]);
            }
        }
    }

    // Function template to print the elements of a std::array
    template <typename T, std::size_t N>
    void dumpBuffer(const std::array<T, N>& buffer) {
        nv::info("0x00: ");
        for (size_t i=0; i<16; i++) {
            if (i && (i % 16 == 0)) nv::info("\n0x%02x: ", i);
            nv::info("%02x ", buffer[i]);
        }
        nv::info("\n");
    }

    template <typename T, std::size_t N, std::size_t M>
    std::array<T, N> insert_into_array(const std::array<T, N>& arr, std::size_t pos, const std::array<T, M>& insert_arr) {
        std::array<T, N> new_arr = {};

        std::size_t elements_to_copy = std::min(M, N - pos);
        // Copy elements before the position
        std::copy(arr.begin(), arr.begin() + pos, new_arr.begin());

        // Insert the new array elements
        std::copy(insert_arr.begin(), insert_arr.begin() + elements_to_copy, new_arr.begin() + pos);

        return new_arr;
    }

    bool send(arr8& send, uint8_t iid=0) {
        //       Mctp priv ------------| MCTP -----------------| Pldm| RqD | Typ | Cmd...
        arr8 req{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x05};

        auto req_with_hdr = insert_into_array(req, 11, send);

        req_with_hdr[9] = 0x80 | iid;

        nv::mctp::Packet Tx_Pkt;
        memcpy(&Tx_Pkt.priv, req_with_hdr.data(), NV_PLDM_BASE_RX_QUEUE_SIZE);
        // @todo hard code
        Tx_Pkt.priv.packet_length = 64;
        auto status = nv::pldm::Task::pldm_tx(Tx_Pkt);
        if (status != nv::pldm::Status::Ok) {
            nv::info("pldm_tx fail %d\n", status);
            return false;
        }

        return true;
    }

    bool send_multi(arr8& send, uint16_t length, uint8_t iid = 0, uint32_t payload = NV_PLDM_MAX_PAYLOAD_SIZE) {
        //                   Mctp priv ------------| MCTP -----------------| Pldm| RqD | Typ | Cmd...
        arr8_hdr_4k_recv req{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x05};

        auto req_with_hdr = insert_into_array(req, 11, send);

        req_with_hdr[9] = 0x80 | iid;

        nv::mctp::Packet& Tx_Pkt = from(req);
        if (remaining_fw_size >= payload) {
            memcpy(&Tx_Pkt.priv, req_with_hdr.data(), NV_PLDM_RESERVE_HEADER_SIZE + payload);
            remaining_fw_size -= payload;
        }
        else if (remaining_fw_size > 0 and remaining_fw_size < payload) {
            memcpy(&Tx_Pkt.priv, req_with_hdr.data(), NV_PLDM_RESERVE_HEADER_SIZE + remaining_fw_size);
            length = length - (payload - remaining_fw_size);
            remaining_fw_size = 0;
        }
        else {
            nv::error("send fw data but remaining_fw_size = 0\n");
            memcpy(&Tx_Pkt.priv, req_with_hdr.data(), NV_PLDM_RESERVE_HEADER_SIZE + NV_PLDM_MAX_PAYLOAD_SIZE);
        }

        Tx_Pkt.priv.packet_length = length;
        auto status = nv::pldm::Task::pldm_tx(Tx_Pkt);
        if (status != nv::pldm::Status::Ok) {
            nv::info("pldm_tx fail %d\n", status);
            return false;
        }

        return true;
    }

    void recv_mctp_cmd() {
        nv::mctp::Driver::Command Cmd;
        auto          cmd_item  = ipc::Queue::Item(std::bit_cast<uint8_t*>(&Cmd),
                                                   sizeof(nv::mctp::Driver::Command));
        auto&         cmd_queue = ipc::Queue::make(ipc::QueueId::MctpCmd);

        (void) cmd_queue.recv(cmd_item, 1s);
        if(Cmd.cmd == (uint16_t)nv::mctp::Driver::CmdCode::RotStateInfoChange) {
            (void) cmd_queue.recv(cmd_item, 1s);
        }
    }

    bool recv(arr8& expect, bool compare=true, bool skipheaders=true, uint8_t iid=0) {
        //       Mctp priv ------------| MCTP -----------------| Pldm| RqD | Typ | Cmd...
        arr8 res{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x05};

        auto res_with_hdr = insert_into_array(res, 11, expect);
        _rqd = res_with_hdr[9];

        // receive
        ipc::Queue::Item     item_recv(result.begin(), result.end());
        auto&                queue = ipc::Queue::make(ipc::QueueId::MctpPldmRequest);

        recv_mctp_cmd();
        if (queue.recv(item_recv, 5s) != ipc::Queue::Status::Ok) {
            nv::info("not recv\n");
            return false;
        }

        _rqd = result[9];

        auto ret = compare
                 ? std::mismatch(res_with_hdr.begin() + (skipheaders ? 11 : 0),
                                 res_with_hdr.end(),
                                 result.begin() + (skipheaders ? 11 : 0)).first
                                 == res_with_hdr.end()
                : true;
        if (!ret) {
            dumpWithCompareBuffers(res_with_hdr.data(), 72, result.data());
        }

        if (result[PLDMFW_REQUEST_OP_CODE_IDX] == PLDMFW_REQUEST_FW_DATA_CMD) {
            if (result[PLDMFW_REQUEST_FW_DATA_CMD_FW_OFFSET_IDX] == 0 && result[PLDMFW_REQUEST_FW_DATA_CMD_FW_OFFSET_IDX+1] == 0 && result[PLDMFW_REQUEST_FW_DATA_CMD_FW_OFFSET_IDX+2] == 0 && result[PLDMFW_REQUEST_FW_DATA_CMD_FW_OFFSET_IDX+3] == 0) {
                remaining_fw_size = fw_image_size;
            }
        }
        return ret;
    }

    bool recv(const arr8_multi_recv& expect, bool compare=true, bool skipheaders=true, uint8_t iid=0) {
        //       Mctp priv ------------| MCTP -----------------| Pldm| RqD | Typ | Cmd...
        arr8_multi_recv res{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05};

        auto res_with_hdr = insert_into_array(res, 11, expect);

        res_with_hdr[9] = iid;

        // receive
        auto&                queue = ipc::Queue::make(ipc::QueueId::MctpPldmRequest);
        ipc::Queue::Item item(result.begin(), result.begin() + queue.item_size());

        //dumpBuffer(result);

        recv_mctp_cmd();
        // auto&    multi_rx = from(_multi_pkt_buf);
        if (queue.recv(item, 5s) != ipc::Queue::Status::Ok) {
            nv::info("not recv\n");
            return false;
        }

        auto ret = compare
                 ? std::mismatch(res_with_hdr.begin() + (skipheaders ? 11 : 0),
                                 res_with_hdr.end(),
                                 result.begin() + (skipheaders ? 11 : 0)).first
                                 == res_with_hdr.end()
                : true;
        if (!ret) {
            dumpWithCompareBuffers(res_with_hdr.data(), 256, result.data());
        }

        return ret;
    }

    bool sendRecv(arr8& send, const arr8& expect, bool compare=true, bool skipheaders=true, uint8_t iid=0) {
        //       Mctp priv ------------| MCTP -----------------| Pldm| RqD | Typ | Cmd...
        arr8 req{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x05};
        //       Mctp priv ------------| MCTP -----------------| Pldm| RqD | Typ | Cmd...
        arr8 res{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05};

        auto req_with_hdr = insert_into_array(req, 11, send);
        auto res_with_hdr = insert_into_array(res, 11, expect);

        req_with_hdr[9] = 0x80 | iid;
        res_with_hdr[9] = iid;
        //dumpBuffer(req);

        nv::mctp::Packet Tx_Pkt;
        memcpy(&Tx_Pkt.priv, req_with_hdr.data(), NV_PLDM_BASE_RX_QUEUE_SIZE);
        // @todo hard code
        Tx_Pkt.priv.packet_length = 64;
        auto status = nv::pldm::Task::pldm_tx(Tx_Pkt);
        if (status == nv::pldm::Status::Ok) {

            // receive
            ipc::Queue::Item     item_recv(result.begin(), result.end());
            auto&                queue = ipc::Queue::make(ipc::QueueId::MctpPldmRequest);

            recv_mctp_cmd();
            if (queue.recv(item_recv, 5s) != ipc::Queue::Status::Ok) {
                nv::info("not recv\n");
                return false;
            }
            //dumpBuffer(result);

            auto ret = compare
                ? std::mismatch(res_with_hdr.begin() + (skipheaders ? 11 : 0),
                                res_with_hdr.end(),
                                result.begin() + (skipheaders ? 11 : 0)).first
                                == res_with_hdr.end()
                : true;

            if (!ret) {
                dumpWithCompareBuffers(res_with_hdr.data(), 72, result.data());
            }

            return ret;
        } else {
            nv::info("pldm_tx fail %d\n", status);
        }
        return false;
    }

    bool queryDeviceIdentifiers() {
        arr8 req{0x01};
        //                  CMD | CC  | Len ------------------| Cnt | inaType   | inaLen    | inaData --------------| uuidType  | uuidLen   |
        arr8_multi_recv res{0x01, 0x00, 0x43, 0x00, 0x00, 0x00, 0x07, 0x01, 0x00, 0x04, 0x00, 0x47, 0x16, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00,
        //       uuid -----------------------------------------------------------------------------------------|
                 0xb5, 0xf8, 0xee, 0x87, 0xf7, 0xaa, 0x4f, 0x1d, 0x9d, 0x9a, 0xb1, 0x7b, 0x34, 0x9f, 0x88, 0xc1,
                 //vid type| vid len   | vid       | did type  | did len   | did       | ssvid type| ssvid len
                 0x00, 0x00, 0x02, 0x00, 0xde, 0x10, 0x00, 0x01, 0x02, 0x00, 0xad, 0x2f, 0x01, 0x01, 0x02, 0x00,
                 //ssvid   | ssdid ty  | ssdid len | ssdid     |
                 0xde, 0x10, 0x02, 0x01, 0x02, 0x00, 0x00, 0x00,
        //       vndrType  | vndrLen   |title| len | str APSKU ------------------| sku id ---------------
                 0xff, 0xff, 0x0b, 0x00, 0x01, 0x05, 0x41, 0x50, 0x53, 0x4b, 0x55, 0x00, 0x00, 0x00, 0x00};

        ensure::is_eq(send(req), true);

        return recv(res);
    }

    bool getFirmwareParameters() {
        // @todo need to implement case for pending version
        arr8 req{0x02};
        //                  CMD | CC  | capabilities during   | comp count|ASCII|ver l|ASCII| verl|
        arr8_multi_recv res{0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x11, 0x00, 0x00};

        //            comp class       | comp id   | idx | comparision stamp     |ASCII| verl|
        arr8_multi_recv res2{0x0a, 0x00, 0x02, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11,
        //  release date ---------------------------------------|
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // pend comparision stamp       |ptyp | verl| release date -------------------------------  |
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //        comp act  | capabilities update   |
                  0x20, 0x00, 0x00, 0x00, 0x00, 0x00};

        // ver str  0     0     0     0     .     0     0     .     0     0     0     0     .     0     0     0     0
        arr8 ver{0x30, 0x30, 0x30, 0x30, 0x2e, 0x30, 0x30, 0x2e, 0x30, 0x30, 0x30, 0x30, 0x2e, 0x30, 0x30, 0x30, 0x30};

        // set version
        std::array<uint8_t, 2> U8_Array;
        std::array<uint8_t, 4> U16_Array;
        U16_Array = U16toAsciiArray(MCU_FW_MAJOR);
        std::copy (U16_Array.begin(), U16_Array.begin() + 4, ver.begin());
        U8_Array = U8toAsciiArray(MCU_FW_MINOR);
        std::copy (U8_Array.begin(), U8_Array.begin() + 2, ver.begin() + 5);
        U16_Array = U16toAsciiArray(MCU_FW_PATCH);
        std::copy (U16_Array.begin(), U16_Array.begin() + 4, ver.begin() + 8);
        U16_Array = U16toAsciiArray(MCU_FW_BUILD);
        std::copy (U16_Array.begin(), U16_Array.begin() + 4, ver.begin() + 13);

        // set component id
        res2[2] = component_id & 0xff;
        res2[3] = (component_id >> 8) & 0xff;

        // set stamp
        res2[5] = MCU_FW_BUILD & 0xff;
        res2[6] = MCU_FW_PATCH & 0xff;
        res2[7] = (MCU_FW_PATCH >> 8) & 0xff;
        res2[8] = MCU_FW_MINOR & 0xff;

        std::copy (ver.begin(), ver.begin() + 17, res.begin() + 12);
        std::copy (res2.begin(), res2.begin() + 39, res.begin() + 29);
        std::copy (ver.begin(), ver.begin() + 17, res.begin() + 68);

        ensure::is_eq(send(req), true);

        return recv(res);
    }

    bool getStatus(bool compare=false, uint8_t cs=0, uint8_t ps=0, uint8_t rc = 0, uint8_t pc = 0x65) {
        arr8 req{0x1B};
        //       CMD | CC  | Cur | Prev| Aux | Aux | %   | Res | Update Flags Enabled
        arr8 res{0x1B, 0x00, cs,   ps,   0x00, 0x00,  pc, 0x00, 0x00, 0x00, 0x00, 0x00};
        res[7] = rc;// reason code
        // idle
        if (cs == 0) {
            res[4] = 3;// Aux
        }
        // lc rdy_xfer
        else if (cs == 1 || cs == 2) {
            res[4] = 3;// Aux
        }
        // verify
        else if (cs == 4) {
            res[8] = 1; // update flags
            res[4] = 0;
        }
        // verify apply activate
        else if (cs == 5 || cs == 6) {
            res[4] = 1; // Aux
            res[8] = 1; // update flags
        }
        // download
        else {
            res[8] = 1; // update flags
            if (pc == 100)
                res[4] = 1;
            else
                res[4] = 3;
        }

        auto status = sendRecv(req, res);
        // TODO: proper testing here
        return status;
    }

    bool getStatus(int currentState, int previousState, int reason_code = 0, int percent = 0x65) {
        return getStatus(true, currentState, previousState, reason_code, percent);
    }

    bool requestUpdate(uint32_t maxTransferSize=0x1000, uint8_t complete_code=PLDM_CC_SUCCESS, uint8_t iid=0x0, uint8_t str_len=0x01, uint8_t comp_cnt=0x01) {
        // TODO: not fully implemented in driver
        //       CMD | Max Trans Size -------| N_Cmpnts      | OutS| PktDataLen|       Typ       | Len    | Str...
        arr8 req{0x10, 0x00, 0x00, 0x00, 0x00, comp_cnt, 0x00, 0x00, 0x00, 0x00, STR_TYPE_UTF16LE, str_len, 0x00};
        arr8 res;

        if (complete_code == 0x0) {
            //     CMD | CC  | FWDataLen | FDWillSeng
            res = {0x10, 0x00, 0x00, 0x00, 0x00};
            fromDword(maxTransferSize, &req[1]);
        }
        else {
            //     CMD | CC  |
            res = {0x10, complete_code};
        }

        return sendRecv(req, res, true, true, iid);
    }

    bool cancelUpdate(uint8_t complete_code=0x0) {
        // TODO: not full implemented in driver
        arr8 req{0x1D};
        arr8 res;
        if (complete_code == 0x00) {
            //     CMD | CC  | Comp| NonFunctioningComponentBitmap ----------------
            res = {0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        }
        else {
            res = {0x1D, complete_code};
        }
        return sendRecv(req, res);
    }

    // Learn Components ----------------------------------------------------------------

    bool passComponentTable(uint16_t id, uint8_t flag = 0x05,uint8_t complete_code = PLDM_CC_SUCCESS, uint8_t iid=0x0,
                            uint8_t cls = FIRMWARE, uint8_t idx = 0x0, uint8_t type = STR_TYPE_UTF16LE, uint8_t len = 0x03, uint8_t resp = 0x0, uint8_t rc = 0x0) {
        // TODO: not fully implemented in driver, Flg 0x05 == Start and End
        //       CMD | Flg | Cls      | Comp Id   | Idx | Comp Comparison Stamp | Typ | Len | Str
        arr8 req{0x13, flag, cls, 0x00, 0x00, 0x00,  idx, 0xFF, 0xFF, 0xFF, 0xFF, type,  len, 0x00, 0x00, 0x00};
        arr8 res;
        fromWord(id, &req[4]);
        //     CMD |          CC  | resp| rc
        res = {0x13, complete_code, resp, rc};
        return sendRecv(req, res, true, true, iid);
    }

    // Read Xfer -----------------------------------------------------------------

    bool updateComponent(uint16_t id, uint32_t len, uint8_t complete_code = PLDM_CC_SUCCESS,
                         uint8_t cpt = 0x0, uint8_t code = 0x0, uint32_t image_size = 0x0, 
                         uint8_t cls = FIRMWARE, uint8_t idx = 0x0, uint8_t type = STR_TYPE_UTF16LE, uint8_t str_len = 0x04) {
        loadFirmware(len);
        fw_image_size = len;
        remaining_fw_size = len;
        // ID=FF00 EC_FW, ID=FF01 SPI
        //       CMD | Comp Class| Comp ID   | Idx | Comp Comparison Stamp | Comp Image Size
        arr8 req{0x14,  cls, 0x00, 0x00, 0x00,  idx, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //     | Update Opt Flags -----| Typ |    Len | String----------------|
                 0x01, 0x00, 0x00, 0x00, type, str_len, 0x00, 0x00, 0x00, 0x00};

        // set length
        if(image_size == 0){
            //replace with len
            fromDword(len, &req[10]);
        }
        else{
            //replace with image size
            fromDword(image_size, &req[10]);
        }
        arr8 res;
        fromWord(id, &req[3]);
        if (complete_code == 0x00) {
            //     CMD | CC  | Cpt | Code| Opt ------------------| Est Time--
            res = {0x14, 0x00,  cpt, code, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
            // set id
            
        }
        else {
            res = {0x14, complete_code, cpt, code};
        }

        return sendRecv(req, res);
    }

    bool onRequestFirmwareData(bool failResponse=false, uint8_t cc=0x01, uint32_t payload = NV_PLDM_MAX_PAYLOAD_SIZE) {
        //       CMD | Offset ---------------| Length ---------------|
        arr8 req{0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        if (recv(req, false)) {

            arr8 res{0x15};
            if (failResponse) {
                res[1] = cc; // CC
            }
            else {
                res[1] = 0x00; // CC
            }
            // mctp hdr <4> + mst type <1> + pldm hdr <4>
            return send_multi(res, 4 + 1 + 4 + payload,(_rqd & 0x7f), payload);
        }
        return false;
    }

    bool onRequestFirmwareDataUntilTransferComplete(uint32_t payload = NV_PLDM_MAX_PAYLOAD_SIZE) {
        arr8 req{0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        bool passing = true;
        while (passing) {
            passing &= recv(req, false);

            if (passing && result[11] == 0x15) {
                arr8 res{0x15};
                passing &= send_multi(res, 4 + 1 + 4 + payload,(_rqd & 0x7f), payload);
                //dumpBuffer(res);
            }
            else if (passing && result[11] == 0x016) { // transfer complete
                passing &= (result[12] == 0x00);
                break;
            }
        }
        return passing;
    }

    void send_auth_success_request()
    {
        pldm::Request request{
            .type   = nv::pldm::RequestType::AuthMcuFinish,
            .length = 1,
            .rsv1   = 0,
            .rsv2   = 0,
        };
        request.buffer[0] = 0;
        nv::pldm::Task::to_pldm(ipchandler::Id::Spdm, request);
    }

    bool transferComplete(bool auth_request=true) {
        arr8 res{0x16, 0x00};
        bool ret = send (res);
        if (auth_request) send_auth_success_request();
        return ret;
    }

    bool ontransferComplete(bool reply=true, uint8_t cc=0x0, bool auth_request=true){
        arr8 req{0x16, cc};
        if (recv(req, true, true)) {
            arr8 res{0x16, 0x00};
            bool ret = false;
            if (reply) {
                ret = send(res);
                if (auth_request) send_auth_success_request();
                return ret;
            }
            else {
                return true;
            }
        }
        return false;
    }

    bool onVerifyComplete(bool reply=true, uint8_t cc=0x0) {
        arr8 req{0x17, cc};
        if (recv(req, true, true)) {
            arr8 res{0x17, 0x00};
            return reply ? send(res) : true;
        }
        return false;
    }

    bool onApplyComplete(bool reply=true) {
        arr8 req{0x18, 0x01, 0x20, 0x00};
        if (recv(req, true, true)) {
            arr8 res{0x18, 0x00}; // require reboot bit
            return reply ? send(res) : true;
        }
        return false;
    }

#if 0
    bool onRequestFirmwareDataMultipacket() {
        //       SPI-------------------| MCTP -----------------| Pldm| RqD | Typ | Cmd | CC
        arr8 hdr{0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x01, 0x80, 0x05, 0x15, 0x00};
        arr8 req{0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        //         MCTP Packet Size - Header - PLDM header - cmd/cc

        bool passing = true;
        while (passing) {
            passing &= recv(req, false);
            hdr[9] = (_rqd & 0x7f);
            auto offset = toDword(&result[1]);
            auto length = toDword(&result[5]);
            uint32_t remain = 0;
            auto chunk = 68 - 4 - 3 - 2;
            if (passing && result[0] == 0x15) {
                int seq = 0;
                auto itr = firmware.begin() + offset;
                remain = length;
                for (int i=0; i < length; i += chunk) {
                    if (i == chunk) chunk += 5;
                    arr8 res(68 + 4, 0);
                    //printf("chunk %d remain %d\n", chunk, remain);
                    if (remain < chunk) {
                        chunk = remain;
                        res.resize(8 + remain);
                    }
                    // update priv hdr lens
                    hdr[1] = chunk + 4;
                    std::copy(hdr.begin(), hdr.end() - (i == 0 ? 0 : 5), res.begin());

                    // set SOM EOM SEQ TO TAG
                    res[7] = 0;
                    if (i == 0) { res[7] = 0b10001000; } // SOM
                    else if (i < length-chunk) { res[7] = 0b00001000; } // som eom

                    if (i + chunk >= length) { // EOM
                        res[7] |= 0b01001000;
                        chunk = length - i;
                    }
                    res[7] |= (seq++ & 0b11) << 4; // set sequence

                    // zero pad
                    auto end = itr + chunk;
                    if (end > firmware.end()) { end = firmware.end(); }

                    std::copy(itr, end, res.begin() + hdr.size() - (i == 0 ? 0 : 5));

                    //dumpBuffer(res);
                    passing &= spb_ap_send(res.size(), res.data()) == SPB_AP_OK;

                    if (!passing) break;
                    itr += chunk;
                    remain -= chunk;
                    printHeartbeat(i);
                }
            }
            else if (passing && result[0] == 0x16) { // Transfer Complete
                passing &= (result[1] == 0x00);
                break;
            }
            else {
                passing = false;
            }
        }
        printf("\r");
        return passing;
    }

#endif

    bool cancelUpdateComponent(uint8_t complete_code=0x0) {
        arr8 req{0x1C};
        //       CMD | CC  |
        arr8 res{0x1C, complete_code};

        return sendRecv(req, res);

    }

    bool activateFirmware(bool selfContained=false, uint8_t complete_code=0x0) {
        //       CMD
        arr8 req{0x1A, static_cast<uint8_t>(selfContained?0x01:0x00)};
        arr8 res;
        if (complete_code == 0x00) {
            //     CMD | CC  | Est Time
            res = {0x1A, 0x00, static_cast<uint8_t>(selfContained?0xb4:0x00), 0x00};
        }
        else {
            res = {0x1A, complete_code};
        }
        return sendRecv(req, res);
    }


};

// Indices to _fixture.result skip RdQ,Type and start at Command byte
TEST_F(pldmtest, state_idle)
{
    ensure::is_eq(fixture.getStatus(), true);
    ensure::is_eq(fixture.queryDeviceIdentifiers(), true);
    ensure::is_eq(fixture.getFirmwareParameters(), true);
    ensure::is_eq(fixture.requestUpdate(0, PLDM_CC_ERR_INVALID_DATA, 0, 0, 1), true); // invalid ver str len
    ensure::is_eq(fixture.requestUpdate(0, PLDM_CC_ERR_INVALID_DATA, 0, 1, 4), true); // invalid comp cnt
    ensure::is_eq(fixture.requestUpdate(0, PLDM_CC_ERR_INVALID_DATA, 0, 1, 0), true); // invalid comp cnt
    ensure::is_eq(fixture.requestUpdate(), true); // leave idle
    ensure::is_eq(fixture.requestUpdate(), true); // non-idempotent
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 1, 2), true); // check status again 2 for cancel update
    // check error complete code
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x05, 0x80), true);
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x1000, 0x80), true);
    ensure::is_eq(fixture.activateFirmware(true, 0x80), true);
    ensure::is_eq(fixture.getStatus(0, 1, 2), true); // check status again 2 for cancel update
    ensure::is_eq(fixture.cancelUpdateComponent(0x80), true);
    ensure::is_eq(fixture.cancelUpdate(0x80), true);
};

TEST_F(pldmtest, state_learn_components)
{
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state

    //lc state
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 1, 2), true); // check status again

    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 1, 2), true); // check status again

    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x01), true);
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 1, 2), true); // check status again

    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x02), true);
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 1, 2), true); // check status again

    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x04), true);
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 2, 2), true); // check status again

    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // non-idempotent
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 2, 2), true); // check status again

    // check error complete code
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.requestUpdate(0x0, 0x84, 0x1), true);
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 4096, 0x84), true);
    ensure::is_eq(fixture.activateFirmware(true, 0x84), true);
    ensure::is_eq(fixture.cancelUpdateComponent(0x84), true);
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
};

TEST_F(pldmtest, state_learn_components_invalid_check)
{
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(1023, 0x01, PLDM_CC_SUCCESS, 0, FIRMWARE, 0, STR_TYPE_UTF16LE, 3,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT), true); // invalid id
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x01, PLDM_CC_ERR_INVALID_DATA, 0, 0, 0, STR_TYPE_UTF16LE, 3,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT), true); // invalid class
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x01, PLDM_CC_ERR_INVALID_DATA, 0, FIRMWARE, 1, STR_TYPE_UTF16LE, 3,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT), true); // invalid class idx
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 1, 2), true); // check status again

    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x01, PLDM_CC_SUCCESS, 0, FIRMWARE, 0, STR_TYPE_UNKNOWN, 3,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_CAN_BE_UPDATE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_CAN_BE_UPDATE), true); // unknown ver str type
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x01, PLDM_CC_ERR_INVALID_DATA, 0, FIRMWARE, 0, STR_TYPE_UTF16LE, 0,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT), true); // invalid ver str len
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 1, 2), true); // check status again
};

TEST_F(pldmtest, state_ready_xfer)
{
    ensure::is_eq(fixture.requestUpdate(), true);       // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);  // TODO: not complete in driver

    //rdy xfer state

    ensure::is_eq(fixture.getStatus(2, 1, 2), true);  // should be in xfer

    ensure::is_eq(fixture.updateComponent(fixture.component_id, 4096), true);  // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareData(), true);  // check for a response
    fixture.ignoreMessage();                  // ignore the next request

    //dl state
    ensure::is_eq(fixture.getStatus(3, 2, 2, 100), true);  // we should be in download state percent is 0
    ensure::is_eq(fixture.cancelUpdate(), true);
    ensure::is_eq(fixture.getStatus(0, 3, 2), true);  //  back to idle

    ensure::is_eq(fixture.requestUpdate(), true);       // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);  // TODO: not complete in driver
    ensure::is_eq(fixture.activateFirmware(true, 0x80), true);
    ensure::is_eq(fixture.getStatus(2, 1, 2), true);
    ensure::is_eq(fixture.cancelUpdate(), true);
    ensure::is_eq(fixture.getStatus(0, 2, 2), true);

    // check update component with wrong comp id
    ensure::is_eq(fixture.requestUpdate(), true);       // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);  // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(1, 1000, 0x0, PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP), true);  // SHD SPI
    ensure::is_eq(fixture.updateComponent(1, 1000, 0x0, PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP), true);  // non idempotent
    ensure::is_eq(fixture.cancelUpdate(), true);
    ensure::is_eq(fixture.getStatus(0, 2, 2), true);

    // check error complete code
    ensure::is_eq(fixture.requestUpdate(), true);       // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);  // enter ready xfer
    ensure::is_eq(fixture.requestUpdate(0x0, 0x81), true);
    ensure::is_eq(fixture.passComponentTable(fixture.component_id, 0x5, 0x84, 0x01), true);
    ensure::is_eq(fixture.cancelUpdateComponent(0x84), true);
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
};

TEST_F(pldmtest, state_ready_xfer_invalid_check)
{
    // check update component with wrong comp class
    ensure::is_eq(fixture.requestUpdate(), true);       // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 1000, PLDM_CC_ERR_INVALID_DATA,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP, 0, 0), true);
    ensure::is_eq(fixture.cancelUpdate(), true);
    ensure::is_eq(fixture.getStatus(0, 2, 2), true);

    // check update component with wrong comp class idx
    ensure::is_eq(fixture.requestUpdate(), true);       // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 1000, PLDM_CC_ERR_INVALID_DATA,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP, 0, FIRMWARE, 1), true);
    ensure::is_eq(fixture.cancelUpdate(), true);
    ensure::is_eq(fixture.getStatus(0, 2, 2), true);

    // check update component with wrong ver str len
    ensure::is_eq(fixture.requestUpdate(), true);       // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 1000, PLDM_CC_ERR_INVALID_DATA,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP, 0, FIRMWARE, 0, STR_TYPE_UTF16LE, 0), true);
    ensure::is_eq(fixture.cancelUpdate(), true);
    ensure::is_eq(fixture.getStatus(0, 2, 2), true);

    // check update component with wrong img size
    ensure::is_eq(fixture.requestUpdate(), true);       // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 1000, PLDM_CC_ERR_INVALID_DATA,
                  PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE, PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP, 0xFFFFFFFF), true);
    ensure::is_eq(fixture.cancelUpdate(), true);
    ensure::is_eq(fixture.getStatus(0, 2, 2), true);
};

TEST_F(pldmtest, state_download)
{
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true); // SHD SPI

    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.getStatus(3, 2, 2, 100), true); // in Download
    ensure::is_eq(fixture.cancelUpdate(), true);  // return to idle
    ensure::is_eq(fixture.getStatus(0, 3, 2), true); // in Idle

    // Go back to download
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2100), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    fixture.ignoreMessage(); // Ignore verify complete
    ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true); // in Verify

    //verify state
    ensure::is_eq(fixture.queryDeviceIdentifiers(), true);
    ensure::is_eq(fixture.getFirmwareParameters(), true);

    ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    ensure::is_eq(fixture.requestUpdate(), true); //enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true);
    ensure::is_eq(fixture.onRequestFirmwareData(true, 0x01), true);
    ensure::is_eq(fixture.ontransferComplete(false, 0x0A), true);
    ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    ensure::is_eq(fixture.getStatus(0, 3, 2), true); //in

    ensure::is_eq(fixture.requestUpdate(), true); //enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true);
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true);
    ensure::is_eq(fixture.onRequestFirmwareData(true, 0x89), true);
    // wait the timeout for retry request firmware data
    std::this_thread::sleep_for(5s);
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    ensure::is_eq(fixture.onVerifyComplete(), true);
    ensure::is_eq(fixture.onApplyComplete(), true);
    ensure::is_eq(fixture.getStatus(2, 5, 2), true);
    ensure::is_eq(fixture.activateFirmware(true, 0x8c), true);    // enter idle state
    ensure::is_eq(fixture.getStatus(0, 6, 1), true);
};

TEST_F(pldmtest, state_download_diff_payload)
{
    // 0x800
    ensure::is_eq(fixture.requestUpdate(NV_PLDM_2K_PAYLOAD_SIZE), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2100), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(NV_PLDM_2K_PAYLOAD_SIZE), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    fixture.ignoreMessage(); // Ignore verify complete
    ensure::is_eq(fixture.getStatus(4, 3, 1, 0), true); // in Verify  --> set idle reason to activate fw due to prev test

    ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    // Remove following testcases since exceeds rate limit
    // // 0x400
    // ensure::is_eq(fixture.requestUpdate(NV_PLDM_1K_PAYLOAD_SIZE), true); // enter LC state
    // ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    // ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true); // SHD SPI
    // ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(NV_PLDM_1K_PAYLOAD_SIZE), true);
    // ensure::is_eq(fixture.transferComplete(), true); // reply
    // fixture.ignoreMessage(); // Ignore verify complete
    // ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true); // in Verify  --> set idle reason to cancel update

    // ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    // ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    // // 0x200
    // ensure::is_eq(fixture.requestUpdate(NV_PLDM_512_PAYLOAD_SIZE), true); // enter LC state
    // ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    // ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true); // SHD SPI
    // ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(NV_PLDM_512_PAYLOAD_SIZE), true);
    // ensure::is_eq(fixture.transferComplete(), true); // reply
    // fixture.ignoreMessage(); // Ignore verify complete
    // ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true); // in Verify  --> set idle reason to cancel update

    // ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    // ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    // // 0x100
    // ensure::is_eq(fixture.requestUpdate(NV_PLDM_256_PAYLOAD_SIZE), true); // enter LC state
    // ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    // ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true); // SHD SPI
    // ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(NV_PLDM_256_PAYLOAD_SIZE), true);
    // ensure::is_eq(fixture.transferComplete(), true); // reply
    // fixture.ignoreMessage(); // Ignore verify complete
    // ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true); // in Verify  --> set idle reason to cancel update

    // ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    // ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    // // 0x80
    // ensure::is_eq(fixture.requestUpdate(NV_PLDM_128_PAYLOAD_SIZE), true); // enter LC state
    // ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    // ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true); // SHD SPI
    // ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(NV_PLDM_128_PAYLOAD_SIZE), true);
    // ensure::is_eq(fixture.transferComplete(), true); // reply
    // fixture.ignoreMessage(); // Ignore verify complete
    // ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true); // in Verify  --> set idle reason to cancel update

    // ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    // ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    // // 0x40
    // ensure::is_eq(fixture.requestUpdate(NV_PLDM_64_PAYLOAD_SIZE), true); // enter LC state
    // ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    // ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true); // SHD SPI
    // ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(NV_PLDM_64_PAYLOAD_SIZE), true);
    // ensure::is_eq(fixture.transferComplete(), true); // reply
    // fixture.ignoreMessage(); // Ignore verify complete
    // ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true); // in Verify  --> set idle reason to cancel update

    // ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    // ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    // // 0x20
    // ensure::is_eq(fixture.requestUpdate(NV_PLDM_32_PAYLOAD_SIZE), true); // enter LC state
    // ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    // ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x2000), true); // SHD SPI
    // ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(NV_PLDM_32_PAYLOAD_SIZE), true);
    // ensure::is_eq(fixture.transferComplete(), true); // reply
    // fixture.ignoreMessage(); // Ignore verify complete
    // ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true); // in Verify  --> set idle reason to cancel update

    // ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    // ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    // 0x20
    ensure::is_eq(fixture.requestUpdate(NV_PLDM_32_PAYLOAD_SIZE), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x820), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(NV_PLDM_32_PAYLOAD_SIZE), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    fixture.ignoreMessage(); // Ignore verify complete
    ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true); // in Verify  --> set idle reason to cancel update

    ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    ensure::is_eq(fixture.getStatus(0, 4, 2), true);
};


TEST_F(pldmtest,state_verify)
{
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x1000), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    ensure::is_eq(fixture.onVerifyComplete(false), true);
    ensure::is_eq(fixture.getStatus(4, 3, 2, 0), true);
    ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    ensure::is_eq(fixture.getStatus(0, 4, 2), true);

    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x1000), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    ensure::is_eq(fixture.onVerifyComplete(), true);
    fixture.ignoreMessage(); // Ignore apply complete

    //apply state
    ensure::is_eq(fixture.queryDeviceIdentifiers(), true);
    ensure::is_eq(fixture.getFirmwareParameters(), true);

    ensure::is_eq(fixture.getStatus(5, 4, 2), true);
    ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    ensure::is_eq(fixture.getStatus(0, 5, 2), true); //

    //TODO: incomplete
};

TEST_F(pldmtest,state_apply)
{
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x1000), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    ensure::is_eq(fixture.onVerifyComplete(), true);
    ensure::is_eq(fixture.onApplyComplete(false), true);
    ensure::is_eq(fixture.getStatus(5, 4, 2), true);
    ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
    ensure::is_eq(fixture.getStatus(0, 5, 2), true);

    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x1000), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    ensure::is_eq(fixture.onVerifyComplete(), true);
    ensure::is_eq(fixture.onApplyComplete(), true);
    ensure::is_eq(fixture.getStatus(2, 5, 2), true);
    ensure::is_eq(fixture.activateFirmware(), true);    // enter idle state
    ensure::is_eq(fixture.activateFirmware(), true);    // non-idempotent
    ensure::is_eq(fixture.getStatus(0, 6, 1), true);

    // check activate contained
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x1000), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    ensure::is_eq(fixture.onVerifyComplete(), true);
    ensure::is_eq(fixture.onApplyComplete(), true);
    ensure::is_eq(fixture.getStatus(2, 5, 1), true);
    ensure::is_eq(fixture.activateFirmware(true, 0x8c), true);    // enter idle state
    ensure::is_eq(fixture.getStatus(0, 6, 1), true);
    //TODO: incomplete
};

TEST_F(pldmtest, timeout_lc)
{
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    // wait the timeout for retry request firmware data
    std::this_thread::sleep_for(75s);
    ensure::is_eq(fixture.getStatus(0, 1, 3), true);
};

TEST_F(pldmtest, timeout_verify)
{
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x1000), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(), true); // reply
    for (int i = 0; i <= 3; ++i) {
        fixture.ignoreMessage();                  // ignore the next request
        std::this_thread::sleep_for(5s);
    }
    std::this_thread::sleep_for(65s);
    ensure::is_eq(fixture.getStatus(0, 4, 6), true);

    // auth request missing
    ensure::is_eq(fixture.requestUpdate(), true); // enter LC state
    ensure::is_eq(fixture.passComponentTable(fixture.component_id), true); // TODO: not complete in driver
    ensure::is_eq(fixture.updateComponent(fixture.component_id, 0x1000), true); // SHD SPI
    ensure::is_eq(fixture.onRequestFirmwareDataUntilTransferComplete(), true);
    ensure::is_eq(fixture.transferComplete(false), true); // reply
    std::this_thread::sleep_for(32s);
    ensure::is_eq(fixture.onVerifyComplete(true, 0x01), true);
    ensure::is_eq(fixture.cancelUpdate(), true); // return to idle
};
