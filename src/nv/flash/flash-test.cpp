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
#include "nv/flash/flash.h"

#include "nv/common/enum_ops.h"
#include "nv/flash/task.h"
#include "nv/ipc/supervisor.h"
#include "nv/nv.h"
#include "nv/ut/unittest.h"

using namespace nv::ut;
using namespace nv::ipc;
using namespace nv::common;
using namespace nv::flash;
using namespace std::chrono_literals;

class FLASH : public Fixture
{
public:
    void setup() override
    {
        auto& mctptask = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Mctp);
        mctptask.suspend();
        auto& pldm = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Pldm);
        pldm.suspend();
    }

    void teardown() override
    {
        auto& mctptask = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Mctp);
        mctptask.resume();
        auto& pldm = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Pldm);
        pldm.resume();
    }
};

TEST_F(FLASH, Program)
{
    // constexpr Queue::Usecs Timeout = Queue::Usecs(100 * 10000);  /// 100ms
    constexpr uint32_t     Address = 0x1000;
    std::array<uint8_t, 4> Patten{0, 1, 2, 3};
    std::array<uint8_t, 4> buffer{};

    auto status = Flash::erase(Address);
    ensure::is_eq(to_underlying(status), to_underlying(Status::Ok));
    status = Flash::write(Address, Patten);
    ensure::is_eq(status, Status::Ok);
    status = Flash::read(Address, buffer);
    ensure::is_eq(status, Status::Ok);

    ensure::is_eq(Patten, buffer);
};

TEST_F(FLASH, BackgroundCopy)
{
    ProgressPercent progress;
    // constexpr Queue::Usecs Timeout = Queue::Usecs(100 * 1000);  /// 100ms
    auto status = Flash::background_copy_query(progress);
    ensure::is_eq(status, Status::BackgroundCopyIdle);
    ensure::is_eq(progress, 0);

    status = Flash::background_copy_start();
    ensure::is_eq(status, Status::Ok);
    vTaskDelay(500);
    status = Flash::background_copy_query(progress);
    ensure::is_eq(status, Status::BackgroundCopyInprogress);

    while (status == Status::BackgroundCopyInprogress) {
        vTaskDelay(500);
        status = Flash::background_copy_query(progress);
    }
    ensure::is_eq(progress, 100);
    ensure::is_eq(status, Status::BackgroundCopyDone);
};

TEST_F(FLASH, BackgroundCopy_Read)
{
    nv::flash::Buffer write_buffer{};
    nv::flash::Buffer read_buffer{};
    for (uint32_t i = 0; i < write_buffer.size(); i++) {
        write_buffer[i] = static_cast<uint8_t>(i);
    }
    nv::flash::Address address = 0x1000;
    auto               status  = Flash::write(address, write_buffer);

    ProgressPercent progress;

    auto bg_status = Flash::background_copy_start();
    ensure::is_eq(bg_status, Status::Ok);
    vTaskDelay(500);
    bg_status = Flash::background_copy_query(progress);
    ensure::is_eq(bg_status, Status::BackgroundCopyInprogress);

    while (bg_status == Status::BackgroundCopyInprogress) {
        vTaskDelay(500);
        bg_status = Flash::background_copy_query(progress);
        ensure::is_ge(bg_status, Status::BackgroundCopyIdle);

        status = Flash::read(address, read_buffer);
        ensure::is_eq(status, Status::Ok);
        ensure::is_eq(write_buffer, read_buffer);
    }
    ensure::is_eq(progress, 100);
    ensure::is_eq(bg_status, Status::BackgroundCopyDone);
};

TEST_F(FLASH, Npds)
{
    Data data_read  = 0x0;
    Data data_write = 0x5A;

    // Data not valid yet
    auto status = Flash::get_data(Key::NpdsStart, data_read);
    ensure::is_eq(status, Status::Error);

    status = Flash::set_data(Key::NpdsStart, data_write);
    ensure::is_eq(status, Status::Ok);

    Flash::get_data(Key::NpdsStart, data_read);
    ensure::is_eq(status, Status::Ok);
    ensure::is_eq(data_read, 0x5A);
};

TEST_F(FLASH, Pds)
{
    Data data_read  = 0x0;
    Data data_write = 0x5A;

    auto status = Flash::get_data(Key::PdsStart, data_read);
    ensure::is_eq(status, Status::Ok);

    status = Flash::set_data(Key::PdsStart, data_write);
    ensure::is_eq(status, Status::Ok);

    Flash::get_data(Key::PdsStart, data_read);
    ensure::is_eq(status, Status::Ok);
    ensure::is_eq(data_read, 0x5A);
};

TEST_F(FLASH, EraseTimeout)
{
    constexpr Queue::Usecs Timeout = Queue::Usecs(0);
    constexpr uint32_t     Address = 0x1000;

    auto status = Flash::erase(Address, Timeout);
    ensure::is_true(status == Status::Timeout || status == Status::Busy);
};

TEST_F(FLASH, WriteTimeout)
{
    constexpr Queue::Usecs Timeout = Queue::Usecs(0);
    constexpr uint32_t     Address = 0x1000;
    std::array<uint8_t, 4> Patten{0, 1, 2, 3};

    auto status = Flash::write(Address, Patten, Timeout);
    ensure::is_true(status == Status::Timeout || status == Status::Busy);
};

TEST_F(FLASH, ReadTimeout)
{
    constexpr Queue::Usecs Timeout = Queue::Usecs(0);
    constexpr uint32_t     Address = 0x1000;
    std::array<uint8_t, 4> buffer{};

    auto status = Flash::read(Address, buffer, Timeout);
    ensure::is_true(status == Status::Timeout || status == Status::Busy);
};
