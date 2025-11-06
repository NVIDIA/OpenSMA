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
#include "nv/ipc/queue.h"

#include "FreeRTOSConfig.h"

#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/nv.h"
#include NV_IPC_CONFIG_H

using namespace nv::ipc;

Queue::Queue(const QueueId id)
: sys::ipc::Queue()
, Info(QueueInfos.at(common::to_underlying(id)))
{
    nv::assert(common::is_in_range(id));

    auto& super = Supervisor::inst();
    // calculate the offset into the static queues' buffer
    for (auto buf = super.static_queue_buffer.begin();
         const auto& [qid, len, item_size] : QueueInfos) {
        if (id == qid) {
            _queue = xQueueCreateStatic(
                len, item_size, buf, &super.static_queues.at(common::to_underlying(id)));
            break;
        }
        buf += len * item_size;
    }

    nv::assert(_queue != nullptr);
}

Queue::Status
Queue::send(const ConstItem& item, Usecs timeout, [[maybe_unused]] CoreId dest_core)
{
    // pre-conditions
    if (item.size() != item_size()) {
        return Status::InvalidParam;
    }

    constexpr auto OneSecInUsecs = 1'000'000;
    // coverity[cert_int32_c_violation] - timeout is cast to unsigned long
    const auto Ticks = configTICK_RATE_HZ * timeout.count() / OneSecInUsecs;
    // coverity[cert_int31_c_violation] - valid cast
    auto res = xQueueSendToBack(_queue, item.data(), Ticks);
    // Translate the result
    switch (res) {
        case pdTRUE: return Status::Ok;
        case errQUEUE_FULL:
            return Status::Full;
            // GBS:BEGIN NO COVERAGE FIXME!!
        default: break;
    }
    return Status::Unknown;
    // GBS:END NO COVERAGE FIXME!!
}
Queue::Status Queue::send_isr(const ConstItem& item)
{
    // pre-conditions
    if (item.size() != item_size()) {
        return Status::InvalidParam;
    }

    auto higher_priority_woken = pdFALSE;
    auto res = xQueueSendToBackFromISR(_queue, item.data(), &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
    return res == pdFALSE ? Status::Unknown : Status::Ok;
}

Queue::Status Queue::send_front(const ConstItem& item, Usecs timeout)
{
    // pre-conditions
    if (item.size() != item_size()) {
        return Status::InvalidParam;
    }

    constexpr auto OneSecInUsecs = 1'000'000;
    // coverity[cert_int32_c_violation] dont care tick lost
    const auto Ticks = configTICK_RATE_HZ * timeout.count() / OneSecInUsecs;
    // coverity[cert_int31_c_violation] dont care tick lost
    auto res = xQueueSendToFront(_queue, item.data(), Ticks);
    // Translate the result
    switch (res) {
        case pdTRUE: return Status::Ok;
        case errQUEUE_FULL:
            return Status::Full;
            // GBS:BEGIN NO COVERAGE FIXME!!
        default: break;
    }
    return Status::Unknown;
    // GBS:END NO COVERAGE FIXME!!
}

Queue::Status Queue::send_front_isr(const ConstItem& item)
{
    // pre-conditions
    if (item.size() != item_size()) {
        return Status::InvalidParam;
    }

    auto higher_priority_woken = pdFALSE;
    auto res = xQueueSendToFrontFromISR(_queue, item.data(), &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
    return res == pdFALSE ? Status::Unknown : Status::Ok;
}

Queue::Status Queue::recv(Item& item, Usecs timeout)
{
    // pre-conditions
    if (item.size() != item_size()) {
        return Status::InvalidParam;
    }

    constexpr auto OneSecInUsecs = 1'000'000;
    // coverity[cert_int31_c_violation] - timeout is cast to unsigned long
    return (xQueueReceive(
               // coverity[cert_int32_c_violation] - valid cast
               _queue,
               item.data(),
               // coverity[cert_int32_c_violation] - valid cast
               configTICK_RATE_HZ * timeout.count() / OneSecInUsecs))
             ? Status::Ok
             : Status::Timeout;
}

Queue::Status Queue::recv_isr(Item& item)
{
    // pre-conditions
    if (item.size() != item_size()) {
        return Status::InvalidParam;
    }

    auto higher_priority_woken = pdFALSE;
    auto res = xQueueReceiveFromISR(_queue, item.data(), &higher_priority_woken);
    if (higher_priority_woken != false) {
        taskYIELD();  // GBS: NO COVERAGE FIXME!!
    }
    return res == pdFALSE ? Status::Unknown : Status::Ok;
}

std::size_t Queue::size() const
{
    // coverity[cert_int30_c_violation] - dont care wrap
    return max_items() - available();
}

std::size_t Queue::available() const
{
    return uxQueueSpacesAvailable(_queue);
}
