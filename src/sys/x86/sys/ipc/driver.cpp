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
#include "nv/ipc/driver.h"
using namespace nv::ipc::task;

nv::ipc::task::Status Driver::init([[maybe_unused]] uint32_t shared_base_stream_buffer_address,
                                   [[maybe_unused]] uint32_t core1_image_address)
{
    return nv::ipc::task::Status::Ok;
}

nv::ipc::task::Status Driver::write([[maybe_unused]] const std::span<const uint8_t> data)
{
    return nv::ipc::task::Status::Ok;
}

nv::ipc::task::Status Driver::read([[maybe_unused]] std::span<uint8_t>           data,
                                   [[maybe_unused]] uint32_t&                    read_size,
                                   [[maybe_unused]] nv::ipc::StreamBuffer::Usecs timeout)
{
    return nv::ipc::task::Status::Ok;
}

size_t Driver::bytes_available_to_read([[maybe_unused]] bool is_tx)
{
    return 0;
}

size_t Driver::bytes_available_to_write([[maybe_unused]] bool is_tx)
{
    return 0;
}

nv::ipc::task::Status Driver::notify([[maybe_unused]] EventType event,
                                     [[maybe_unused]] uint16_t  send_data)
{
    return nv::ipc::task::Status::Ok;
}

nv::ipc::task::Status Driver::register_event([[maybe_unused]] EventType event)
{
    return nv::ipc::task::Status::Ok;
}

nv::ipc::StreamBuffer::IdType Driver::get_c2c_id([[maybe_unused]] bool is_tx)
{
    return 0;
}

StreamBufferHandle_t Driver::get_c2c_handle([[maybe_unused]] bool is_tx)
{
    return nullptr;
}

bool Driver::get_peer_core_interrupt_ready()
{
    return false;
}

void Driver::set_peer_core_interrupt_ready([[maybe_unused]] bool ready)
{
    return;
}

void Driver::init_c2c_communication([[maybe_unused]] uint32_t base_addr,
                                    [[maybe_unused]] bool     direct_memory_access)
{
    return;
}

nv::ipc::task::Status sys::ipc::task::Driver::write_queue_request(
    [[maybe_unused]] const nv::ipc::task::Request&    request,
    [[maybe_unused]] const nv::ipc::Queue::ConstItem& item)
{
    return nv::ipc::task::Status::Ok;
}

nv::ipc::task::Status sys::ipc::task::Driver::write_event_request(
    [[maybe_unused]] const nv::ipc::task::Request& request)
{
    return nv::ipc::task::Status::Ok;
}

nv::ipc::CoreId
sys::ipc::task::Driver::get_core_from_client([[maybe_unused]] nv::mctp::Client client)
{
    return nv::ipc::CoreId::Core0;
}