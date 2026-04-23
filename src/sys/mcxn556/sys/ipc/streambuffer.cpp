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

// ARM Cortex-M memory barrier intrinsics (GCC built-ins)
#ifndef __DSB
#define __DSB() __asm volatile("dsb sy" ::: "memory")
#endif
#ifndef __ISB
#define __ISB() __asm volatile("isb sy" ::: "memory")
#endif

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

        auto c2c_buf = Supervisor::_static_c2c_buffer.begin();

        for (int i = 0; i < static_cast<int>(Supervisor::NumC2CBuffers); i++) {
            // coverity[dead_error_line] - Expect cannot reach in single core
            if (id == static_cast<StreamBufferId>(i)) {
                _stream_buffer = xMessageBufferCreateStatic(
                    buf_size, c2c_buf, &static_stream_buffer);
                break;
            }
            c2c_buf += std::get<1>(StreamBufferInfos.at(i));
        }

        // Future use: Could add other stream buffer not use for c2c, but not used now

        nv::assert(_stream_buffer != nullptr);
    }
}

size_t StreamBuffer::send(StreamBufferHandle_t handle, const ConstItem& item)
{
    if (xPortIsInsideInterrupt()) {
        auto higher_priority_woken = pdFALSE;
        auto res                   = xMessageBufferSendFromISR(
            handle,
            item.data(),
            common::align_to(item.size(), sys::ipc::StreamBufferSendAlignment),
            &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken);
        return res;
    }
    else {
        // Timeout should be 0, since it is in critical section
        return xMessageBufferSend(
            handle,
            item.data(),
            common::align_to(item.size(), sys::ipc::StreamBufferSendAlignment),
            0);
    }
}

void StreamBuffer::send_completed_isr(StreamBufferHandle_t handle)
{
    // Memory barrier to ensure we see all data written by other core
    // before waking up the waiting task
    __DSB();
    __ISB();

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    (void)xMessageBufferSendCompletedFromISR(handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

size_t StreamBuffer::recv(StreamBufferHandle_t handle, StreamBuffer::Item& item, Usecs timeout)
{
    // Memory barrier to ensure we see the latest data written by other core
    // This is critical for multi-core MessageBuffer where Core1 writes head pointer
    __DSB();

    if (xPortIsInsideInterrupt()) {
        auto higher_priority_woken = pdFALSE;
        auto res                   = xMessageBufferReceiveFromISR(
            handle, item.data(), item.size(), &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken);
        return res;
    }
    else {
        constexpr auto OneSecInUsecs = 1'000'000;
        // coverity[cert_int31_c_violation] - timeout is cast to unsigned long
        // coverity[cert_int32_c_violation]
        return xMessageBufferReceive(handle,
                                     item.data(),
                                     item.size(),
                                     configTICK_RATE_HZ * timeout.count() / OneSecInUsecs);
    }
}

std::size_t StreamBuffer::bytes_available_to_read(StreamBufferHandle_t handle)
{
    // Memory barrier to ensure we see the latest head pointer from other core
    __DSB();
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
    if (nv::ipc::get_current_core() == nv::ipc::CoreId::Core0) {
        return _stream_buffer;
    }
    else {
        // NOLINTNEXTLINE(*-reinterpret-cast)
        return reinterpret_cast<StreamBufferHandle_t>(&static_stream_buffer);
    }
}