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
#include <span>
#include "sys/ipc/driver.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/streambuffer.h"

namespace nv::ipc::task {
class Driver : protected sys::ipc::task::Driver
{
public:
    Driver() = default;

    // Get and set shared data address
    static uint32_t get_shared_base_stream_buffer_address();
    static void     set_shared_base_stream_buffer_address(uint32_t address);

    // McMGR related functions
    static nv::ipc::task::Status         init(uint32_t shared_base_stream_buffer_address = 0,
                                              uint32_t core1_image_address               = 0);
    static nv::ipc::task::Status         write(const std::span<const uint8_t> data);
    nv::ipc::task::Status                read(std::span<uint8_t> data, uint32_t& read_size);
    static size_t                        bytes_available_to_read(bool is_tx = false);
    static size_t                        bytes_available_to_write(bool is_tx = false);
    static nv::ipc::task::Status         notify(EventType event, uint16_t send_data);
    static nv::ipc::task::Status         register_event(EventType event);
    static nv::ipc::StreamBuffer::IdType get_stream_buffer_id(bool is_tx);
    static StreamBufferHandle_t          get_stream_buffer_handle(bool is_tx);
    // For setup core communication
    static bool get_core_communication_ready();
    static void set_core_communication_ready(bool ready);

    static Status send_start_up_queue(ipc::QueueId queue_id, CmdCode cmd, bool isr = false);
    static Status set_event(ipc::EventId event_id, EventBits event_bits, bool isr = false);
};
}  // namespace nv::ipc::task