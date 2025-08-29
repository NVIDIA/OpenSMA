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
#include "nv/mctp/driver.h"

#include <chrono>

#include "nv/i2c/task.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"
#include "nv/mctp/interface.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;
using namespace std::chrono_literals;

using arr8_req = std::array<uint8_t, 32>;
using arr8_res = std::array<uint8_t, 256>;

class VendorDriver : public ut::Fixture
{
public:

    nv::mctp::Control*       _control = new nv::mctp::Control();
    nv::mctp::Vendor         _vendor  = *_control;
    std::array<uint8_t, 256> result;

    void setup() override {}

    void teardown() override {}

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

    bool sendRecv(arr8_req&       send,
                      const arr8_res& expect,
                      size_t          size        = 0,
                      bool            compare     = true,
                      bool            skipheaders = true,
                      uint8_t         iid         = 0)
    {
        const size_t header_len = 15;
        arr8_req     req{
        //  Mctp priv ------------| MCTP -----------------|
            0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //  Vdr | iana ---------------- | RqD | vMT | Cmd...
            0x7f, 0x00, 0x00, 0x16, 0x47, 0x80, 0x01};
        arr8_res res{
        //  Mctp priv ------------| MCTP -----------------|
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        //  Vdr | iana ---------------- | RqD | vMT | Cmd...
            0x7f, 0x00, 0x00, 0x16, 0x47, 0x80, 0x01};
        auto req_with_hdr = insert_into_array_nsm(req, header_len, send);
        auto res_with_hdr = insert_into_array_nsm(res, header_len, expect);

        auto& tx = from(result);
        auto& rx = from(req_with_hdr);

        rx.priv.packet_length = header_len + size - 4;

        memset(&tx, 0, sizeof(result));
        bool success = true;
        success      = _vendor.process(rx, tx);
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
};

TEST_F(VendorDriver, Bgcopy)
{
    // disable bg
    //       Cmd | ver | set
    arr8_req req{0x09, 0x01, 0x00};
    //       Cmd | ver | CC  |
    arr8_res res{0x09, 0x01, 0x00};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // query setup
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x05};
    //     Cmd | ver | CC  | stu
    res = {0x09, 0x01, 0x00, 0x00};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // enable bg
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x01};
    //     Cmd | ver | CC  |
    res = {0x09, 0x01, 0x00};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // query setup
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x05};
    //     Cmd | ver | CC  | stu
    res = {0x09, 0x01, 0x00, 0x01};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // set one time disable
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x02};
    //     Cmd | ver | CC  |
    res = {0x09, 0x01, 0x00};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // query setup
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x05};
    //     Cmd | ver | CC  | stu
    res = {0x09, 0x01, 0x00, 0x02};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // set one time enable
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x03};
    //     Cmd | ver | CC  |
    res = {0x09, 0x01, 0x00};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // query setup
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x05};
    //     Cmd | ver | CC  | stu
    res = {0x09, 0x01, 0x00, 0x03};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // init bg
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x04};
    //     Cmd | ver | CC
    res = {0x09, 0x01, 0x00};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // query progress
    //     Cmd | ver | set
    req = {0x09, 0x01, 0x06};
    //     Cmd | ver | CC  | sta | prog
    res = {0x09, 0x01, 0x00, 0x01, 0x64};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // query progress version 2
    //     Cmd | ver | set
    req = {0x09, 0x02, 0x06};
    //     Cmd | ver | CC  | sta | prog
    res = {0x09, 0x02, 0x00, 0x03, 0x64};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);

    // invalid version
    //     Cmd | ver | set
    req = {0x09, 0xff, 0x00};
    //     Cmd | ver | CC
    res = {0x09, 0xff, 0x05};
    ensure::is_eq(fixture.sendRecv(req, res, true, true), true);
};