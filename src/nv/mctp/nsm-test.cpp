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
#include <cstring>

#include "nv/mctp/control.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/enums.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;
using namespace mctp;

using arr8_req = std::array<uint8_t, 32>;
using arr8_res = std::array<uint8_t, 256>;

class nsmtest : public ut::Fixture
{
public:
    void setup() override {}

    void teardown() override {}

public:
    nv::mctp::Control*       _control = new nv::mctp::Control();
    nv::mctp::Nsm            _nsm     = *_control;
    std::array<uint8_t, 256> result;

    static nv::mctp::Packet& from(std::array<uint8_t, 256>& arr)
    {
        return *std::bit_cast<nv::mctp::Packet*>(&arr[0]);
    }
    static nv::mctp::Packet& from(std::array<uint8_t, 32>& arr)
    {
        return *std::bit_cast<nv::mctp::Packet*>(&arr[0]);
    }

    template<typename T>
    void dumpWithCompareBuffers(const T* expect, size_t size1, const T* result)
    {
        for (size_t i = 0; i < size1; i++) {
            if (expect[i] != result[i])
                nv::info("i = %d exp = %02x res = %02x", i, expect[i], result[i]);
        }
    }

    // Function template to print the elements of a std::array
    template<typename T, std::size_t N>
    void dumpBuffer(const std::array<T, N>& buffer, const size_t size)
    {
        nv::info("0x00: ");
        for (size_t i = 0; i < size; i += 8) {
            if (i && (i % 16 == 0)) nv::info("\n0x%02x: ", i);
            nv::info("%02x %02x %02x %02x %02x %02x %02x %02x",
                     buffer[i],
                     buffer[i + 1],
                     buffer[i + 2],
                     buffer[i + 3],
                     buffer[i + 4],
                     buffer[i + 5],
                     buffer[i + 6],
                     buffer[i + 7]);
        }
        nv::info("\n");
    }

    template<typename T, std::size_t N, std::size_t M>
    std::array<T, N> insert_into_array_nsm(const std::array<T, N>& arr,
                                           std::size_t             pos,
                                           const std::array<T, M>& insert_arr)
    {
        std::array<T, N> new_arr;

        // Copy elements before the position
        std::copy(arr.begin(), arr.begin() + pos, new_arr.begin());

        const size_t offset = sizeof(insert_arr) - pos;
        // Insert the new array elements
        std::copy(insert_arr.begin(), insert_arr.begin() + offset, new_arr.begin() + pos);

        return new_arr;
    }

    using MctpMessageRequest  = arr8_req;
    using MctpMessageResponse = arr8_res;

    bool compare_response(const MctpMessageResponse& expected_response,
                          size_t                     header_len,
                          bool                       compare     = true,
                          bool                       skipheaders = true)
    {
        auto ret = compare
                     ? std::mismatch(expected_response.begin() + (skipheaders ? header_len : 0),
                                     expected_response.end(),
                                     result.begin() + (skipheaders ? header_len : 0))
                               .first
                           == expected_response.end()
                     : true;

        if (!ret) {
            dumpWithCompareBuffers(expected_response.data(), 256, result.data());
        }
        return ret;
    }

    // OCP Header V1
    MctpMessageRequest make_request_V1_with_header(const size_t header_len, arr8_req& send_req)
    {
        arr8_req req{
            0x1A, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x32, 0xC8, 0x7E, 0x10, 0xDE, 0x81, 0x89};
        //  Mctp priv ------------| MCTP Header-----------| VPci|   pciVdrId| ----| ocp V1
        MctpMessageRequest request = insert_into_array_nsm(req, header_len, send_req);
        return request;
    }

    // OCP Header V1
    MctpMessageResponse make_response_V1_with_header(const size_t    header_len,
                                                     const arr8_res& expected)
    {
        arr8_res res{
            0x1A, 0xB4, 0x00, 0x00, 0x01, 0x32, 0x00, 0xC0, 0x7E, 0x10, 0xDE, 0x81, 0x89};
        //  Mctp priv ------------| MCTP Header-----------| VPci|   pciVdrId| ----| ocp V1
        MctpMessageResponse response = insert_into_array_nsm(res, header_len, expected);
        return response;
    }

    // OCP Header V2
    MctpMessageRequest make_request_V2_with_header(const size_t header_len, arr8_req& send_req)
    {
        arr8_req req{
            0x1A, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x32, 0xC8, 0x7E, 0x10, 0xDE, 0x81, 0x8A};
        //  Mctp priv ------------| MCTP Header-----------| VPci|   pciVdrId| ----| ocp V2
        MctpMessageRequest request = insert_into_array_nsm(req, header_len, send_req);
        return request;
    }

    // OCP Header V2
    MctpMessageResponse make_response_V2_with_header(const size_t    header_len,
                                                     const arr8_res& expected)
    {
        arr8_res res{
            0x1A, 0xB4, 0x00, 0x00, 0x01, 0x32, 0x00, 0xC0, 0x7E, 0x10, 0xDE, 0x81, 0x8A};
        //  Mctp priv ------------| MCTP Header-----------| VPci|   pciVdrId| ----| ocp V2
        MctpMessageResponse response = insert_into_array_nsm(res, header_len, expected);
        return response;
    }

    // OCP Header V1
    bool sendRecv_nsm(arr8_req&       send,
                      const arr8_res& expect,
                      size_t          size        = 0,
                      bool            compare     = true,
                      bool            skipheaders = true)
    {
        const size_t header_len = 13;

        auto req_with_hdr = make_request_V1_with_header(header_len, send);
        auto res_with_hdr = make_response_V1_with_header(header_len, expect);

        auto& tx = from(result);
        auto& rx = from(req_with_hdr);

        rx.priv.packet_length = header_len + size - 4;

        memset(&tx, 0, sizeof(result));
        bool success = true;
        success      = _nsm.process(rx, tx);
        if (success) {
            return compare_response(res_with_hdr, header_len, compare, skipheaders);
        }
        else {
            nv::info("process fail\n");
        }
        return false;
    }

    // OCP Header V2
    bool sendRecv_v2_nsm(arr8_req&       send,
                         const arr8_res& expect,
                         size_t          size        = 0,
                         bool            compare     = true,
                         bool            skipheaders = true)
    {
        const size_t header_len = 13;

        auto req_with_hdr = make_request_V2_with_header(header_len, send);
        auto res_with_hdr = make_response_V2_with_header(header_len, expect);

        auto& tx = from(result);
        auto& rx = from(req_with_hdr);

        rx.priv.packet_length = header_len + size - 4;

        memset(&tx, 0, sizeof(result));
        bool success = true;
        success      = _nsm.process(rx, tx);
        if (success) {
            return compare_response(res_with_hdr, header_len, compare, skipheaders);
        }
        else {
            nv::info("process fail\n");
        }
        return false;
    }

