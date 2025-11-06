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
// TODO: Define a reasonable time for waiting c2c has enough space
constexpr uint32_t WaitForC2CHasEnoughSpaceTimeOut = 500;
constexpr uint32_t Core1StartupTimeout             = 1000000;  // 1s
constexpr uint32_t C2CBufferSize                   = nv::ipc::C2CBufferSize;
constexpr uint32_t C2CUsbPldmUpdate4KSize          = nv::ipc::C2CUsbPldmUpdate4KSize;
constexpr uint32_t C2CBufferExtraSpace             = nv::ipc::C2CBufferExtraSpace;
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

    static constexpr bool can_direct_access_on_current_core(nv::ipc::CoreId id_core)
    {
        return (id_core == nv::ipc::CoreId::Both) || (nv::ipc::get_current_core() == id_core);
    }
    static constexpr bool can_cross_core_access(nv::ipc::CoreId id_core)
    {
        return (id_core == nv::ipc::CoreId::Both)
            // get_current_core() == nv::ipc::CoreId::Core1 will always be false for Core0
            // coverity[result_independent_of_operands]
            || (nv::ipc::get_current_core() == nv::ipc::CoreId::Core1
                && id_core == nv::ipc::CoreId::Core0)
            || (nv::ipc::get_current_core() == nv::ipc::CoreId::Core0
                && id_core == nv::ipc::CoreId::Core1);
    }

    // Check if task, queue, event, timer, mctp client can be accessed directly on the current
    // core
    static bool can_direct_access_on_current_core(nv::ipc::TaskId task_id);
    static bool can_direct_access_on_current_core(nv::ipc::QueueId queue_id);
    static bool can_direct_access_on_current_core(nv::ipc::EventId event_id);
    static bool can_direct_access_on_current_core(nv::ipc::TimerId timer_id);
    static bool can_direct_access_on_current_core(nv::mctp::Client client);

    // Check if queue, event can be accessed cross core
    static bool can_cross_core_access(nv::ipc::QueueId queue_id);
    static bool can_cross_core_access(nv::ipc::EventId event_id);

    // Get the CoreId from the mctp client
    static nv::ipc::CoreId get_core_from_client(nv::mctp::Client client);
};
}  // namespace sys::ipc::task
