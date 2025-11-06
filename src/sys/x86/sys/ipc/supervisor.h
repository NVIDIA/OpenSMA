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
#include <array>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <timers.h>

#include "nv/common/utils.h"
#include NV_IPC_CONFIG_H

namespace sys::ipc {

/// compile time helper to calculate the size of the queue static buffer array
constexpr auto calc_static_buf_size() noexcept
{
    std::size_t total = 0;
    for (const auto& [id, len, s] : nv::ipc::QueueInfos) {
        // coverity[cert_int31_c_violation] - valid cast
        std::size_t siz = len * s;
        // coverity[cert_int30_c_violation] - will not wrap (total + siz)
        total += siz;
    }
    return total;
}

/// compile time helper to calculate the size of the c2c static buffer array
constexpr auto calc_static_c2c_buf_size() noexcept
{
    return 0;
}

struct Supervisor
{
#ifdef NV_UNITTEST
    int          argc;
    const char** argv;
#endif

    /// storage for static queue
    std::array<uint8_t, calc_static_buf_size()> static_queue_buffer;

    /// freertos static queue struct storage
    std::array<StaticQueue_t, nv::ipc::QueueInfos.size()> static_queues;

    /// freertos static event struct storage
    std::array<StaticEventGroup_t, nv::common::to_underlying(nv::ipc::EventId::End)>
        static_events;

    /// freertos static timer struct storage
    std::array<StaticTimer_t, nv::common::to_underlying(nv::ipc::TimerId::End)> static_timers;
};

uint32_t get_os_ticks();

bool is_scheduler_run();
bool is_in_isr();

}  // namespace sys::ipc