    bool sendRecv_nsm_type6_sequence(arr8_req&       send1,
                                     arr8_req&       send2,
                                     const arr8_res& expect,
                                     size_t          size1       = 0,
                                     size_t          size2       = 0,
                                     bool            compare     = true,
                                     bool            skipheaders = true)
    {
        // Ensure 1st command is IrreversibleConf enable or disable command
        if (send1[1] != uint8_t(mctp::NsmFWCmdCode::IrreversibleConf)
            || (send1[3] != 0x01 && send1[3] != 0x02)) {
            return false;
        }

        const size_t header_len = 13;
        arr8_req     req{
            0x1A, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x32, 0xC8, 0x7E, 0x10, 0xDE, 0x81, 0x89};
        //  Mctp priv ------------| MCTP Header-----------| VPci|   pciVdrId| ----| ocp
        arr8_res res{
            0x1A, 0xB4, 0x00, 0x00, 0x01, 0x32, 0x00, 0xC0, 0x7E, 0x10, 0xDE, 0x81, 0x89};
        //  Mctp priv ------------| MCTP Header-----------| VPci|   pciVdrId| ----| ocp

        auto req_with_hdr = insert_into_array_nsm(req, header_len, send1);
        auto res_with_hdr = insert_into_array_nsm(res, header_len, expect);

        auto& tx = from(result);
        auto& rx = from(req_with_hdr);

        rx.priv.packet_length = header_len + size1 - 4;

        memset(&tx, 0, sizeof(result));

        // Run enable or disable irreversible
        _nsm.process(rx, tx);

        req_with_hdr = insert_into_array_nsm(req, header_len, send2);

        // Ensure 2nd command is Update Code Authentication Key Permissions or Update Minimum
        // Security Version Number or IrreversibleConf
        if (send2[1] == uint8_t(mctp::NsmFWCmdCode::UpdateCodeAuthKey)
            || send2[1] == uint8_t(mctp::NsmFWCmdCode::UpdateMinSecVerNum)) {
            // Do Xnor operation  on the nonce bitmap provided by send2 and the result nonce of
            // 1st command
            for (size_t i = 0; i < 8; ++i) {
                req_with_hdr[header_len + 8 + i] = ~(req_with_hdr[header_len + 8 + i]
                                                     ^ result[header_len + 6 + i]);
            }
        }
        else if (send2[1] != uint8_t(mctp::NsmFWCmdCode::IrreversibleConf)) {
            return false;
        }

        rx = from(req_with_hdr);

        rx.priv.packet_length = header_len + size2 - 4;

        memset(&tx, 0, sizeof(result));
        bool success = true;
        success      = _nsm.process(rx, tx);
        if (success) {
            auto ret = compare
                         ? std::mismatch(res_with_hdr.begin() + (skipheaders ? header_len : 0),
                                         res_with_hdr.end(),
                                         result.begin() + (skipheaders ? header_len : 0))
                                   .first
                               == res_with_hdr.end()
                         : true;

            if (!ret) {
                dumpWithCompareBuffers(res_with_hdr.data(), 256, result.data());
            }
            return ret;
        }
        else {
            nv::info("process fail\n");
        }
        return false;
    }

