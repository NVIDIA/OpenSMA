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
, Info(std::make_tuple(id, 0))
, _is_stream_buffer(is_stream_buffer)
{}

size_t StreamBuffer::send([[maybe_unused]] StreamBufferHandle_t handle,
                          [[maybe_unused]] const ConstItem&     item)
{
    return 0;
}

size_t StreamBuffer::recv([[maybe_unused]] StreamBufferHandle_t handle,
                          [[maybe_unused]] StreamBuffer::Item&  item)
{
    return 0;
}

std::size_t StreamBuffer::bytes_available_to_read([[maybe_unused]] StreamBufferHandle_t handle)
{
    return 0;
}

std::size_t StreamBuffer::bytes_available_to_write([[maybe_unused]] StreamBufferHandle_t handle)
{
    return 0;
}