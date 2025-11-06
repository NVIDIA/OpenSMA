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
#include <chrono>
#include <cstdint>
#include <span>

#include "nv/common/expected.h"
#include "nv/ipc/object.h"
#include "sys/ipc/streambuffer.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

/**
 * An abstracted Event interface.
 *
 * The non os-specific implementation can be found in nv/ipc/event.cpp,
 * while the os-specific implementation can be found in sys/{os}/sys/ipc/event.{h,cpp}
 */
class StreamBuffer
: public Object
, sys::ipc::StreamBuffer
{
public:
    enum class Status
    {
        Ok,                       ///< Success
        InsufficientPermissions,  ///< Calling task had no permission for event operation.
        Timeout,                  ///< A timeout occured.
        InvalidParam,             ///< One of the calling parameters was deemed invalid.
        Unknown,                  ///< An unknown error has occured.
    };

    using IdType    = sys::ipc::StreamBufferIdType;
    using InfoType  = sys::ipc::StreamBufferInfoType;
    using Item      = std::span<uint8_t>;
    using ConstItem = std::span<const uint8_t>;
    using Usecs     = std::chrono::microseconds;
    static StreamBuffer& make(IdType   id,
                              bool     is_stream_buffer     = false,
                              uint32_t base_addr            = 0,
                              bool     direct_memory_access = false);

    static StreamBufferHandle_t get_stream_buffer_handle(IdType id);
    static void set_stream_buffer_handle(IdType id, StreamBufferHandle_t handle);

    /// Return the id associated with this stream buffer.
    constexpr IdType id() const { return std::get<0>(Info); }

    constexpr size_t buffer_size() const { return std::get<1>(Info); }

    /// Number of bytes remaining in stream buffer.
    static std::size_t bytes_available_to_read(StreamBufferHandle_t handle);
    static std::size_t bytes_available_to_write(StreamBufferHandle_t handle);

    StreamBufferHandle_t get_stream_buffer_handle_by_core();

    /**
     * Enqueue an item with an optional timeout.
     *
     * The item must be a valid accessable address and item_size() in length.
     * @param[in]   item    Item to be emplaced in the stream buffer.
     * @param[in]   timeout Wait wait timeout microseconds for a stream buffer slot to free.
     * @return      size of the item sent.
     */
    [[nodiscard]] static size_t send(StreamBufferHandle_t handle, const ConstItem& item);

    /**
     * Receive an item from the stream buffer with an optional timeout.
     *
     * The item must be a valid and accessible address and item_size() in length.
     *
     * @param[out] item    Span into which the item will be placed.
     * @param[in]  timeout Will wait timeout microseconds for an item to arrive in the stream
     * buffer.
     * @return      size of the item received.
     */
    [[nodiscard]] static size_t
    recv(StreamBufferHandle_t handle, Item& item, Usecs timeout = Usecs::max());

    // For interrupt handler to notify the ipc_task that the send is done
    static void send_completed_isr(StreamBufferHandle_t handle);

    /// For static array initialization in supervisor only.
    StreamBuffer() noexcept : sys::ipc::StreamBuffer() {}

private:
    // is_stream_buffer is used to define stream buffer or message buffer
    // We only use message buffer
    StreamBuffer(IdType id, bool is_stream_buffer = false);
    const InfoType Info;
    bool           _is_stream_buffer = false;
};

}  // namespace nv::ipc
