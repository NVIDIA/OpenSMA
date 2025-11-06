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
#include "nv/ipc/streambuffer.h"

namespace nv::ipc::task {
class Driver : protected sys::ipc::task::Driver
{
public:
    Driver() = default;

    static nv::ipc::task::Status init(uint32_t shared_base_c2c_buffer_address = 0,
                                      uint32_t core1_image_address            = 0);
    static nv::ipc::task::Status write(const std::span<const uint8_t> data);
    nv::ipc::task::Status
                                 read(std::span<uint8_t>           data,
                                      uint32_t&                    read_size,
                                      nv::ipc::StreamBuffer::Usecs timeout = nv::ipc::StreamBuffer::Usecs::max());
    static size_t                bytes_available_to_read(bool is_tx);
    static size_t                bytes_available_to_write(bool is_tx);
    static nv::ipc::task::Status notify(EventType event, uint16_t send_data);
    static nv::ipc::task::Status register_event(EventType event);

    // Get c2c id and handle
    static nv::ipc::StreamBuffer::IdType get_c2c_id(bool is_tx);
    static StreamBufferHandle_t          get_c2c_handle(bool is_tx);
    // For knowing if peer core is ready to receive interrupt
    static bool get_peer_core_interrupt_ready();
    static void set_peer_core_interrupt_ready(bool ready);

    // Core1 fault buffer management
    static void     set_shared_memory_base_address(uint32_t address);
    static uint32_t get_shared_memory_base_address();
    static uint8_t* get_core1_fault_buffer();

    // Init c2c communication
    static void init_c2c_communication(uint32_t base_addr            = 0,
                                       bool     direct_memory_access = false);
};
}  // namespace nv::ipc::task