    // clang-format off
    bool type5_set_fatal_fault_error_type()
    {
        uint8_t fatal_fault_bitmask = 0x10;  // 4th bit set                                             
        arr8_req set_req{
        //  nvmsg| cmd| size| bitmask             
            0x05, 0x06, 0x08, fatal_fault_bitmask, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        //               nvmsg| cmd | code| reserved  | data size
        arr8_res set_res{0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00};
        return sendRecv_nsm(set_req, set_res, 11);
    }

    bool type5_enable_error_mode()
    {
        // enable error injection mode
        //                 nvmsg| cmd| size| enable
        arr8_req enable_req{0x05, 0x03, 0x01, 0x01};
        //                 nvmsg| cmd | code| reserved  | data size
        arr8_res enable_res{0x05, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
        return sendRecv_nsm(enable_req, enable_res, 4);
    }
    
    bool type5_set_fatal_fault_injection_payload(std::array<uint8_t, 4>& bitmask, uint8_t expected_error = 0x00)
    {
        const uint8_t fatalFault = DeviceError;
        //              nvmsg| cmd|rsvd8|     size16|     rsvd16|   offset                |          Fatal Fault U32    |
        arr8_req set_req{0x05, 0x0B, 0x00, 0x0C, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, fatalFault, 0x00, 0x00, 0x00,
                         // bitmask
                         bitmask.at(0), bitmask.at(1), bitmask.at(2), bitmask.at(3)};
        //               nvmsg| cmd | code         |reserved  | data size |
        arr8_res set_res{0x05, 0x0B, expected_error, 0x00, 0x00, 0x00, 0x00};
        return sendRecv_v2_nsm(set_req, set_res, 19);
    }

    bool type5_get_fatal_fault_injection_payload(std::array<uint8_t, 4>& bitmask, uint8_t expected_error = 0x00)
    {
        const uint8_t fatalFault = DeviceError;
        //               nvmsg| cmd|rsvd8|    size16|     rsvd16|        Fatal Fault Error U32
        arr8_req get_req{0x05, 0x0A, 0x00, 0x04, 0x00, 0x00, 0x00, fatalFault, 0x00, 0x00, 0x00};
        //               nvmsg| cmd | code        |  reserved  | data size| offset                |       Fatal Fault Error U32 |
        arr8_res get_res{0x05, 0x0A, expected_error, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, fatalFault, 0x00, 0x00, 0x00,
                         // bitmask
                         bitmask.at(0), bitmask.at(1), bitmask.at(2), bitmask.at(3)};
        return sendRecv_v2_nsm(get_req, get_res, 11);
    }
    // clang-format on

    struct AggregateInfo
    {
        using TelemetryRecordPointer     = TelemetryRecord<1>*;
        using TelemetryMultiBytesPointer = TelemetryMultiBytes*;

        void*    _aggregate;
        uint8_t* _data;
        uint16_t _size;
        uint16_t _size_data;
        uint8_t  _valid;

        AggregateInfo(void* aggregate)
        : _aggregate{aggregate}
        , _data(nullptr)
        , _size{0}
        , _size_data{0}
        , _valid{0}
        {
            setValues();
        }

        void setValues()
        {
            uint8_t*               ptTemp      = static_cast<uint8_t*>(_aggregate);
            TelemetryRecordPointer ptAggregate = static_cast<TelemetryRecordPointer>(
                _aggregate);
            TelemetryMultiBytesPointer
                ptAggreateMultiBytes = static_cast<TelemetryMultiBytesPointer>(_aggregate);
            if (static_cast<int>(ptAggregate->b) == 0) {
                _size_data = power_of_two(static_cast<size_t>(ptAggregate->length));
                _data      = &ptAggregate->data[0];
                _size      = sizeof(TelemetryRecord<1>) - 1 + _size_data;
                _valid     = static_cast<uint8_t>(ptAggregate->v);
            }
            else {
                _size_data = ptAggreateMultiBytes->multi_bytes_length;
                _data      = ptTemp + sizeof(TelemetryMultiBytes);
                _size      = sizeof(TelemetryMultiBytes) + _size_data;
                _valid     = static_cast<uint8_t>(ptAggreateMultiBytes->v);
            }
        }

        uint8_t tag()
        {
            TelemetryRecordPointer ptAggregate = static_cast<TelemetryRecordPointer>(
                _aggregate);
            return ptAggregate->tag;
        }

        uint8_t  valid() { return _valid; }
        uint8_t* data() { return _data; }
        uint8_t* address() { return static_cast<uint8_t*>(_aggregate); }
        uint16_t size() { return _size; }
        uint16_t sizeData() { return _size_data; }

        // go to  to next Record
        void next()
        {
            uint8_t* ptTemp = static_cast<uint8_t*>(_aggregate);
            _aggregate      = ptTemp + _size;
            setValues();
        }
    };
};

// ------------------     type0     ------------------
// ------------------- 0x00 : Ping -------------------
TEST_F(nsmtest, ping)
{
    //          nvmsg| cmd | size
    arr8_req req{0x00, 0x00, 0x00};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

// ------ 0x01 : Get Supported NV Message Types ------
TEST_F(nsmtest, get_supported_nvidia_message_types)
{
    //          nvmsg| cmd | size
    arr8_req req{0x00, 0x01, 0x00};
    //          nvmsg| cmd | code| reserved  | data size | supported nv msg bitmask
    arr8_res res{0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    if constexpr (nv::ipc::Enable_Nsm_type3) {
        unsigned int bitmask  = (1u << static_cast<unsigned int>(
                                    NsmMsgType::PlatformEnviromentals));
        res[7]               |= static_cast<uint8_t>(bitmask);
    }

    if constexpr (nv::ipc::Enable_Nsm_type4) {
        unsigned int bitmask  = (1u << static_cast<unsigned int>(NsmMsgType::Diagnostics));
        res[7]               |= static_cast<uint8_t>(bitmask);
    }

    if constexpr (nv::ipc::Enable_Nsm_type5) {
        unsigned int bitmask  = (1u
                                << static_cast<unsigned int>(NsmMsgType::DeviceConfiguration));
        res[7]               |= static_cast<uint8_t>(bitmask);
    }

    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

// ------- 0x02 : Get Supported Command Codes --------
TEST_F(nsmtest, get_supported_command_codes_type0)
{
    //          nvmsg| cmd | size| nvmsg
    arr8_req req{0x00, 0x02, 0x01, 0x00};
    //          nvmsg| cmd | code| reserved  | data size | supported cmd bitmask type0
    arr8_res res{0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0xFF, 0x82, 0x01, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, get_supported_command_codes_type6)
{
    //          nvmsg| cmd | size| nvmsg
    arr8_req req{0x00, 0x02, 0x01, 0x06};
    //          nvmsg| cmd | code| reserved  | data size | supported cmd bitmask type6
    arr8_res res{0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, get_supported_command_codes_unsupported_nv_msg)
{
    if constexpr (false == nv::ipc::Enable_Nsm_type5) {
        //          nvmsg| cmd | size| nvmsg
        arr8_req req{0x00, 0x02, 0x01, 0x05};
        //          nvmsg| cmd | code| reserved  | data size
        arr8_res res{0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00};
        ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
    }
};

TEST_F(nsmtest, get_supported_command_codes_invalid_data_len)
{
    //          nvmsg| cmd | size| missing nvmsg
    arr8_req req{0x00, 0x02, 0x01};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

// ------- 0x03 : Get Supported Event Sources --------
TEST_F(nsmtest, get_supported_event_sources_type0)
{
    //          nvmsg| cmd | size| nvmsg
    arr8_req req{0x00, 0x03, 0x01, 0x00};
    //          nvmsg| cmd | code| reserved  | data size | supported event bitmask type0
    arr8_res res{0x00,  // nvmsg
                 0x03,  // cmd
                 0x00,  // code
                 0x00,  // reserved
                 0x00,
                 0x08,  // data size
                 0x00,
                 0x00,  // supported event bitmask type0
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, get_supported_event_sources_type6)
{
    //          nvmsg| cmd | size| nvmsg
    arr8_req req{0x00, 0x03, 0x01, 0x06};
    //          nvmsg| cmd | code| reserved  | data size | supported event bitmask type6
    arr8_res res{0x00,  // nvmsg
                 0x03,  // cmd
                 0x00,  // code
                 0x00,  // reserved
                 0x00,
                 0x08,  // data size
                 0x00,
                 0x02,  // supported event bitmask type6
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, get_supported_event_sources_unsupported_nv_msg)
{
    //          nvmsg| cmd | size| nvmsg
    arr8_req req{0x00, 0x03, 0x01, 0x05};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, get_supported_event_sources_invalid_data_len)
{
    //          nvmsg| cmd | size| missing nvmsg
    arr8_req req{0x00, 0x03, 0x01};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

// -------- 0x04 : Get Current Event Sources ---------
// -------- 0x05 : Set Current Event Sources ---------
TEST_F(nsmtest, get_current_event_sources_type0)
{
    //          nvmsg| cmd | size| nvmsg
    arr8_req req{0x00, 0x04, 0x01, 0x00};
    //          nvmsg| cmd | code| reserved  | data size | current event bitmask type0
    arr8_res res{0x00,  // nvmsg
                 0x04,  // cmd
                 0x00,  // code
                 0x00,  // reserved
                 0x00,
                 0x08,  // data size
                 0x00,
                 0x00,  // current event bitmask type6
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, get_current_event_sources_type6)
{
    //          nvmsg| cmd | size| nvmsg
    arr8_req req{0x00, 0x04, 0x01, 0x06};
    //          nvmsg| cmd | code| reserved  | data size | current event bitmask type6
    arr8_res res{0x00,  // nvmsg
                 0x04,  // cmd
                 0x00,  // code
                 0x00,  // reserved
                 0x00,
                 0x08,  // data size
                 0x00,
                 0x00,  // current event bitmask type6
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, set_get_current_event_sources_type0_after_set_all)
{
    //           nvmsg| cmd | size|nvmsg| set event bitmask type0
    arr8_req req1{0x00, 0x05, 0x09, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res1{0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 12), true);

    //           nvmsg| cmd | size| nvmsg
    arr8_req req2{0x00, 0x04, 0x01, 0x00};
    arr8_res res2{0x00,  // nvmsg
                  0x04,  // cmd
                  0x00,  // code
                  0x00,  // reserved
                  0x00,
                  0x08,  // data size
                  0x00,
                  0x00,  // current event bitmask type0
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 4), true);
};

TEST_F(nsmtest, set_get_current_event_sources_type6_after_set_all)
{
    //           nvmsg| cmd | size|nvmsg| set event bitmask type6
    arr8_req req1{0x00, 0x05, 0x09, 0x06, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res1{0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 12), true);

    //           nvmsg| cmd | size| nvmsg
    arr8_req req2{0x00, 0x04, 0x01, 0x06};
    arr8_res res2{0x00,  // nvmsg
                  0x04,  // cmd
                  0x00,  // code
                  0x00,  // reserved
                  0x00,
                  0x08,  // data size
                  0x00,
                  0x02,  // current event bitmask type6
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 4), true);
};

TEST_F(nsmtest, get_current_event_sources_type6_after_set_exclude_event_id_1)
{
    //           nvmsg| cmd | size|nvmsg| set event bitmask type6
    arr8_req req1{0x00, 0x05, 0x09, 0x06, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res1{0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 12), true);

    //           nvmsg| cmd | size| nvmsg
    arr8_req req2{0x00, 0x04, 0x01, 0x06};
    arr8_res res2{0x00,  // nvmsg
                  0x04,  // cmd
                  0x00,  // code
                  0x00,  // reserved
                  0x00,
                  0x08,  // data size
                  0x00,
                  0x00,  // current event bitmask type6
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 4), true);
};

TEST_F(nsmtest, get_current_event_sources_unsupported_nv_msg)
{
    //          nvmsg| cmd | size| nvmsg
    arr8_req req{0x00, 0x04, 0x01, 0x05};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, get_current_event_sources_invalid_data_len)
{
    //          nvmsg| cmd | size| missing nvmsg
    arr8_req req{0x00, 0x04, 0x01};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x04, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

TEST_F(nsmtest, set_current_event_sources_unsupported_nv_msg)
{
    //          nvmsg| cmd | size|nvmsg| set event bitmask type5 (invalid)
    arr8_req req{0x00, 0x05, 0x09, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 12), true);
};

TEST_F(nsmtest, set_current_event_sources_invalid_data_len)
{
    //          nvmsg| cmd | size|nvmsg| bitmask with only 7 bytes
    arr8_req req{0x00, 0x05, 0x09, 0x06, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x05, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 11), true);
};

// ---------- 0x06 : Set Event Subscription ----------
// ---------- 0x07 : Get Event Subscription ----------
TEST_F(nsmtest, set_get_event_subscription_disable)
{
    //           nvmsg| cmd | size|setting| mctp endpoint ID
    arr8_req req1{0x00, 0x06, 0x02, 0x00, 0xFE};
    //           nvmsg| cmd | code| reserved  | data size
    arr8_res res1{0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 5), true);

    //           nvmsg| cmd | size
    arr8_req req2{0x00, 0x07, 0x00};
    //           nvmsg| cmd | code| reserved  | data size | mctp endpoint ID
    arr8_res res2{0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0xFE};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 3), true);
};

TEST_F(nsmtest, set_get_event_subscription_polling)
{
    //           nvmsg| cmd | size|setting| mctp endpoint ID
    arr8_req req1{0x00, 0x06, 0x02, 0x01, 0xFE};
    //           nvmsg| cmd | code| reserved  | data size
    arr8_res res1{0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 5), true);

    //           nvmsg| cmd | size
    arr8_req req2{0x00, 0x07, 0x00};
    //           nvmsg| cmd | code| reserved  | data size | mctp endpoint ID
    arr8_res res2{0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0xFE};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 3), true);
};

TEST_F(nsmtest, set_get_event_subscription_push)
{
    //           nvmsg| cmd | size|setting| mctp endpoint ID
    arr8_req req1{0x00, 0x06, 0x02, 0x02, 0xFE};
    //           nvmsg| cmd | code| reserved  | data size
    arr8_res res1{0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 5), true);

    //           nvmsg| cmd | size
    arr8_req req2{0x00, 0x07, 0x00};
    //           nvmsg| cmd | code| reserved  | data size | mctp endpoint ID
    arr8_res res2{0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0xFE};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 3), true);
};

