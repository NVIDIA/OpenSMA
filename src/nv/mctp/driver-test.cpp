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

class MctpDriver : public ut::Fixture
{
public:
    void setup() override
    {
        // suspend other tasks that are attached to Mctp
        // auto& i2c0task = nv::ipc::Supervisor::inst().task(ipc::TaskId::I2c0);
        // i2c0task.suspend();
    }

    void teardown() override
    {
        // restart any other tasks
        // auto& i2c0task = nv::ipc::Supervisor::inst().task(ipc::TaskId::I2c0);
        // i2c0task.resume();
    }
};

TEST_F(MctpDriver, GetEndpointEid)
{
    // GetEndpointEid
    uint8_t test[] = {
        0x02, 0x09, 0x00, 0x00, 0x01, 0x00, 0x27, 0xC8, 0x00, 0x80, 0x02, 0x00, 0x18};

    // send data
    ipc::Queue::Item item(test, test + 72);
    ensure::is_eq(mctp::Driver::mctp_send(item, mctp::Client::UsI2c), mctp::Status::Ok);

    // receive
    i2c::Task::Request tx;
    ipc::Queue::Item   item_recv(std::bit_cast<uint8_t*>(&tx), sizeof(tx));
    auto&              queue = ipc::Queue::make(ipc::QueueId::I2c0);
    ensure::is_eq(queue.recv(item_recv, 500ms), ipc::Queue::Status::Ok);

    // verify packet
    mctp::Driver::Buffer buffer{};
    std::copy(std::begin(tx.data), std::end(tx.data), std::begin(buffer));
    auto& res = mctp::Control::PktRes::from(mctp::Driver::from(buffer));
    ensure::is_eq(res.completion_code, mctp::Ccode::Success);
    ensure::is_eq(res.data[0], 0x00);  // should be NotAssignedYet
};
