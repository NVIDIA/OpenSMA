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
#include "nv/ipc/supervisor.h"

#include "nv/common/preproc.h"
#include "nv/common/utils.h"
#include NV_IPC_CONFIG_H
#include "nv/ipc/task.h"
#include "nv/nv.h"

#ifdef NV_UNITTEST
#include "nv/ut/task.h"
#endif

using namespace nv;
using namespace nv::ipc;

namespace {
NV_SHARED_BSS Supervisor supervisor;  // NOLINT(*-non-const-global-variables)
}
#ifdef CPU_MCXN547VDF_cm33_core0
// Only declare stream buffer in core0 of MCXN547
__attribute__((section(".stream_buffer_data"),
               aligned(0x1000))) std::array<StreamBuffer, Supervisor::NumStreamBuffers>
                                 Supervisor::_stream_buffers;
#endif

Supervisor& Supervisor::inst()
{
    // TODO: Should the supervisor be created on demand using a specific carveout?
    return supervisor;
}

void Supervisor::startup([[maybe_unused]] int         argc,
                         [[maybe_unused]] const char* argv[])  // NOLINT(*-c-arrays)
{
#ifdef NV_UNITTEST
    // create the unittesting task if we are unittesting.
    this->argc = argc;
    this->argv = argv;
    ut::Task::make();
#endif

    // Ensure _tasks and _tasks_pub have the same size
    static_assert(std::tuple_size_v<decltype(_tasks)>
                      == std::tuple_size_v<decltype(_tasks_pub)>,
                  "_tasks and _tasks_pub arrays must have the same size");

    // MPU V2 request to create eveything in main
    for (int i = 0; i < static_cast<int>(ipc::EventId::End); i++) {
        ipc::Event::make(static_cast<ipc::EventId>(i));
    }

    for (int i = 0; i < static_cast<int>(ipc::QueueId::End); i++) {
        ipc::Queue::make(static_cast<ipc::QueueId>(i));
    }

// Only make stream buffer in core0 of MCXN547
#ifdef CPU_MCXN547VDF_cm33_core0
    // To avoid make stream buffer in single core of MCXN547
    if (sys::ipc::StreamBufferSize > 0) {
        for (int i = 0; i < static_cast<int>(ipc::StreamBufferId::End); i++) {
            auto& stream_buffer = ipc::StreamBuffer::make(static_cast<ipc::StreamBufferId>(i),
                                                          false);
            ipc::StreamBuffer::set_stream_buffer_handle(
                static_cast<ipc::StreamBufferId>(i),
                stream_buffer.get_stream_buffer_handle_by_core());
        }
    }
#endif

    for (int id = 0; auto& task : _tasks) {
        if (task == nullptr) {
            // TODO: still in development, otherwise this is an error.
            // GBS:BEGIN NO COVERAGE FIXME!!
            nv::warn("task with id '%d' has not been registered with supervisor\n", id);
            // GBS:END NO COVERAGE FIXME!!
        }
        else {
            // initial public information - ensure safe enum cast
            // coverity[cert_int31_c_violation] - valid cast
            _tasks_pub.at(common::to_underlying(static_cast<ipc::TaskId>(id)))
                .handle() = task->handle();
            // coverity[cert_int31_c_violation] - valid cast
            _tasks_pub.at(common::to_underlying(static_cast<ipc::TaskId>(id)))
                .id() = task->id();
            // coverity[cert_int31_c_violation] - valid cast
            _tasks_pub.at(common::to_underlying(static_cast<ipc::TaskId>(id))).task = task;
        }

        // coverity[cert_int32_c_violation] - valid cast
        id++;
    }
}

void Supervisor::shutdown()
{
    // TODO: we should implement clean shutdown/exit here.
    for (auto& task : _tasks) {
        if (task != nullptr) {
            task->~Task();
            task = nullptr;
        }
    }
}

void Supervisor::register_task(Task& task)
{
    // don't register the fake 'root' task.
    if (task.id() != TaskId::End) {
        nv::info("registering task '%s:%d'\n", task.name().data(), task.id());
        _tasks.at(common::to_underlying(task.id())) = &task;
    }
}

Event* Supervisor::memory_for(EventId id)
{
    auto idx = common::to_underlying(id);
    return &_events.at(idx);
}

Queue* Supervisor::memory_for(QueueId id)
{
    auto idx = common::to_underlying(id);
    return &_queues.at(idx);
}

StreamBuffer*
Supervisor::memory_for(StreamBuffer::IdType id, uint32_t base_addr, bool direct_memory_access)
{
    auto idx = static_cast<uint32_t>(id);
    // If direct memory access is requested, return the stream buffer address
    if (direct_memory_access) {
        nv::assert(idx <= UINT32_MAX / sizeof(StreamBuffer));
        nv::assert(base_addr <= UINT32_MAX - idx * sizeof(StreamBuffer));

        auto stream_buffer_address = base_addr + idx * sizeof(StreamBuffer);
        // NOLINTNEXTLINE(*-reinterpret-cast)
        return reinterpret_cast<StreamBuffer*>(stream_buffer_address);
    }
    else {
        // Only make stream buffer in core0 of MCXN547
#ifdef CPU_MCXN547VDF_cm33_core0
        return &_stream_buffers.at(idx);
#else
        // Shoud not happen, core1 should always use direct memory access
        nv::assert(false);
        return nullptr;
#endif
    }
}

StreamBufferHandle_t* Supervisor::memory_for(StreamBuffer::IdType id)
{
    auto idx = static_cast<uint32_t>(id);
    // coverity[unsigned_compare] - id should be less than StreamBufferEnd
    if (static_cast<uint32_t>(id) < sys::ipc::StreamBufferEnd) {
        return &_stream_buffer_handles.at(idx);
    }
    else {
        return nullptr;
    }
}

Timer* Supervisor::memory_for(TimerId id)
{
    auto idx = common::to_underlying(id);
    return &_timers.at(idx);
}

uint32_t Supervisor::get_os_ticks()
{
    return sys::ipc::get_os_ticks();
}

bool Supervisor::is_scheduler_run()
{
    return sys::ipc::is_scheduler_run();
}

bool Supervisor::is_in_isr()
{
    return sys::ipc::is_in_isr();
}