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

namespace nv::ipc {

enum class CoreId : uint8_t
{
    Begin,
    Core0 = Begin,  // Core0 owns the instance
    Core1,          // Core1 owns the instance
    Both,           // Both core has its own instance, TaskId should not use this
    Abstract,       // Only used for TaskId, meaning the task doesn't really exist.
    Invalid,
    End = Invalid
};

constexpr nv::ipc::CoreId get_current_core()
{
    return nv::ipc::CoreId::Core0;
}

constexpr nv::ipc::CoreId get_peer_core()
{
    return nv::ipc::CoreId::Invalid;
}

}  // namespace nv::ipc