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

#include <cstdint>
#include <span>

#include "nv/ipc/event.h"
#include "nv/ipc/task.h"

namespace nv::diag {

class Task : public ipc::Task
{
public:
    Task() noexcept;

    static void make();
    static void entrypoint(void* params);

    void start();

    static bool load_test_code(std::span<const uint8_t> code);
    static bool clear_test_code();

    // Get test info
    static uint32_t get_test_address();
    static uint32_t get_test_size();

private:
    nv::ipc::Event& _event;

    static uint32_t _test_offset;
    static uint32_t _test_size;
    static bool     _test_loaded;
};

}  // namespace nv::diag