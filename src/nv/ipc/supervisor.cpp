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
#include "nv/ipc/supervisor.h"
#include "nv/ipc/bm_core1_cfg_data.h"

#include "nv/common/preproc.h"
#include "nv/common/utils.h"
#include "nv/ipc/c2c_stream_buffer.h"
#include NV_IPC_CONFIG_H
#include "nv/ipc/task.h"
#include "nv/nv.h"
#include "nv/ipc/driver.h"

#ifdef NV_UNITTEST
#include "nv/ut/task.h"
#endif

#ifdef NV_COVERAGE
#include "nv/fw_parser/fw_parser_mcu.h"
#endif

using namespace nv;
using namespace nv::ipc;

namespace {
NV_SHARED_BSS Supervisor supervisor;  // NOLINT(*-non-const-global-variables)
}
#if defined(CPU_MCXN556SCDF_cm33_core0) && NCSI_ENABLE
// MCXN556 NCSI: Use NV_SHARED_BSS to place in MPU shared region (shared_bss section)
// This allows non-privileged IPC tasks to access C2C buffers.
// NCSI projects pass the actual buffer address (not __shared_memory_start__).
NV_SHARED_BSS std::array<uint8_t, sys::ipc::calc_static_c2c_buf_size()>
              Supervisor::_static_c2c_buffer;

NV_SHARED_BSS std::array<StreamBuffer, Supervisor::NumC2CBuffers> Supervisor::_c2c_buffers;

/// Filled by Core0 before starting Core1; passed as startup data so Core1 can read both
/// addresses
NV_SHARED_BSS nv::ipc::StartupInfo nv::ipc::g_core1_startup_data;

// Core1 bare-metal driver expects sizeof(StreamBuffer) = 60 bytes
// If this fails, update Core0StreamBufferSize in sys/mcxn556/sys/ipc_bm/driver.cpp
static_assert(sizeof(StreamBuffer) == 60,
              "StreamBuffer size mismatch - update Core1 ipc_bm driver");

// Verify C2CStreamBufferCtrl matches FreeRTOS StaticStreamBuffer_t layout.
// If FreeRTOS changes its internal StreamBuffer_t layout, these will fail at compile time.
static_assert(offsetof(C2CStreamBufferCtrl, tail)
                  == offsetof(StaticStreamBuffer_t, uxDummy1) + 0 * sizeof(size_t),
              "C2C tail offset must match StaticStreamBuffer_t.uxDummy1[0]");
static_assert(offsetof(C2CStreamBufferCtrl, head)
                  == offsetof(StaticStreamBuffer_t, uxDummy1) + 1 * sizeof(size_t),
              "C2C head offset must match StaticStreamBuffer_t.uxDummy1[1]");
static_assert(offsetof(C2CStreamBufferCtrl, length)
                  == offsetof(StaticStreamBuffer_t, uxDummy1) + 2 * sizeof(size_t),
              "C2C length offset must match StaticStreamBuffer_t.uxDummy1[2]");
static_assert(offsetof(C2CStreamBufferCtrl, buffer_ptr)
                  == offsetof(StaticStreamBuffer_t, pvDummy2) + 2 * sizeof(void*),
              "C2C buffer_ptr offset must match StaticStreamBuffer_t.pvDummy2[2]");
static_assert(offsetof(C2CStreamBufferCtrl, flags) == offsetof(StaticStreamBuffer_t, ucDummy3),
              "C2C flags offset must match StaticStreamBuffer_t.ucDummy3");

#elif defined(CPU_MCXN556SCDF_cm33_core0) || defined(CPU_MCXN547VDF_cm33_core0)
// Non-NCSI MCXN556 and MCXN547: Keep C2C buffers in .c2c_memory_data section
// (covered by __shared_memory_start__). Projects pass &__shared_memory_start__
// as the C2C base address, so the buffers must live in that section.
__attribute__((
    section(".c2c_memory_data"))) std::array<uint8_t, sys::ipc::calc_static_c2c_buf_size()>
                                  Supervisor::_static_c2c_buffer;

__attribute__((section(".c2c_memory_data"))) std::array<StreamBuffer, Supervisor::NumC2CBuffers>
                                             Supervisor::_c2c_buffers;

#else
std::array<uint8_t, sys::ipc::calc_static_c2c_buf_size()> Supervisor::_static_c2c_buffer;
std::array<StreamBuffer, Supervisor::NumC2CBuffers>       Supervisor::_c2c_buffers;

#endif

Supervisor& Supervisor::inst()
{
    // TODO: Should the supervisor be created on demand using a specific carveout?
    return supervisor;
}

void Supervisor::startup([[maybe_unused]] int         argc,
                         [[maybe_unused]] const char* argv[])  // NOLINT(*-c-arrays)
{
// x86 coverage build
#ifdef NV_UNITTEST
    // create the unittesting task if we are unittesting.
    this->argc = argc;
    this->argv = argv;
    ut::Task::make(argc, argv);
#else
// mcu coverage build
#ifdef NV_COVERAGE
    // if NV_COVERAGE is defined and not debug signed, halt the system
    // only core0 is allowed to read the key version since it is placed in secure memory zone
    if (nv::ipc::get_current_core() == nv::ipc::CoreId::Core0) {
        auto key_version = fw_parser::mcu::get_image_signing_key_version(
            fw_parser::mcu::ParsingFwType::ActiveSlot);
        if (!key_version || *key_version != 0) {
            inst().on_exit(-1);
        }
    }
#endif
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

// Only create mutex on Core0
#if defined(CPU_MCXN547VDF_cm33_core0) || defined(CPU_MCXN556SCDF_cm33_core0)
    for (int i = 0; i < static_cast<int>(ipc::MutexId::End); i++) {
        ipc::Mutex::make(static_cast<ipc::MutexId>(i));
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

Mutex* Supervisor::memory_for(MutexId id)
{
    auto idx = common::to_underlying(id);
    return &_mutexes.at(idx);
}

StreamBuffer*
Supervisor::memory_for(StreamBuffer::IdType id, uint32_t base_addr, bool direct_memory_access)
{
    auto idx = static_cast<uint32_t>(id);
    // Only dual core support direct memory access
    // If direct memory access is requested, return the stream buffer address
    if constexpr (EnableDualCore) {
        if (direct_memory_access) {
            nv::assert(idx <= UINT32_MAX / sizeof(StreamBuffer));
            nv::assert(base_addr <= UINT32_MAX - idx * sizeof(StreamBuffer));

            auto stream_buffer_address = base_addr + idx * sizeof(StreamBuffer);
            // NOLINTNEXTLINE(*-reinterpret-cast)
            return reinterpret_cast<StreamBuffer*>(stream_buffer_address);
        }
        else {
            // coverity[cert_int31_c_violation] dont care
            if (idx < Supervisor::NumC2CBuffers) {
                return &_c2c_buffers.at(idx);
            }
            // Future use: Could add other stream buffer not use for c2c, but not used now
            else {
                return nullptr;
            }
        }
    }
    // Future use: Could add other stream buffer not use for c2c, but not used now
    return nullptr;
}

StreamBufferHandle_t* Supervisor::memory_for(StreamBuffer::IdType id)
{
    auto idx = static_cast<uint32_t>(id);
    // coverity[unsigned_compare] - id should be less than NumStreamBuffers
    if (static_cast<uint32_t>(id) < sys::ipc::NumStreamBuffers) {
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