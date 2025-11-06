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
#include "nv/ipc/streambuffer.h"
#include "nv/ipc/driver.h"
#include "nv/ipc/supervisor.h"
#include "nv/nv.h"

using namespace nv::ipc;

StreamBuffer& StreamBuffer::make(IdType   id,
                                 bool     is_stream_buffer,
                                 uint32_t base_addr,
                                 bool     direct_memory_access)
{
    StreamBuffer* obj_mem = nullptr;
    // coverity[unsigned_compare] - id should be less than NumStreamBuffers
    if (static_cast<uint32_t>(id) < sys::ipc::NumStreamBuffers) {
        obj_mem = Supervisor::inst().memory_for(id, base_addr, direct_memory_access);
    }

    nv::always_assert(obj_mem != nullptr);
    if (obj_mem->obj_is_allocated || direct_memory_access) {
        return *obj_mem;
    }

    // coverity[cert_mem54_cpp_violation] - obj_mem points to sufficient pre-allocated storage
    auto obj = new (obj_mem) StreamBuffer(id, is_stream_buffer);  // NOLINT (*-owning-memory)
    obj->obj_is_allocated = true;
    return *obj;
}

StreamBufferHandle_t StreamBuffer::get_stream_buffer_handle(IdType id)
{
    auto handle_ptr = Supervisor::inst().memory_for(id);
    if (handle_ptr != nullptr) {
        return *handle_ptr;
    }
    return nullptr;
}

void StreamBuffer::set_stream_buffer_handle(IdType id, StreamBufferHandle_t handle)
{
    auto handle_ptr = Supervisor::inst().memory_for(id);
    if (handle_ptr != nullptr) {
        *handle_ptr = handle;
    }
}