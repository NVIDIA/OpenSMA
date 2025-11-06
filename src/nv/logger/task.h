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
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/logger/log.h"
#include "nv/uart/driver.h"

namespace nv::logger {

class Task : public nv::ipc::Task
{
public:
    static void make();
    static void entrypoint(void* params);
    Task() noexcept;

    void start();

    static nv::logger::Status
    request(const Item& request, Item& response, bool wait, nv::ipc::Queue::Usecs timeout);

    static void request(const AsciiArr& ascii_arr);

    static nv::logger::Status request_from_isr(const Item& request);

    static nv::logger::Status download(Dlreq& dl_req, nv::ipc::Queue::Usecs timeout);

    static nv::logger::Status clean();
    static void               wdt_notify();
    Logger&                   get_logger_obj() { return this->_logger; };

private:
    bool log_signing_class();
    bool log_debug_token();

    nv::ipc::Event& _event;
    nv::ipc::Queue& _request;
    nv::ipc::Queue& _response;

    nv::ipc::Queue& download_queue;
    nv::ipc::Queue& download_resp_queue;

    nv::ipc::Queue& isr_queue;

    nv::ipc::Queue::Status receive_request_from_queue(const Item& item_req);
    nv::ipc::Queue::Status send_response_to_queue(Item& item_resp);

    nv::ipc::Queue::Status receive_request_from_queue(const Dlreq& dl_req);
    nv::ipc::Queue::Status send_response_to_queue(Dlreq& dl_req);

    nv::logger::Status dump_uart(const Item& request);

    void dump_ascii(const AsciiArr& ascii_arr);

    Logger           _logger;
    nv::uart::Driver _uart_driver;

    std::array<uint8_t, DumpBufferSize> dump_buffer{};
};

}  // namespace nv::logger