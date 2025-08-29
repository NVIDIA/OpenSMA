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
#include "mcmgr.h"
#include "nv/ipc/common.h"
#include "nv/ipc/queue.h"

namespace sys::ipc::task {
// TODO: Define a reasonable time for waiting streambuffer has enough space
constexpr uint32_t WaitForStreamBufferHasEnoughSpaceTime = 500;
class Driver
{
public:
    static nv::ipc::task::Status write_queue_request(const nv::ipc::task::Request&    request,
                                                     const nv::ipc::Queue::ConstItem& item);
    static nv::ipc::task::Status write_queue_request_svc(const nv::ipc::task::Request& request,
                                                         const nv::ipc::Queue::ConstItem& item);
    static nv::ipc::task::Status
                                 write_queue_request_impl(const nv::ipc::task::Request&    request,
                                                          const nv::ipc::Queue::ConstItem& item);
    static nv::ipc::task::Status write_event_request(const nv::ipc::task::Request& request);
    static nv::ipc::task::Status write_event_request_svc(const nv::ipc::task::Request& request);
    static nv::ipc::task::Status
    write_event_request_impl(const nv::ipc::task::Request& request);

    static nv::ipc::task::Status wait_for_enough_space(size_t                    size,
                                                       std::chrono::microseconds timeout);

    static constexpr nv::ipc::CoreId get_current_core()
    {
#if defined(CPU_MCXN547VDF_cm33_core0)
        return nv::ipc::CoreId::Core0;
#else
        return nv::ipc::CoreId::Core1;
#endif
    }
    static constexpr bool is_on_same_core(nv::ipc::CoreId id_core)
    {
        return (id_core == nv::ipc::CoreId::Both) || (get_current_core() == id_core);
    }
    // Check if task, queue, event, timer is on the same core by id
    static bool is_on_same_core(nv::ipc::TaskId task_id);
    static bool is_on_same_core(nv::ipc::QueueId queue_id);
    static bool is_on_same_core(nv::ipc::EventId event_id);
    static bool is_on_same_core(nv::ipc::TimerId timer_id);
};
}  // namespace sys::ipc::task