TEST_F(nsmtest, set_event_subscription_invalid_setting)
{
    //          nvmsg| cmd | size|setting| mctp endpoint ID
    arr8_req req{0x00, 0x06, 0x02, 0x03, 0xFE};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 5), true);
};

TEST_F(nsmtest, set_event_subscription_invalid_data_len)
{
    //          nvmsg| cmd | size|setting| missing mctp endpoint ID
    arr8_req req{0x00, 0x06, 0x02, 0x02};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, get_event_subscription_no_subscribe)
{
    //          nvmsg| cmd | size
    arr8_req req{0x00, 0x07, 0x00};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

TEST_F(nsmtest, get_event_subscription_no_subscribe_after_twice_disable)
{
    //           nvmsg| cmd | size|setting| mctp endpoint ID
    arr8_req req1{0x00, 0x06, 0x02, 0x00, 0xFE};
    //           nvmsg| cmd | code| reserved  | data size
    arr8_res res1{0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 5), true);

    //           nvmsg| cmd | size
    arr8_req req2{0x00, 0x07, 0x00};
    //           nvmsg| cmd | code| reserved  | data size | mctp endpoint ID
    arr8_res res2{0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0xFE};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 3), true);

    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 5), true);

    //          nvmsg| cmd | size
    arr8_req req3{0x00, 0x07, 0x00};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res3{0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req3, res3, 3), true);
};

// -------------- 0x09 : Query Device ID -------------
TEST_F(nsmtest, query_device_id)
{
    //          nvmsg| cmd | size
    arr8_req req{0x00, 0x09, 0x00};
    //          nvmsg| cmd | code| reserved  | data size | Device ID        | instance ID
    arr8_res res{0x00, 0x09, 0x00, 0x00, 0x00, 0x02, 0x00, NvMctpDeviceMcuId, 0xFF};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

// ---------- Unsupported Type0 Command ---------- 0x08
TEST_F(nsmtest, unsupported_dcd_cmd_0x08)
{
    //          nvmsg| cmd | size
    arr8_req req{0x00, 0x08, 0x00};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x00, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

// ------------------     type6     ------------------
// ---------- 0x01 : NSM_GET_ROT_STATE_INFO ----------
/*
TEST_F(nsmtest, get_rot_info)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x01, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x00};
    // TODO - get_rot_info res could be different in the future
    arr8_res res{0x06, 0x01, 0x00, 0x19, 0x00, 0x01, 0x01, 0x01, 0x02, 0x01, 0x00, 0x03, 0x01,
0x00, 0x0D, 0x02, 0x00, 0x00, 0x0F, 0x01, 0x00, 0x10, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x05, 0x01, 0x02, 0x06, 0x01, 0x00, 0x07, 0x09, 0x30, 0x30, 0x2E, 0x30, 0x30, 0x2E,
0x30, 0x30, 0x30, 0x30, 0x2E, 0x30, 0x30, 0x30, 0x30, 0x00, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00,
0x09, 0x01, 0x00, 0x0A, 0x00, 0x00, 0x04, 0x01, 0x00, 0x0B, 0x01, 0x01, 0x0C, 0x02, 0x00, 0x00,
0x0E, 0x02, 0x00, 0x00, 0x06, 0x01, 0x01, 0x07, 0x09, 0x30, 0x30, 0x2E, 0x30, 0x30, 0x2E, 0x30,
0x30, 0x30, 0x30, 0x2E, 0x30, 0x30, 0x30, 0x30, 0x00, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00, 0x09,
0x01, 0x00, 0x0A, 0x00, 0x00, 0x04, 0x01, 0x00, 0x0B, 0x01, 0x05, 0x0C, 0x02, 0x00, 0x00, 0x0E,
0x02, 0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};
*/
TEST_F(nsmtest, get_rot_info_invalid_comp_class)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x01, 0x05, 0xA0, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x00};  // 0A
                                                                                         // 00
                                                                                         // ->
                                                                                         // A0
                                                                                         // 00
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, get_rot_info_invalid_comp_id)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x01, 0x05, 0x0A, 0x00, NvPldmMcuComponentIdPrefix, 0x02, 0x00};  // 02
                                                                                         // FF
                                                                                         // ->
                                                                                         // FF
                                                                                         // 02
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, get_rot_info_invalid_comp_class_idx)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x01, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x01};  // 00
                                                                                         // ->
                                                                                         // 01
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, get_rot_info_invalid_len)
{
    //          nvmsg| cmd | size| comp_class| comp_id
    arr8_req req{
        0x06, 0x01, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix};  // missing
                                                                          // comp_class_idx
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 7), true);
};

// ---------- 0x02 : NSM_IRREVERSIBLE_CONF ----------
TEST_F(nsmtest, query_irreversible)
{
    //          nvmsg| cmd | size| request type - query
    arr8_req req{0x06, 0x02, 0x01, 0x00};
    //          nvmsg| cmd | code| reserved  | data size | irreverible config - disable
    arr8_res res{0x06, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, irreversible_invalid_request_type)
{
    //          nvmsg| cmd | size| request type
    arr8_req req{0x06, 0x02, 0x01, 0x03};  // 03 is invalid
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, irreversible_invalid_data_len)
{
    //          nvmsg| cmd | size| missing request type
    arr8_req req{0x06, 0x02, 0x01};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

TEST_F(nsmtest, query_irreversible_after_disable)
{
    //           nvmsg| cmd | size| request type - disable
    arr8_req req1{0x06, 0x02, 0x01, 0x01};
    //           nvmsg| cmd | code| reserved  | data size
    arr8_res res1{0x06, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req1, res1, 4), true);

    //           nvmsg| cmd | size| request type - query
    arr8_req req2{0x06, 0x02, 0x01, 0x00};
    //           nvmsg| cmd | code| reserved  | data size | irreverible config - disable
    arr8_res res2{0x06, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 4), true);
};

/*
// TODO - Case : enable irreversible
// raised STORAGE_ERROR : stack overflow or erroneous memory access
// Trace code : Error occurs when generating random bytes using function from sdk lib
// TODO - "res" should be fix since it returns random bytes
TEST_F(nsmtest, enable_irreversible)
{
    //          nvmsg| cmd | size| request type - enable
    arr8_req req{0x06, 0x02, 0x01, 0x02};
    //          nvmsg| cmd | code| reserved  | data size | nonce(8 bytes)
    arr8_res res{0x06, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF}; ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
};

TEST_F(nsmtest, query_irreversible_after_enable)
{
    //           nvmsg| cmd | size| request type - enable
    arr8_req req1{0x06, 0x02, 0x01, 0x02};
    // return random bytes -> no check

    //           nvmsg| cmd | size| request type - query
    arr8_req req2{0x06, 0x02, 0x01, 0x00};
    //           nvmsg| cmd | code| reserved  | data size | irreverible config - enable
    arr8_res res2{0x06, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01};
    ensure::is_eq(fixture.sendRecv_nsm(req2, res2, 4), true);
};
*/

// ---------- 0x03 : NSM_QUERY_FW_CODE_AUTH_KEY ----------
TEST_F(nsmtest, query_fw_code_auth_key)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x03, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x00};
    // get_image_signing_key_version would fail in unit test
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, query_fw_code_auth_key_invalid_comp_class)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x03, 0x05, 0xA0, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x00};  // 0A
                                                                                         // 00
                                                                                         // ->
                                                                                         // A0
                                                                                         // 00
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, query_fw_code_auth_key_invalid_comp_id)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x03, 0x05, 0x0A, 0x00, NvPldmMcuComponentIdPrefix, 0x02, 0x00};  // 02
                                                                                         // FF
                                                                                         // ->
                                                                                         // FF
                                                                                         // 02
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, query_fw_code_auth_key_invalid_comp_class_idx)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x03, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x01};  // 00
                                                                                         // ->
                                                                                         // 01
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, query_fw_code_auth_key_invalid_data_len)
{
    //          nvmsg| cmd | size| comp_class| comp_id
    arr8_req req{0x06, 0x03, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix};  // missing
                                                                                   // idx
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 7), true);
};

