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
#include <array>

#include "nv/common/utils.h"
#include "nv/flash/background_copy.h"
#include "nv/flash/common.h"
#include "nv/flash/driver.h"
#include "nv/flash/flash.h"
#include "nv/flash/npds.h"
#include "nv/flash/pds.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
namespace nv::flash {

constexpr auto FlashTimeout                      = std::chrono::microseconds(50000);
constexpr auto DualCoreFlashRequestQueueMaxItems = 2;

class Task : public nv::ipc::Task
{
public:
    static void make();
    static void entrypoint(void* params);
    static nv::flash::Status
    request(const Request& request, Response& response, nv::ipc::Queue::Usecs timeout);

    Task() noexcept;

    void start();

    nv::flash::Status get_data_from_kernel(Key key, Data& data);

    nv::flash::Status set_data_from_kernel(Key key, const Data& data);

    static void wdt_notify();

private:
    nv::ipc::Event& _event;
    nv::ipc::Queue& _request;
    nv::ipc::Queue& _response;

    void process(const Request& request, Response& response);
    void read(const Request& request, Response& response);
    void write(const Request& request, Response& response);
    void erase(const Request& request, Response& response);

    void set_data(const Request& request, Response& response);
    void get_data(const Request& request, Response& response);

    void cfpa_get_customer(const Request& request, Response& response);
    void cfpa_set_customer(const Request& request, Response& response);

    void cfpa_set_secure_version(const Request& request, Response& response);
    void cfpa_set_key_revoke(const Request& request, Response& response);

    void cfpa_get(const Request& request, Response& response);
    void cmpa_get(const Request& request, Response& response);

    void efuse_program(const Request& request, Response& response);
    void efuse_read(const Request& request, Response& response);
    void efuse_life_cycle_read(Response& response);
    void crc_read(Response& response);

    void get_ap_fw_authenticate_data(const Request& request, Response& response);
    void set_ap_fw_authenticate_data(const Request& request, Response& response);

    // @WAR GFWLYNT1-213 Semaphore
    static nv::ipc::Queue::Status semaphore_take(nv::ipc::Queue::Usecs timeout = FlashTimeout);
    static nv::ipc::Queue::Status
    semaphore_give(nv::ipc::Queue::Usecs timeout   = FlashTimeout,
                   nv::ipc::CoreId       dest_core = nv::ipc::get_current_core());

    nv::ipc::Queue::Status receive_request_from_queue(const Request& request);
    nv::ipc::Queue::Status
    send_response_to_queue(Response&       response,
                           nv::ipc::CoreId dest_core = nv::ipc::get_current_core());

    nv::flash::Driver _driver;
    BackgroundCopy    _bg_handler;
    Pds               _pds;
    Npds              _npds;
    uint8_t           cfpa_write_page{};
};

}  // namespace nv::flash
