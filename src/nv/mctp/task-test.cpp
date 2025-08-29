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
#include "nv/ipc/queue.h"
#include "nv/mctp/driver.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;

TEST(Mctp, SetEpId)
{
    std::array<uint8_t, 72> buf = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x01, 0x00, 0x18};

    ipc::Queue::Item item(buf.begin(), buf.begin() + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    buf[11] = 0x2;  // SetEndpoint::SetEidReset:
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    buf[11] = 0x3;  // SetEndpoint::SetEidDiscovered:
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    buf[11] = 0x4;  // unknown
    mctp::Driver::mctp_send_from_us_i2c_0(item);
};

TEST(Mctp, GetEpId)
{
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x02, 0x00, 0x18};

    ipc::Queue::Item item(test, test + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item);
};

TEST(Mctp, GetEpUuid)
{
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x03, 0x00, 0x18};

    nv::ipc::Queue::Item item(test, test + 72);
    nv::mctp::Driver::mctp_send_from_us_i2c_0(item);
};

TEST(Mctp, GetMctpVerSupport)
{
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x04, 0xFF, 0x18};

    ipc::Queue::Item item(test, test + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[11] = 0x0;  // ControlMsg
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[11] = 0x1;  // Pldm
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[11] = 0x7F;  // Vendor
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[11] = 0x56;  // unknown
    mctp::Driver::mctp_send_from_us_i2c_0(item);
};

TEST(Mctp, GetMsgTypeSupport)
{
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x05, 0x00, 0x18};

    ipc::Queue::Item item(test, test + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item);
};

TEST(Mctp, GetVndrMsgSupport)
{
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x06, 0x00, 0x18};

    ipc::Queue::Item item(test, test + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[11] = 0x1;
    mctp::Driver::mctp_send_from_us_i2c_0(item);
};

TEST(Mctp, Unsupport)
{
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x07, 0x00, 0x18};

    ipc::Queue::Item item(test, test + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    /* ctl.tag_owner 1 && ctl.rq 1 && ctl.d 1 */
    test[9] = 0xC0;
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[9] = 0x80;

    /* ctl.tag_owner 0 && ctl.rq 0 && ctl.d 0 */
    test[7] = 0xC0;
    test[9] = 0x0;
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[7] = 0xC8;
    test[9] = 0x80;

    /* ctl.tag_owner 1 && ctl.rq 0 && ctl.d 0 */
    test[9] = 0x0;
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[9] = 0x80;

    // wrong eid;
    test[5] = 0x19;
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[5] = 0x0;

    // hdr_ver = 2;
    test[4] = 0x2;
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[4] = 0x1;

    // msg_type
    test[8] = 0x55;
    mctp::Driver::mctp_send_from_us_i2c_0(item);
};

TEST(Mctp, VDM)
{
    uint8_t test[] = {0x02,
                      0x0D,
                      0x00,
                      0x00,
                      0x01,
                      0x00,
                      0x27,
                      0xC8,
                      0x7F,
                      0x47,
                      0x16,
                      0x00,
                      0x00,
                      0x81,
                      0x01,
                      0x03,
                      0x01};

    ipc::Queue::Item item(test, test + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    // wrong eid;
    test[5] = 0x19;
    mctp::Driver::mctp_send_from_us_i2c_0(item);
    test[5] = 0x0;

    // pkt_seq = 1;
    test[7] = 0xD8;
    mctp::Driver::mctp_send_from_us_i2c_0(item);

    test[7] = 0xC8;

    /* ctl.tag_owner 1 && ctl.rq 1 && ctl.d 1 */
    test[13] = 0xC0;
    mctp::Driver::mctp_send_from_us_i2c_0(item);
    test[13] = 0x81;

    /* ctl.tag_owner 0 && ctl.rq 0 && ctl.d 0 */
    test[7]  = 0xC0;
    test[13] = 0x0;
    mctp::Driver::mctp_send_from_us_i2c_0(item);
    test[7]  = 0xC8;
    test[13] = 0x81;

    /* ctl.tag_owner 1 && ctl.rq 0 && ctl.d 0 */
    test[13] = 0x0;
    mctp::Driver::mctp_send_from_us_i2c_0(item);
    test[13] = 0x81;
};

TEST(Mctp, toPLDM)
{
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x01, 0x11, 0x22, 0x33, 0x44, 0x55};

    // pldm
    ipc::Queue::Item item(test, test + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item);
};

TEST(Mctp, fromPLDM)
{
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x07, 0x00, 0x18};

    ipc::Queue::Item item(test, test + 256);
    mctp::Driver::mctp_send_from_pldm(item);
};

TEST(Mctp, multipacket)
{
    uint8_t test[] = {
        0x48, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0x88, 0x01, 0x11, 0x22, 0x33, 0x44};

    ipc::Queue::Item item1(test, test + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item1);

    uint8_t test1[] = {
        0x48, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0x58, 0x55, 0x66, 0x77, 0x88, 0x99};

    ipc::Queue::Item item2(test1, test1 + 72);
    mctp::Driver::mctp_send_from_us_i2c_0(item2);
};
