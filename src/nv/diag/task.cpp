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
#include "nv/diag/task.h"

#include <chrono>
#include <cstring>

#include "nv/common/debug.h"
#include "nv/common/preproc.h"
#include "nv/diag/common.h"
#include "nv/nv.h"

using namespace std::chrono_literals;

// Linker-defined symbols for m_diag RAM region
extern "C" {
extern uint8_t __diag_ram_start;
extern uint8_t __diag_ram_length;  // absolute symbol: &__diag_ram_length == LENGTH(m_diag)
}

namespace nv::diag {

uint32_t Task::_test_offset = 0;
uint32_t Task::_test_size   = 0;
bool     Task::_test_loaded = false;

inline size_t get_diag_ram_size()
{
    // coverity[cert_int31_c_violation] safe to cast
    return static_cast<size_t>(reinterpret_cast<uintptr_t>(&__diag_ram_length));
}

Task::Task() noexcept
: ipc::Task(nv::ipc::TaskId::Diag, "DIAG")
, _event(nv::ipc::Event::make(nv::ipc::EventId::DiagEvent))
{}

void Task::make()
{
    constexpr auto StackSize = std::max(2080, int(configMINIMAL_STACK_SIZE));

    NV_TASK_DATA static Task                       task;
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));

    task.setup(stack.span(), Priv, Priority::Diag, Task::entrypoint);

    size_t ram_size = get_diag_ram_size();
    memset(&__diag_ram_start, 0x00, ram_size);

    nv::info("Diag: Test RAM initialized at 0x%x (%d bytes)\n",
             reinterpret_cast<uint32_t>(&__diag_ram_start),
             ram_size);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

void Task::start()
{
    while (true) {
        auto event = _event.wait(EventBits::LoadTestCode, true, false, 1s);

        if (event.value() & EventBits::LoadTestCode) {
            nv::info("Diag: LoadTestCode event received\n");
        }
    }
}

bool Task::load_test_code(std::span<const uint8_t> code)
{
    uint32_t code_size = code.size();
    size_t   ram_size  = get_diag_ram_size();

    if (code_size > ram_size) {
        nv::info("Diag: Code too large (%d bytes, max %d)\n", code_size, ram_size);
        return false;
    }

    nv::info("Diag: Loading test handler (%d bytes) to RAM at 0x%x\n",
             code_size,
             reinterpret_cast<uint32_t>(&__diag_ram_start));

    std::memcpy(&__diag_ram_start, code.data(), code_size);

    _test_offset = 0;
    _test_size   = code_size;
    _test_loaded = true;

    nv::info("Diag: Test handler loaded successfully\n");
    return true;
}

bool Task::clear_test_code()
{
    if (!_test_loaded) {
        nv::info("Diag: No test code loaded\n");
        return false;
    }

    nv::info("Diag: Clearing test handler from RAM\n");

    std::memset(&__diag_ram_start, 0x00, _test_size);

    _test_offset = 0;
    _test_size   = 0;
    _test_loaded = false;

    nv::info("Diag: Test handler cleared\n");
    return true;
}

uint32_t Task::get_test_address()
{
    if (!_test_loaded) return 0;

    return reinterpret_cast<uint32_t>(&__diag_ram_start) + _test_offset;
}

uint32_t Task::get_test_size()
{
    return _test_size;
}

}  // namespace nv::diag
