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
#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <message_buffer.h>

#include "nv/common/utils.h"
#include "nv/ipc/streambuffer.h"
#include NV_IPC_CONFIG_H

namespace sys::ipc {
using StreamBufferIdType                         = uint32_t;
using StreamBufferInfoType                       = std::tuple<StreamBufferIdType, uint32_t>;
constexpr uint32_t StreamBufferStart             = 0;
constexpr uint32_t StreamBufferEnd               = 0;
constexpr uint32_t NumStreamBuffers              = StreamBufferEnd - StreamBufferStart;
constexpr uint32_t StreamBufferSize              = 0;
constexpr uint32_t StreamBufferAlignment         = 4;
constexpr uint32_t StreamBufferInternalAlignment = 0;
/**
 * OS specific data of the Posix FreeRTOS specific static implementation of a IPC stream buffer.
 */
class StreamBuffer
{
public:
    StreamBuffer() noexcept = default;

    /// Access the stream buffer's underlying handle.
    auto& handle() const { return _stream_buffer; }

    StreamBufferHandle_t _stream_buffer = nullptr;

    /// storage for static stream buffer
    std::array<uint8_t, StreamBufferSize> static_stream_buffer_buffer;

    /// freertos static stream buffer struct storage
    StaticStreamBuffer_t static_stream_buffer;
};

}  // namespace sys::ipc