// ---------- 0x04 : NSM_UPDATE_CODE_AUTH_KEY ----------
// Req2 will pass nonce bitmap instead of exact nonce
// The bitmap will be used for XNOR bitwise operation with nonce(random bytes get from req1)
// All 8 bytes of 0xFF :Reserve the nonce get from req1
// TODO - Only has request type "permitted"
/*
TEST_F(nsmtest, update_code_auth_key_permitted_enable)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x04, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00};
    // TODO - res could be different when GFWLYNT1-433 finished
    arr8_res res{0x06, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19), true);
};

TEST_F(nsmtest, update_code_auth_key_permitted_disable)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x01}; // disable irreversible
    arr8_req req2{0x06, 0x04, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00}; arr8_res res{0x06, 0x04, 0x87, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19), true);
};

TEST_F(nsmtest, update_code_auth_key_permitted_nonce_mismatch)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x04, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xAB, 0xFF, 0xFF, 0x01, 0x00}; // 0xFF -> 0xAB arr8_res res{0x06, 0x04, 0x88, 0x00, 0x00,
0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19), true);
};

TEST_F(nsmtest, update_code_auth_key_permitted_invalid_comp_class)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x04, 0x10, 0x00, 0x00, 0x0A, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00}; // 0A 00 -> A0 00 arr8_res res{0x06, 0x04, 0x02, 0x00,
0x00, 0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19),
true);
};

TEST_F(nsmtest, update_code_auth_key_permitted_invalid_comp_id)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x04, 0x10, 0x00, 0x0A, 0x00, 0xFF, 0x02, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00}; // 02 FF -> FF 02 arr8_res res{0x06, 0x04, 0x02, 0x00,
0x00, 0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19),
true);
};

TEST_F(nsmtest, update_code_auth_key_permitted_invalid_comp_class_idx)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x04, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00}; // 00 -> 01 arr8_res res{0x06, 0x04, 0x02, 0x00, 0x00,
0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19), true);
};

TEST_F(nsmtest, update_code_auth_key_permitted_invalid_len)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x04, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF}; // missing last 2 bytes arr8_res res{0x06, 0x04, 0x03, 0x00, 0x00,
0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 17), true);
};

TEST_F(nsmtest, update_code_auth_key_specified_invalid_len)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x04, 0x10, 0x01, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01}; // missing last 1 bytes arr8_res res{0x06, 0x04, 0x03, 0x00,
0x00, 0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 18),
true);
};

TEST_F(nsmtest, update_code_auth_key_specified_invalid_bitmap_len)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    // missing last 1 bytes , bitmap len = 0x02
    arr8_req req2{0x06, 0x04, 0x10, 0x01, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x02}; arr8_res res{0x06, 0x04, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 18), true);
};
*/

// ---------- 0x05 : NSM_QUERY_FW_SEC_VER_NUM ----------
TEST_F(nsmtest, query_fw_svn)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x05, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x00};
    // get_security_version would fail in unit test
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, query_fw_svn_invalid_comp_class)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x05, 0x05, 0xA0, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x00};  // 0A
                                                                                         // 00
                                                                                         // ->
                                                                                         // A0
                                                                                         // 00
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, query_fw_svn_invalid_comp_id)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x05, 0x05, 0x0A, 0x00, NvPldmMcuComponentIdPrefix, 0x02, 0x00};  // 02
                                                                                         // FF
                                                                                         // ->
                                                                                         // FF
                                                                                         // 02
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, query_fw_svn_invalid_comp_class_idx)
{
    //          nvmsg| cmd | size| comp_class| comp_id                | comp_class_idx
    arr8_req req{0x06, 0x05, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix, 0x01};  // 00
                                                                                         // ->
                                                                                         // 01
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 8), true);
};

TEST_F(nsmtest, query_fw_svn_invalid_data_len)
{
    //          nvmsg| cmd | size| comp_class| comp_id
    arr8_req req{0x06, 0x05, 0x05, 0x0A, 0x00, 0x02, NvPldmMcuComponentIdPrefix};  // missing
                                                                                   // idx
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x05, 0x03, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 7), true);
};

// ---------- 0x06 : NSM_UPDATE_MIN_SEC_VER_NUM ----------
// Req2 will pass nonce bitmap instead of exact nonce
// The bitmap will be used for XNOR bitwise operation with nonce(random bytes get from req1)
// All 8 bytes of 0xFF :Reserve the nonce get from req1
// TODO - Only has request type "permitted"
/*
TEST_F(nsmtest, update_svn_permitted_enable)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x06, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00};
    // TODO - res could be different when GFWLYNT1-433 finished
    arr8_res res{0x06, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19), true);
};

TEST_F(nsmtest, update_svn_permitted_disable)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x01}; // disable irreversible
    arr8_req req2{0x06, 0x06, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00}; arr8_res res{0x06, 0x06, 0x87, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19), true);
};

TEST_F(nsmtest, update_svn_permitted_nonce_mismatch)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x06, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xAB, 0xFF, 0xFF, 0x01, 0x00}; // 0xFF -> 0xAB arr8_res res{0x06, 0x06, 0x88, 0x00, 0x00,
0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19), true);
};

TEST_F(nsmtest, update_svn_permitted_invalid_comp_class)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x06, 0x10, 0x00, 0x00, 0x0A, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00}; // 0A 00 -> A0 00 arr8_res res{0x06, 0x06, 0x02, 0x00,
0x00, 0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19),
true);
};

TEST_F(nsmtest, update_svn_permitted_invalid_comp_id)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x06, 0x10, 0x00, 0x0A, 0x00, 0xFF, 0x02, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00}; // 02 FF -> FF 02 arr8_res res{0x06, 0x06, 0x02, 0x00,
0x00, 0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19),
true);
};

TEST_F(nsmtest, update_svn_permitted_invalid_comp_class_idx)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x06, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00}; // 00 -> 01 arr8_res res{0x06, 0x06, 0x02, 0x00, 0x00,
0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 19), true);
};

TEST_F(nsmtest, update_svn_permitted_invalid_len)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x06, 0x10, 0x00, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF}; // missing last 3 bytes arr8_res res{0x06, 0x06, 0x03, 0x00, 0x00, 0x00,
0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 16), true);
};

TEST_F(nsmtest, update_svn_specified_invalid_len)
{
    arr8_req req1{0x06, 0x02, 0x01, 0x02}; // enable irreversible
    arr8_req req2{0x06, 0x06, 0x10, 0x01, 0x0A, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF}; // missing last 2 bytes arr8_res res{0x06, 0x06, 0x03, 0x00, 0x00,
0x00, 0x00}; ensure::is_eq(fixture.sendRecv_nsm_type6_sequence(req1, req2, res, 4, 17), true);
};
*/

