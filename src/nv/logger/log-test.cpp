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
#include "nv/common/enum_ops.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "nv/ut/unittest.h"

using namespace nv::ut;
using namespace nv::ipc;
using namespace nv::common;
using namespace nv::logger;
using namespace std::chrono_literals;
using namespace nv;

class LOGGER : public Fixture
{
public:
    void setup() override
    {
        auto& mctptask = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Mctp);
        mctptask.suspend();
        auto& pldm = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Pldm);
        pldm.suspend();

        nv::logger::Item resp_item{};
        auto response_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&resp_item),
                                                  sizeof(Item));
        (void)nv::ipc::Queue::make(nv::ipc::QueueId::LogResponseBlocking)
            .recv(response_item, 500ms);
        nv::logger::Dlreq dl_resp{};
        auto dl_response_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&dl_resp),
                                                     sizeof(dl_resp));
        (void)nv::ipc::Queue::make(nv::ipc::QueueId::LogDownloadResp)
            .recv(dl_response_item, 500ms);
    }

    void teardown() override
    {
        auto& mctptask = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Mctp);
        mctptask.resume();
        auto& pldm = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Pldm);
        pldm.resume();
    }
};

static void Clean_log()
{
    Logger::clean_requset();
    Dlreq req{.session = LogSessionMax};
    Logger::download(req);
    LogDLHdr header      = *std::bit_cast<LogDLHdr*>(req.data.data());
    uint32_t delay_count = 10;
    // Wait Clean event stored
    while (header.event_size == 0 && delay_count > 0) {
        vTaskDelay(500);
        req.session = LogSessionMax;
        Logger::download(req);
        header = *std::bit_cast<LogDLHdr*>(req.data.data());
        delay_count--;
    }
}

TEST_F(LOGGER, Clean)
{
    Clean_log();
    Dlreq req{.session = LogSessionMax};
    auto  status = Logger::download(req);
    ensure::is_eq(status, Status::Ok);
    LogDLHdr header      = *std::bit_cast<LogDLHdr*>(req.data.data());
    uint16_t event_size  = header.event_size;
    auto     cur_session = req.session;
    ensure::is_eq(event_size, 0);
};

TEST_F(LOGGER, Add)
{
    Clean_log();
    const std::array<uint8_t, 8> LogData{1, 2, 3, 4, 5, 6, 7, 8};
    auto                         status = info_wait(logger::Event::CommonRaw, LogData);
    ensure::is_eq(status, Status::Ok);

    status = nv::logger::error(logger::Event::CommonRaw, LogData);
    ensure::is_eq(status, Status::Ok);

    Dlreq req{.session = LogSessionMax};
    status = Logger::download(req);
    ensure::is_eq(status, Status::Ok);
    LogDLHdr header      = *std::bit_cast<LogDLHdr*>(req.data.data());
    uint16_t event_size  = header.event_size;
    auto     cur_session = req.session;
    ensure::is_eq(event_size, 2);

    req.session = cur_session;
    Logger::download(req);
    Entry entry      = *std::bit_cast<Entry*>(req.data.data());
    auto  event      = entry.event;
    auto  entry_data = entry.data;
    auto  level      = static_cast<Level>(std::to_underlying(entry.level) & 0x07);
    ensure::is_eq(event, logger::Event::CommonRaw.unique_id);
    ensure::is_eq(entry_data, LogData);
    ensure::is_eq(level, logger::Level::Info);

    req.session = cur_session;
    Logger::download(req);
    entry      = *std::bit_cast<Entry*>(req.data.data());
    event      = entry.event;
    entry_data = entry.data;
    level      = static_cast<Level>(std::to_underlying(entry.level) & 0x07);
    ensure::is_eq(event, logger::Event::CommonRaw.unique_id);
    ensure::is_eq(entry_data, LogData);
    ensure::is_eq(level, logger::Level::Error);
};