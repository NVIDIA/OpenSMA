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
#include "sys/ipc/driver.h"
#include NV_IPC_CONFIG_H

namespace sys::ipc {

/// compile time helper to calculate the size of the queue static buffer array
constexpr auto calc_static_buf_size() noexcept
{
    std::size_t total = 0;
    for (const auto& [id, len, s, core] : nv::ipc::QueueInfos) {
        if (sys::ipc::task::Driver::can_direct_access_on_current_core(core)) {
            // coverity[cert_int31_c_violation] - valid cast
            std::size_t siz = len * s;
            // coverity[cert_int30_c_violation] - will not wrap (total + siz)
            total += siz;
        }
    }
    return total;
}

/// compile time helper to calculate the size of the c2c static buffer array
constexpr auto calc_static_c2c_buf_size() noexcept
{
    auto        current_core = nv::ipc::get_current_core();
    std::size_t total        = 0;
    // Only core0 needs to calculate the size of the c2c static buffer
    if (current_core == nv::ipc::CoreId::Core0) {
        for (const auto& [id, size] : nv::ipc::StreamBufferInfos) {
            if (id < nv::ipc::StreamBufferId::C2CEnd) {
                // coverity[cert_int30_c_violation] - will not wrap (total + size)
                total += size;
            }
        }
    }
    return total;
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
