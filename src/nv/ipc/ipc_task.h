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
#include "nv/ipc/task.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/event.h"
#include "nv/ipc/common.h"
#include "nv/ipc/driver.h"

namespace nv::ipc::task {
class Task : public ipc::Task
{
public:
    // core1_image_address & shared_base_stream_buffer_address & shared_stream_buffer_size are
    // used only by core0, nonsensical for core1
    struct Config
    {
        nv::ipc::TaskId  task_id;
        std::string_view task_name;
        ipc::EventId     event_id;
        ipc::QueueId     start_up_queue_id;
        uint32_t         core1_image_address;
        uint32_t         shared_base_stream_buffer_address;
        uint32_t         shared_stream_buffer_size;
        Config(nv::ipc::TaskId  task_id_,
               std::string_view task_name_,
               ipc::EventId     event_id_,
               ipc::QueueId     start_up_queue_id_,
               uint32_t         core1_image_address_               = 0,
               uint32_t         shared_base_stream_buffer_address_ = 0,
               uint32_t         shared_stream_buffer_size_         = 0)
        : task_id(task_id_)
        , task_name(task_name_)
        , event_id(event_id_)
        , start_up_queue_id(start_up_queue_id_)
        , core1_image_address(core1_image_address_)
        , shared_base_stream_buffer_address(shared_base_stream_buffer_address_)
        , shared_stream_buffer_size(shared_stream_buffer_size_)
        {}
    };
    Task(Config config) noexcept;
    static void make(Config config);
    static void entrypoint(void* params);

    [[noreturn]] void start();

    // Functions called by queue.cpp & event.cpp
    static nv::ipc::task::Status
    handle_queue_data(const ipc::Queue::ConstItem& item, ipc::QueueId queue_id, bool is_front);
    static nv::ipc::task::Status
    handle_event_data(ipc::EventId event_id, bool is_set, uint32_t event_bits);

    static void debug_core(const char* fmt, auto&&... args)
    {
#if defined(CPU_MCXN547VDF)
        nv::common::debug_core_wrapper(fmt, args...);
#endif
    }

private:
    nv::ipc::EventId      _event_id;
    nv::ipc::QueueId      _start_up_queue_id;
    uint32_t              _core1_image_address;
    uint32_t              _shared_base_stream_buffer_address;
    uint32_t              _shared_stream_buffer_size;
    bool                  _peer_core_is_ready;
    nv::ipc::task::Driver _driver;
    bool                  _is_queue_item_data_pending = false;
    Request               _request{};
    // To read data from streambuffer
    alignas(4) std::array<uint8_t, sys::ipc::StreamBufferSize> _buffer{};

    void set_up_core_communication();

    // Functions called by ipc_task to handle data read from streambuffer
    nv::ipc::task::Status handle_data_read(uint32_t read_size);
};
}  // namespace nv::ipc::task