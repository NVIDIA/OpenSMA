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
#include <chrono>

#include "nv/ipc/object.h"
#include "sys/ipc/timer.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

/**
 * An abstracted task timer interface.
 */
class Timer
: public Object
, sys::ipc::Timer
{
public:
    enum class Status
    {
        Ok,                       ///< Success
        InsufficientPermissions,  ///< Calling task had no permission for queue operation.
        InvalidParam,             ///< One of the calling parameters was deemed invalid.
        Unknown,                  ///< An unknown error occurred.
        InvalidOperation,         ///< The operation is not allowed on the current core.
    };

    using Callback = void (*)(Timer& id);

    TimerId id() const;

    Status start();

    Status stop();

    Status reset();

    bool enabled() const;

    std::chrono::microseconds period() const;

    Status period(std::chrono::microseconds us);

    Timer() noexcept : sys::ipc::Timer() {}

    static Timer& make(TimerId                   id,
                       std::chrono::microseconds period,
                       Callback                  cb,
                       bool                      reload = true) noexcept;

    Callback on_timeout = nullptr;

private:
    Timer(TimerId id, std::chrono::microseconds period, Callback cb, bool reload) noexcept;

    TimerId _id = TimerId::End;
};

}  // namespace nv::ipc
