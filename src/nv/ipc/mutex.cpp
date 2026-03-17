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
#include "nv/ipc/mutex.h"

#include "nv/ipc/supervisor.h"
#include "nv/nv.h"

using namespace nv::ipc;

Mutex& Mutex::make(MutexId id)
{
    // pre-conditions
    nv::always_assert(common::is_in_range(id));

    auto obj_mem = Supervisor::inst().memory_for(id);
    if (obj_mem->obj_is_allocated) {
        return *obj_mem;
    }

    // coverity[cert_mem54_cpp_violation] - obj_mem points to sufficient pre-allocated storage
    auto obj              = new (obj_mem) Mutex(id);  // NOLINT (*-owning-memory)
    obj->obj_is_allocated = true;
    return *obj;
}
