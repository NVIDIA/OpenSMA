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

#include "FreeRTOSConfig.h"
#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/nv.h"

using namespace nv::ipc;

Mutex::Mutex(const nv::ipc::MutexId id) : sys::ipc::Mutex(), _id(id)
{
    nv::assert(common::is_in_range(id));

    // Create the FreeRTOS recursive mutex using static allocation
    auto& super = Supervisor::inst();
    _mutex      = xSemaphoreCreateRecursiveMutexStatic(
        &super.static_mutexes.at(common::to_underlying(id)));
    nv::assert(_mutex != nullptr);
}

Mutex::Status Mutex::lock(Usecs timeout)
{
    constexpr auto OneSecInUsecs = 1'000'000;
    // coverity[cert_int32_c_violation] - timeout is cast to unsigned long
    const auto Ticks = configTICK_RATE_HZ * timeout.count() / OneSecInUsecs;

    auto res = xSemaphoreTakeRecursive(_mutex, Ticks);
    return (res == pdTRUE) ? Status::Ok : Status::Timeout;
}

void Mutex::unlock()
{
    xSemaphoreGiveRecursive(_mutex);
}
