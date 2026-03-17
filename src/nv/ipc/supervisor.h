/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include "nv/common/utils.h"
#include "nv/ipc/event.h"
#include "nv/ipc/mutex.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/streambuffer.h"
#include "nv/ipc/task.h"
#include "nv/ipc/timer.h"
#include "sys/ipc/supervisor.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

/**
 * All privileged code that is expected to run as 'root' should be part of this
 * class.
 */
class Supervisor
: public sys::ipc::Supervisor
, common::NonCopyable
{
public:
    constexpr static auto NumTasks  = common::to_underlying(TaskId::End);
    constexpr static auto NumEvents = common::to_underlying(EventId::End);
    constexpr static auto NumQueues = common::to_underlying(QueueId::End);
    constexpr static auto NumTimers = common::to_underlying(TimerId::End);
#if defined(CPU_MCXN547VDF_cm33_core0) || defined(CPU_MCXN556SCDF_cm33_core0)
    constexpr static auto NumMutexes = common::to_underlying(MutexId::End);
#else
    // Set num mutexes to 0 for Core1 (no mutexes on Core1)
    constexpr static auto NumMutexes = 0;
#endif
    constexpr static auto                          NumC2CBuffers = EnableDualCore ? 2 : 0;
    static std::array<StreamBuffer, NumC2CBuffers> _c2c_buffers;
    /// storage for static c2c buffer
    static std::array<uint8_t, sys::ipc::calc_static_c2c_buf_size()> _static_c2c_buffer;

    /// Access singleton.
    static Supervisor& inst();

    /// Create all tasks before the scheduler is started.
    void startup(int argc, const char* argv[]);  // NOLINT(*-c-arrays)

    /// To be ran if the supervisor is stopped and we expect a clean exit.
    void shutdown();

    /// Registers a task with the supervisor.
    void register_task(Task& task);

    /// Access the current task
    const Task& current_task() const;

    /// get current task id
    TaskId current_task_id() const;

    const auto& task(TaskId id) const { return *_tasks[common::to_underlying(id)]; }

    auto& task(TaskId id) { return *_tasks[common::to_underlying(id)]; }

    auto& task_pub(TaskId id) { return _tasks_pub[common::to_underlying(id)]; }

    auto& timer(TimerId id) { return _timers[common::to_underlying(id)]; }

    /// Retrieve uninitialized event memory storage.
    Event* memory_for(EventId id);

    /// Retrieve uninitialized queue memory storage.
    Queue* memory_for(QueueId id);

    /// Retrieve uninitialized mutex memory storage.
    Mutex* memory_for(MutexId id);

    /// Retrieve uninitialized stream buffer memory storage.
    StreamBuffer*
    memory_for(StreamBuffer::IdType id, uint32_t base_addr, bool direct_memory_access);

    /// Retrieve uninitialized queue memory storage.
    Timer* memory_for(TimerId id);

    StreamBufferHandle_t* memory_for(StreamBuffer::IdType id);

    /// Will be called by std::terminate()
    [[noreturn]] void on_terminate();

    /// Will be called at application shutdown (_exit)
    [[noreturn]] void on_exit(int status);

    /// Get os ticks
    static uint32_t get_os_ticks();

    /// Check whether scheduler is running
    static bool is_scheduler_run();

    /// Check whether is in isr
    static bool is_in_isr();

private:
    std::array<Task*, NumTasks>      _tasks{};
    std::array<TaskPublic, NumTasks> _tasks_pub{};

    std::array<Event, NumEvents>                                 _events;
    std::array<Queue, NumQueues>                                 _queues;
    std::array<Mutex, NumMutexes>                                _mutexes;
    std::array<Timer, NumTimers>                                 _timers;
    std::array<StreamBufferHandle_t, sys::ipc::NumStreamBuffers> _stream_buffer_handles;
};

}  // namespace nv::ipc
