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
#include <chrono>
#include <cstdint>

#include "nv/ipc/object.h"
#include "sys/ipc/mutex.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

/**
 * An abstracted recursive mutex interface for resource protection.
 *
 * This mutex can be locked multiple times by the same task without deadlock,
 * which is useful for nested operations or function calls.
 *
 * The non os-specific implementation can be found in nv/ipc/mutex.cpp,
 * while the os-specific implementation can be found in sys/{os}/sys/ipc/mutex.{h,cpp}
 */
class Mutex
: public Object
, sys::ipc::Mutex
{
public:
    enum class Status
    {
        Ok,       ///< Success
        Timeout,  ///< A timeout occurred.
        Error,    ///< An error occurred.
    };

    using IdType = MutexId;
    using Usecs  = std::chrono::microseconds;

    /// Default timeout for mutex operations (1 second)
    static constexpr Usecs DefaultTimeout = Usecs(1'000'000);

    /// Allocate or access mutex by MutexId only by this method
    static Mutex& make(MutexId id);

    /// Return the id associated with this mutex.
    constexpr MutexId id() const { return _id; }

    /**
     * Lock the mutex with an optional timeout.
     *
     * @param[in]   timeout Wait timeout microseconds to acquire the mutex.
     * @return      A valid Mutex::Status.
     */
    [[nodiscard]] Status lock(Usecs timeout = DefaultTimeout);

    /**
     * Unlock the mutex.
     */
    void unlock();

    /// Null constructor for storage initialization only.
    Mutex() noexcept : sys::ipc::Mutex() {}

private:
    Mutex(MutexId id);
    const MutexId _id = MutexId::End;
};

}  // namespace nv::ipc
