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

#include "nv/ipc/object.h"
#include "sys/ipc/queue.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

/**
 * An abstracted Interprocess/task queue interface.
 *
 * The non os-specific implementation can be found in nv/ipc/queue.cpp,
 * while the os-specific implementation can be found in sys/{os}/sys/ipc/queue.{h,cpp}
 */
class Queue
: public Object
, sys::ipc::Queue
{
public:
    enum class Status
    {
        Ok,                       ///< Success
        InsufficientPermissions,  ///< Calling task had no permission for queue operation.
        Timeout,                  ///< A timeout occurred.
        InvalidParam,             ///< One of the calling parameters was deemed invalid.
        Empty,                    ///< Attemtped to retrieve from an empty queue.
        Full,                     ///< Attempted to send to a full queue.
        Unknown,                  ///< An unknown error occurred.
        InvalidOperation,         ///< The operation is not allowed on the current core.
        CommunicationFailed,      ///< Communication failed.
    };

    using IdType    = QueueId;
    using Item      = std::span<uint8_t>;
    using ConstItem = std::span<const uint8_t>;
    using Usecs     = std::chrono::microseconds;

    /// Allocate or access objects by Id only by this method
    static Queue& make(QueueId id);

    /// Return the id associated with this queue.
    constexpr QueueId id() const { return std::get<0>(Info); }

    /// The queue's item size.
    constexpr std::size_t item_size() const { return std::get<2>(Info); }

    /// Maxiumum possible number of elements that can be placed in the queue.
    constexpr std::size_t max_items() const { return std::get<1>(Info); }

    /// Number of items currently in queue.
    std::size_t size() const;

    /// Number of item spaces remaining in queue.
    std::size_t available() const;

    /**
     * Enqueue an item with an optional timeout.
     *
     * The item must be a valid accessable address and item_size() in length.
     * @param[in]   item    Item to be emplaced in the queue.
     * @param[in]   timeout Wait wait timeout microseconds for a queue slot to free.
     * @param[in]   dest_core The core to send the item to. If not specified, send if can access
     * on current core. If specified, check if dest and current core are the same.
     * @return      A valid Queue::Status.
     */
    [[nodiscard]] Status send(const ConstItem& item,
                              Usecs            timeout   = Usecs::max(),
                              CoreId           dest_core = CoreId::Invalid);
    [[nodiscard]] Status send_isr(const ConstItem& item);

    /**
     * Receive an item from the queue with an optional timeout.
     *
     * The item must be a valid and accessible address and item_size() in length.
     *
     * @param[out] item    Span into which the item will be placed.
     * @param[in]  timeout Will wait timeout microseconds for an item to arrive in the queue.
     * @return      A valid Queue::Status.
     */
    [[nodiscard]] Status recv(Item& item, Usecs timeout = Usecs::max());
    [[nodiscard]] Status recv_isr(Item& item);

    /**
     * Enqueue an item with an optional timeout.
     *
     * The item must be a valid accessable address and item_size() in length.
     * @param[in]   item    Item to be emplaced in the queue.
     * @param[in]   timeout Wait wait timeout microseconds for a queue slot to free.
     * @return      A valid Queue::Status.
     */
    [[nodiscard]] Status send_front(const ConstItem& item, Usecs timeout = Usecs::max());
    [[nodiscard]] Status send_front_isr(const ConstItem& item);

    /// Null constructor for storage initialization only.
    Queue() noexcept : sys::ipc::Queue() {}

    void reset();

private:
    Queue(QueueId id);
    const QueueInfo Info;
};

constexpr std::size_t get_queue_max_items(nv::ipc::QueueId id)
{
    for (const auto& info : nv::ipc::QueueInfos) {
        if (std::get<0>(info) == id) {
            return std::get<1>(info);
        }
    }
    // You can make this return a default or cause a compile error
    return 0;  // Fallback (optional)
}

}  // namespace nv::ipc
