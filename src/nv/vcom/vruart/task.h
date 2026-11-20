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

#include <cstddef>
#include <cstdint>
#include <utility>

#include "nv/ipc/task.h"
#include "nv/ipc/event.h"

#include "nv/vcom/vruart/bridge.h"

namespace nv::vruart {

// Single task for UART-USB bridge
// Handles both directions:
//   USB RX → UART TX (synchronous, UART is bottleneck)
//   UART RX → USB TX (via queue, USB may be busy)
class Task : public ipc::Task
{
public:
    explicit Task(uint8_t index) noexcept;

    [[noreturn]] void main();

    static void make();
    static void entrypoint(void* params);

private:
    template<size_t Index>
    static void make_instance();

    template<size_t... Indices>
    static void make_instances(std::index_sequence<Indices...>);

    uint8_t index;
    Bridge& bridge;
};

}  // namespace nv::vruart
