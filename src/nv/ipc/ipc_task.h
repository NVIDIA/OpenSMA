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
#include "sys/ipc/driver.h"

namespace nv::ipc::task {
constexpr std::chrono::microseconds C2CReceiveTimeout = std::chrono::seconds(1);  // 1 second
constexpr std::chrono::microseconds SendQueueTimeout  = std::chrono::seconds(1);  // 1 second
class Task : public ipc::Task
{
public:
    // core1_image_address & shared_base_c2c_memory_address are used only by core0,
    // nonsensical for core1
    struct Config
    {
        nv::ipc::TaskId  task_id;
        std::string_view task_name;
        uint32_t         core1_image_address;
        uint32_t         shared_base_c2c_memory_address;
        Config(nv::ipc::TaskId  task_id_,
               std::string_view task_name_,
               uint32_t         core1_image_address_            = 0,
               uint32_t         shared_base_c2c_memory_address_ = 0)
        : task_id(task_id_)
        , task_name(task_name_)
        , core1_image_address(core1_image_address_)
        , shared_base_c2c_memory_address(shared_base_c2c_memory_address_)
        {}
    };
    Task(Config config) noexcept;
    static void make(Config config);
    static void entrypoint(void* params);

    [[noreturn]] void start();

    // Functions called in queue.cpp & event.cpp
    static nv::ipc::task::Status
    handle_queue_data(const ipc::Queue::ConstItem& item, ipc::QueueId queue_id, bool is_front);
    static nv::ipc::task::Status
    handle_event_data(ipc::EventId event_id, bool is_set, uint32_t event_bits);

private:
    uint32_t              _core1_image_address;
    uint32_t              _shared_base_c2c_memory_address;
    nv::ipc::task::Driver _driver;
    bool                  _is_queue_item_data_pending = false;
    QueueRequest          _queue_request{};
    // To read data from c2c buffer
    alignas(4) std::array<uint8_t, sys::ipc::task::C2CBufferSize> _buffer{};

    // Functions called by ipc_task to handle data read from c2c memory
    nv::ipc::task::Status handle_data_read(uint32_t read_size);
};
}  // namespace nv::ipc::task