// ---------- 0x07 : NSM_QUERY_FW_COMP_ID ----------
TEST_F(nsmtest, query_fw_comp_id)
{
    //          nvmsg| cmd | size
    arr8_req req{0x06, 0x07, 0x00};
    arr8_res res{0x06,  // nvmsg
                 0x07,  // cmd
                 0x00,  // code
                 0x00,  // reserved
                 0x00,
                 0x06,  // data size
                 0x00,
                 0x01,  // cnt
                 0x0A,  // comp_class
                 0x00,
                 0x02,  // comp_id
                 NvPldmMcuComponentIdPrefix,
                 0x00};  // class_idx
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

// ---------- Unsupported Type6 Command ---------- 0x08
TEST_F(nsmtest, unsupported_fw_cmd_0x08)
{
    //          nvmsg| cmd | size
    arr8_req req{0x06, 0x08, 0x00};
    //          nvmsg| cmd | code| reserved  | data size
    arr8_res res{0x06, 0x08, 0x05, 0x00, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
};

//  ---------- Type 5 -  Get Supported Command codes
TEST_F(nsmtest, get_supported_command_codes_type5)
{
    if constexpr (nv::ipc::Enable_Nsm_type5) {
        //          nvmsg| cmd | size| nvmsg
        arr8_req req{0x00, 0x02, 0x01, 0x05};
        //          nvmsg| cmd | code| reserved  | data size
        arr8_res res{0x00,        // nvmsg
                     0X02,        // cmd
                     0X00,        // code
                     0X00, 0X00,  // reserved
                     0X20, 0X00,  // data size
                     0XF8, 0X1C,  // bitmask data
                     0X00,        // 30 bytes from bitmask
                     0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
                     0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
                     0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00};
        ensure::is_eq(fixture.sendRecv_nsm(req, res, 4), true);
    }
};

//  ---------- Type 5 - Fatal Fault Injection Payload
TEST_F(nsmtest, type5_fatal_fault_injection_payload_watchdog)
{
    // clang-format off
    if constexpr (nv::ipc::Enable_Nsm_type5) {
        ensure::is_eq(fixture.type5_enable_error_mode(), true);
        ensure::is_eq(fixture.type5_set_fatal_fault_error_type(), true);
        std::array<uint8_t, 4> payload_watchdog = {0x02, 0x00, 0x00, 0x00};
        ensure::is_eq(fixture.type5_set_fatal_fault_injection_payload(payload_watchdog), true);
        ensure::is_eq(fixture.type5_get_fatal_fault_injection_payload(payload_watchdog), true);
    }
    // clang-format on
};

TEST_F(nsmtest, type5_fatal_fault_injection_payload_mcu_exception)
{
    // clang-format off
    if constexpr (nv::ipc::Enable_Nsm_type5) {
        ensure::is_eq(fixture.type5_enable_error_mode(), true);
        ensure::is_eq(fixture.type5_set_fatal_fault_error_type(), true);
        std::array<uint8_t, 4> mcu_exception{0x01, 0x00, 0x00, 0x00};
        ensure::is_eq(fixture.type5_set_fatal_fault_injection_payload(mcu_exception), true);
        ensure::is_eq(fixture.type5_get_fatal_fault_injection_payload(mcu_exception), true);
    }
    // clang-format on
};

TEST_F(nsmtest, type5_fatal_fault_injection_invalid_bitmask)
{
    // 0x03 = both Watchdog timeout and Mcu exception set
    std::array<uint8_t, 4> invalid_bitmask{0x03, 0x00, 0x00, 0x00};
    ensure::is_eq(fixture.type5_enable_error_mode(), true);
    ensure::is_eq(fixture.type5_set_fatal_fault_error_type(), true);

    auto errorInvalidData = static_cast<uint8_t>(Ccode::ErrorInvalidData);
    ensure::is_eq(
        fixture.type5_set_fatal_fault_injection_payload(invalid_bitmask, errorInvalidData),
        true);
};

//  ---------- Type 5 - Set/Get Error Injection Mode
TEST_F(nsmtest, type5_get_set_error_injection_mode)
{
    if constexpr (nv::ipc::Enable_Nsm_type5) {
        ensure::is_eq(fixture.type5_enable_error_mode(), true);

        //               nvmsg| cmd | size
        arr8_req get_req{0x05, 0x04, 0x00};
        //               nvmsg| cmd | code| reserved  |data size |
        arr8_res get_res{0x05,  // nvmsg
                         0x04,  // cmd
                         0x00,  // code
                         0x00,
                         0x00,  // reserved
                         0x05,
                         0x00,  // data size
                         // data
                         0x01,  // data mode
                         0x00,
                         0x00,
                         0x00,
                         0x00};  // data bitfield32
        ensure::is_eq(fixture.sendRecv_nsm(get_req, get_res, 3), true);
    }
};

//  ---------- Type 5 - Get Supported Error Types
TEST_F(nsmtest, type5_get_supp_error_types)
{
    if constexpr (nv::ipc::Enable_Nsm_type5) {
        //          nvmsg| cmd | size
        arr8_req req{0x05, 0x05, 0x00};
        arr8_res res{0x05,  // nvmsg
                     0x05,  // cmd
                     0x00,  // code
                     0x00,
                     0x00,  // reserved
                     0x08,
                     0x00,  // data size
                     // data
                     0x10,  // error types bitmask
                     0x00,
                     0x00,
                     0x00,
                     0x00,
                     0x00,
                     0x00,
                     0x00};
        ensure::is_eq(fixture.sendRecv_nsm(req, res, 3), true);
    }
};

//  ---------- Type 5 - Get/Set Current Error Types
TEST_F(nsmtest, type5_get_set_current_error_types)
{
    if constexpr (nv::ipc::Enable_Nsm_type5) {
        ensure::is_eq(fixture.type5_enable_error_mode(), true);
        ensure::is_eq(fixture.type5_set_fatal_fault_error_type(), true);

        //               nvmsg| cmd | size
        arr8_req get_req{0x05, 0x07, 0x00};
        //               nvmsg| cmd | code| reserved  |data size |
        arr8_res get_res{0x05,  // nvmsg
                         0x07,  // cmd
                         0x00,  // code
                         0x00,
                         0x00,  // reserved
                         0x08,
                         0x00,  // data size
                         // data
                         0x10,  // data bitmask
                         0x00,
                         0x00,
                         0x00,
                         0x00,
                         0x00,
                         0x00,
                         0x00};
        ensure::is_eq(fixture.sendRecv_nsm(get_req, get_res, 3), true);
    }
};

// ---------- Type 3 generic test TelemetryRecord ----------
TEST_F(nsmtest, TelemetryRecord)
{
    uint32_t metric_value    = 0x12345678;  // value considering big-endian
    uint8_t  little_endian[] = {0x78, 0x56, 0x34, 0x12};
    uint8_t  tag{0x10};
    TelemetryRecord<sizeof(uint32_t)> metric(tag);

    // values are stored in little-endian
    ensure::is_eq(metric.setValue(metric_value), true);
    ensure::is_eq(std::memcmp(&metric.data[0], &little_endian[0], sizeof(uint32_t)), 0);
    // valid is set after setValue
    ensure::is_eq(static_cast<int>(metric.v), 1);

    // test TelemetryRecord::getValue()
    uint32_t other_value = 0;
    ensure::is_eq(metric.getValue(other_value), true);
    ensure::is_eq(other_value, metric_value);

    // check size for TelemetryRecord::setValue() and
    // TelemetryRecord::getValue()
    uint16_t unsupported = 0;  // only variables with same size are supported
    ensure::is_false(metric.setValue(unsupported));
    // valid is set to false if setValue fails
    ensure::is_eq(static_cast<int>(metric.v), 0);

    // check TelemetryRecord field values
    ensure::is_eq(metric.tag, tag);
    int value_length = static_cast<int>(metric.length);
    ensure::is_eq(value_length, 2);  // 4 bytes 2**2
    int value_rsvd = static_cast<int>(metric.rsvd);
    ensure::is_eq(value_rsvd, 0);
};

TEST_F(nsmtest, telemetryRecordArray_two_nvU8)
{
    TelemetryRecordArrayBuffer record{};
    uint8_t                    value1 = 1, value2 = 2;
    ensure::is_eq(record.addRecordNvU8(1, value1), true);
    ensure::is_eq(record.addRecordNvU8(2, value2), true);
    using RecordType        = TelemetryRecord<sizeof(uint8_t)>;
    auto expected_data_size = sizeof(RecordType) * 2;
    ensure::is_eq(expected_data_size, record.arraySize());
    RecordType* first_record = static_cast<RecordType*>(record.data());
    uint8_t     check_value  = 0xff;
    first_record->getValue(check_value);
    ensure::is_eq(value1, check_value);
    RecordType* second_record = first_record + 1;
    ensure::is_eq(second_record->getValue(check_value), true);
    ensure::is_eq(second_record->tag, 2);
    ensure::is_eq(static_cast<int>(second_record->length), 0);
    ensure::is_eq(static_cast<int>(second_record->rsvd), 0);
};

TEST_F(nsmtest, telemetryRecordArray_two_nvU8_caller_data)
{
    std::array<uint8_t, 8> my_data{};
    TelemetryRecordArray   record(my_data.data(), my_data.size());
    uint8_t                value1 = 1, value2 = 2;
    ensure::is_eq(record.addRecordNvU8(1, value1), true);
    ensure::is_eq(record.addRecordNvU8(2, value2), true);
    using RecordType        = TelemetryRecord<sizeof(uint8_t)>;
    auto expected_data_size = sizeof(RecordType) * 2;
    ensure::is_eq(expected_data_size, record.arraySize());
    auto        voidPointer  = static_cast<void*>(my_data.data());
    RecordType* first_record = static_cast<RecordType*>(voidPointer);
    uint8_t     check_value  = 0xff;
    ensure::is_eq(first_record->getValue(check_value), true);
    ensure::is_eq(first_record->tag, 1);
    ensure::is_eq(static_cast<int>(first_record->length), 0);
    ensure::is_eq(static_cast<int>(first_record->rsvd), 0);
    first_record->getValue(check_value);
    ensure::is_eq(value1, check_value);
    RecordType* second_record = first_record + 1;
    ensure::is_eq(second_record->getValue(check_value), true);
    ensure::is_eq(second_record->tag, 2);
    ensure::is_eq(static_cast<int>(second_record->length), 0);
    ensure::is_eq(static_cast<int>(second_record->rsvd), 0);
};

TEST_F(nsmtest, telemetryRecordArray_aggregate)
{
    std::array<uint8_t, TelemetryRecordArray::DefaultMctpDataSize> area{};
    TelemetryRecordArray aggregate{area.data(), area.size()};
    uint8_t              timestampTag = 0xFF;
    aggregate.addTimestampRecord(timestampTag);
    uint32_t value1 = 0x00AA00AA;
    aggregate.addRecordNvU32(0x00, value1);
    uint32_t value2 = 0x00BB00BB;
    aggregate.addRecordNvU32(0x01, value2);

    using RecordTypeTimestamp = TelemetryRecord<sizeof(uint64_t)>;
    using RecordType          = TelemetryRecord<sizeof(uint32_t)>;

    auto                 voidPointer      = static_cast<void*>(area.data());
    RecordTypeTimestamp* timestamp_record = static_cast<RecordTypeTimestamp*>(voidPointer);
    // check Timestamp record
    ensure::is_eq(timestamp_record->tag, timestampTag);
    ensure::is_eq(static_cast<int>(timestamp_record->v), 1);
    ensure::is_eq(static_cast<int>(timestamp_record->length), 3);
    ensure::is_eq(static_cast<int>(timestamp_record->rsvd), 0);

    auto ptNvU32Records     = area.data() + sizeof(RecordTypeTimestamp);
    voidPointer             = ptNvU32Records;
    auto first_nvU32_record = static_cast<RecordType*>(voidPointer);
    ensure::is_eq(first_nvU32_record->tag, 0x00);
    ensure::is_eq(static_cast<int>(first_nvU32_record->v), 1);
    ensure::is_eq(static_cast<int>(first_nvU32_record->length), 2);
    ensure::is_eq(static_cast<int>(first_nvU32_record->rsvd), 0);
    uint32_t expected_value = 0x00000000;
    ensure::is_eq(first_nvU32_record->getValue(expected_value), true);
    ensure::is_eq(expected_value, value1);

    auto second_nvU32_record = first_nvU32_record + 1;
    ensure::is_eq(second_nvU32_record->tag, 0x01);
    ensure::is_eq(static_cast<int>(second_nvU32_record->v), 1);
    ensure::is_eq(static_cast<int>(second_nvU32_record->length), 2);
    ensure::is_eq(static_cast<int>(second_nvU32_record->rsvd), 0);
    ensure::is_eq(second_nvU32_record->getValue(expected_value), true);
    ensure::is_eq(expected_value, value2);
};

TEST_F(nsmtest, telemetryRecordArray_addRecorVariableArray)
{
    using SixteenBytesArray = std::array<uint8_t, 16>;
    SixteenBytesArray sixteen;
    sixteen.fill(0xA);
    TelemetryRecordArrayBuffer array;
    array.addRecorVariableArray(0x10, sixteen.size(), sixteen);
    ensure::is_eq(array.arraySize(), sixteen.size() + 2);

    TelemetryRecord<16> telemetry;
    std::memcpy(&telemetry, array.data(), array.arraySize());

    ensure::is_eq(telemetry.tag, 0x10);
    ensure::is_eq(static_cast<int>(telemetry.v), 0x01);
    ensure::is_eq(static_cast<int>(telemetry.length), 4);

    SixteenBytesArray other;
    ensure::is_eq(telemetry.getValue(other), true);
    for (size_t counter = 0; counter < other.size(); ++counter) {
        ensure::is_eq(sixteen.at(counter), other.at(counter));
    }
};

// ---------- Type 3 tests with invalid Sensors ----------
TEST_F(nsmtest, invalid_type3_sensors)
{
    if constexpr (nv::ipc::Enable_Nsm_type3) {
        constexpr uint8_t invalid_tag_sensor = 0xFE;
        constexpr auto    errorInvalidData   = static_cast<uint8_t>(Ccode::ErrorInvalidData);

        //                       nvmsg|cmd | size
        arr8_req req_temperature{0x03, 0x00, 0x01, invalid_tag_sensor};
        //                      nvmsg| cmd| size
        arr8_req req_power_draw{0x03, 0x03, 0x01, invalid_tag_sensor};

        //                       nvmsg| cmd| code|             reserved   | data size
        arr8_res res_temperature{0x03, 0x00, errorInvalidData, 0x00, 0x00, 0x00, 0x00};

        //                      nvmsg| cmd | code           | reserved   | data size
        arr8_res res_power_draw{0x03, 0x03, errorInvalidData, 0x00, 0x00, 0x00, 0x00};

        ensure::is_eq(fixture.sendRecv_nsm(req_temperature, res_temperature, 4), true);
        ensure::is_eq(fixture.sendRecv_nsm(req_power_draw, res_power_draw, 4), true);
    }
};

//  ---------- Type 4 - Bridge and Port Recovery
TEST_F(nsmtest, type4_bridge_port_recovery)
{
    if constexpr (nv::ipc::Enable_Nsm_type4) {
        // Test Protocol Reset (1)
        //           nvmsg|cmd | size | recovery_level | reset_target
        arr8_req req_protocol{0x04, 0x70, 0x02, 0x01, 0x05};  // Protocol Reset, EID=5
        //          nvmsg| cmd | code| reserved | data size
        arr8_res res_protocol{0x04, 0x70, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00};
        fixture.sendRecv_nsm(req_protocol, res_protocol, 4);
        auto& tx_protocol  = fixture.from(fixture.result);
        auto& ntx_protocol = NsmPktResp::from(tx_protocol);
        ensure::is_eq(ntx_protocol.completion_code, Ccode::Success);
        uint16_t data_size_protocol = ntx_protocol.data_size;
        ensure::is_eq(data_size_protocol, 4);  // sizeof(BridgePortRecoveryResp) = 4 bytes

        // Test Port Reset (2)
        //           nvmsg|cmd | size | recovery_level | reset_target
        arr8_req req_port{0x04, 0x70, 0x02, 0x02, 0x03};  // Port Reset, Port=3
        //          nvmsg| cmd | code| reserved | data size
        arr8_res res_port{0x04, 0x70, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00};
        fixture.sendRecv_nsm(req_port, res_port, 4);
        auto& tx_port  = fixture.from(fixture.result);
        auto& ntx_port = NsmPktResp::from(tx_port);
        ensure::is_eq(ntx_port.completion_code, Ccode::Success);
        uint16_t data_size_port = ntx_port.data_size;
        ensure::is_eq(data_size_port, 4);  // sizeof(BridgePortRecoveryResp) = 4 bytes

        // Test Query Next Target (255)
        //           nvmsg|cmd | size | recovery_level | reset_target
        arr8_req req_query{0x04, 0x70, 0x02, 0xFF, 0x00};  // Query Next Target
        //          nvmsg| cmd | code| reserved | data size
        arr8_res res_query{0x04, 0x70, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00};
        fixture.sendRecv_nsm(req_query, res_query, 4);
        auto& tx_query  = fixture.from(fixture.result);
        auto& ntx_query = NsmPktResp::from(tx_query);
        ensure::is_eq(ntx_query.completion_code, Ccode::Success);
        uint16_t data_size_query = ntx_query.data_size;
        ensure::is_eq(data_size_query, 4);  // sizeof(BridgePortRecoveryResp) = 4 bytes

        // Test unsupported Application Reset (0)
        //           nvmsg|cmd | size | recovery_level | reset_target
        arr8_req req_app{0x04, 0x70, 0x02, 0x00, 0x00};  // Application Reset (not supported)
        //          nvmsg| cmd | code| reserved | data size
        arr8_res res_app{0x04, 0x70, 0x00, 0x00, 0x00, 0x00};
        fixture.sendRecv_nsm(req_app, res_app, 4);
        auto& tx_app  = fixture.from(fixture.result);
        auto& ntx_app = NsmPktResp::from(tx_app);
        ensure::is_eq(ntx_app.completion_code, Ccode::ErrorUnsupportedCmd);

        // Test unsupported OOB Hardware Reset (3)
        //           nvmsg|cmd | size | recovery_level | reset_target
        arr8_req req_oob{0x04, 0x70, 0x02, 0x03, 0x05};  // OOB Hardware Reset (not supported)
        //          nvmsg| cmd | code| reserved | data size
        arr8_res res_oob{0x04, 0x70, 0x00, 0x00, 0x00, 0x00};
        fixture.sendRecv_nsm(req_oob, res_oob, 4);
        auto& tx_oob  = fixture.from(fixture.result);
        auto& ntx_oob = NsmPktResp::from(tx_oob);
        ensure::is_eq(ntx_oob.completion_code, Ccode::ErrorUnsupportedCmd);

        // Test reserved value (4)
        //           nvmsg|cmd | size | recovery_level | reset_target
        arr8_req req_reserved{0x04, 0x70, 0x02, 0x04, 0x00};  // Reserved value
        //          nvmsg| cmd | code| reserved | data size
        arr8_res res_reserved{0x04, 0x70, 0x00, 0x00, 0x00, 0x00};
        fixture.sendRecv_nsm(req_reserved, res_reserved, 4);
        auto& tx_reserved  = fixture.from(fixture.result);
        auto& ntx_reserved = NsmPktResp::from(tx_reserved);
        ensure::is_eq(ntx_reserved.completion_code, Ccode::ErrorUnsupportedCmd);
    }
};

/* commented these two UTs due to it crashes on
    nv::ipc::Task::get_task_id_and_priority() calling uxTaskPriorityGet()
    the pxTCB is invalid pointer
*/
#if 0 
//  ---------- Type 4 - Get Device Diagnostics
TEST_F(nsmtest, type4_get_device_diagnostics)
{
    if constexpr (nv::ipc::Enable_Nsm_type4) {
        //           nvmsg|cmd | size | segment
        arr8_req req{0x04, 0x40, 0x01, 0x00};
        //          nvmsg| cmd | code| reserved | data size
        arr8_res res{0x04, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00};
        fixture.sendRecv_nsm(req, res, 4);
        auto& tx  = fixture.from(fixture.result);
        auto& ntx = NsmPktResp::from(tx);
        ensure::is_eq(ntx.completion_code, Ccode::Success);
        ensure::is_ne(static_cast<int>(ntx.data_size), 0);
        ensure::is_eq(ntx.data[0], 0xFF);

        int                           offset = 1;
        struct nsmtest::AggregateInfo aggregate(&ntx.data[offset]);
        constexpr uint8_t             DIAG_START_MCU_TAG = 200;

        // ignore any tagId that belongs to Type4CommonDiagnosticEntries
        while (aggregate.tag() <= DIAG_START_MCU_TAG) {
            nv::debug("[1] tag=%d offset=%d\n",
                      aggregate.tag(),
                      aggregate.address() - &ntx.data[offset]);
            ensure::is_eq(aggregate.valid(), 1);
            aggregate.next();
        }

        // check if all telemetries tagId are present
        for (const auto& [telemetry_tagid, telemetry_type, sensor_tagid] :
             mcuDiagnosticTelemetries) {
            (void)telemetry_type;
            (void)sensor_tagid;
            nv::debug("[2] tag=%d offset=%d\n",
                      aggregate.tag(),
                      aggregate.address() - &ntx.data[offset]);
            ensure::is_eq(aggregate.valid(), 1);
            ensure::is_eq(telemetry_tagid, aggregate.tag());
            aggregate.next();
        }
        ensure::is_eq(aggregate.tag(), DIAG_CurrentTimestamp);
        ensure::is_eq(1, aggregate.valid());
    }
};

TEST_F(nsmtest, type4_get_appendRecord_task_priority)
{
    std::array<uint8_t, TelemetryRecordArray::MaxNsmBulkResponseSize> buffer;
    TelemetryRecordArray array(buffer.data(), buffer.size());
    auto                 rcCode = nv::mctp::appendRecord_task_priority(array);
    ensure::is_eq(rcCode, Ccode::Success);

    struct nsmtest::AggregateInfo aggregate(buffer.data());

    ensure::is_eq(aggregate.tag(), DIAG_TASK_PRIORITY);
    uint16_t size_data = aggregate.sizeData();
    nv::debug("task_priority size_data=%d\n", size_data);
    ensure::is_true(size_data > 0);
};
#endif

namespace nv::mctp {
Ccode appendRecord_task_execution_time(TelemetryRecordArray& devDiagTelemetryArray);
Ccode appendRecord_cpu_utilization(TelemetryRecordArray& devDiagTelemetryArray);
Ccode appendRecord_task_priority(TelemetryRecordArray& devDiagTelemetryArray);
Ccode appendRecord_all_error_counter_telemetries(TelemetryRecordArray& devDiagTelemetryArray);
}  // namespace nv::mctp

TEST_F(nsmtest, type4_get_appendRecord_task_execution_time)
{
    std::array<uint8_t, TelemetryRecordArray::MaxNsmBulkResponseSize> buffer;
    TelemetryRecordArray array(buffer.data(), buffer.size());
    auto                 rcCode = nv::mctp::appendRecord_task_execution_time(array);
    ensure::is_eq(rcCode, Ccode::Success);
};

TEST_F(nsmtest, type4_get_appendRecord_cpu_utilization)
{
    std::array<uint8_t, TelemetryRecordArray::MaxNsmBulkResponseSize> buffer;
    TelemetryRecordArray array(buffer.data(), buffer.size());
    auto                 rcCode = nv::mctp::appendRecord_cpu_utilization(array);
    ensure::is_eq(rcCode, Ccode::Success);

    struct nsmtest::AggregateInfo aggregate(buffer.data());

    ensure::is_eq(aggregate.tag(), DIAG_CPU_UTILIZATION);
    uint16_t size_data = aggregate.sizeData() / sizeof(int);
    nv::debug("cpu_utilization size_data=%d\n", size_data);
    ensure::is_true(size_data > 0 && size_data <= nv::perf_mon::CpuUtilizationEntryNum);

#if 0  // CPU utilization must be greater than zero but on testrunner all data are zeroes        
    while (size_data--) {
        int* ptInteger  = static_cast<int*>(aggregate.data());        
        ensure::is_gt(*ptInteger, 0);
        aggregate.next();
    }
#endif
};

TEST_F(nsmtest, type4_append_error_counter_telemetries)
{
    std::array<uint8_t, TelemetryRecordArray::MaxNsmBulkResponseSize> buffer;
    TelemetryRecordArray array(buffer.data(), buffer.size());
    auto                 rcCode = nv::mctp::appendRecord_all_error_counter_telemetries(array);
    ensure::is_eq(rcCode, Ccode::Success);

    struct nsmtest::AggregateInfo aggregate(buffer.data());
    uint16_t                      size_data = aggregate.sizeData();
    nv::debug("error_counter_telemetries size_data=%d\n", size_data);
    ensure::is_true(size_data > 0);
};
