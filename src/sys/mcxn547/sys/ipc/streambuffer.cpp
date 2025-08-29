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

#include "FreeRTOSConfig.h"

#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/nv.h"
#include "mpu_syscall_numbers.h"
#include "nv/ipc/ipc_task.h"
#include NV_IPC_CONFIG_H

using namespace nv::ipc;

StreamBuffer::StreamBuffer(const StreamBuffer::IdType id, bool is_stream_buffer)
: sys::ipc::StreamBuffer()
, Info(StreamBufferInfos.at(common::to_underlying(id)))
, _is_stream_buffer(is_stream_buffer)
{
    // We only use message buffer
    if (_is_stream_buffer == false) {
        nv::assert(common::is_in_range(id));

        const auto buf_size = std::get<1>(Info);

        _stream_buffer = xMessageBufferCreateStatic(
            buf_size, static_stream_buffer_buffer.begin(), &static_stream_buffer);

        nv::assert(_stream_buffer != nullptr);
    }
}

size_t StreamBuffer::send(StreamBufferHandle_t handle, const ConstItem& item)
{
    const bool from_isr = sys::ipc::is_in_isr();
    if (from_isr) {
        auto higher_priority_woken = pdFALSE;
        auto res                   = xMessageBufferSendFromISR(
            handle,
            item.data(),
            common::align_to(item.size(), sys::ipc::StreamBufferAlignment),
            &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken);
        return res;
    }
    else {
        return xMessageBufferSend(
            handle,
            item.data(),
            common::align_to(item.size(), sys::ipc::StreamBufferAlignment),
            0);
    }
}

size_t StreamBuffer::recv(StreamBufferHandle_t handle, StreamBuffer::Item& item)
{
    const bool from_isr = sys::ipc::is_in_isr();
    if (from_isr) {
        auto higher_priority_woken = pdFALSE;
        auto res                   = xMessageBufferReceiveFromISR(
            handle, item.data(), item.size(), &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken);
        return res;
    }
    else {
        return xMessageBufferReceive(handle, item.data(), item.size(), 0);
    }
}

std::size_t StreamBuffer::bytes_available_to_read(StreamBufferHandle_t handle)
{
    return xStreamBufferBytesAvailable(handle);
}

std::size_t StreamBuffer::bytes_available_to_write(StreamBufferHandle_t handle)
{
    return xStreamBufferSpacesAvailable(handle);
}

StreamBufferHandle_t StreamBuffer::get_stream_buffer_handle_by_core()
{
    // Handler is encoded in kernel, Core0 has MPU and knowledge to decode it
    // Core1 has no MPU, so we need to use the address of static_stream_buffer
    if (sys::ipc::task::Driver::get_current_core() == nv::ipc::CoreId::Core0) {
        return _stream_buffer;
    }
    else {
        // NOLINTNEXTLINE(*-reinterpret-cast)
        return reinterpret_cast<StreamBufferHandle_t>(&static_stream_buffer);
    }
}