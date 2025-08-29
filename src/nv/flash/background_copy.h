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
#include "nv/flash/common.h"
#include "nv/flash/driver.h"
namespace nv::flash {

class BackgroundCopy
{
public:
    enum class State : uint32_t
    {
        Begin = 0,
        Idle  = Begin,
        Done,
        InProgress,
        Failed,
        End,
    };

    BackgroundCopy()
    {
        current_offset = 0x0;
        _state         = State::Idle;
    }

    Status start();
    void   stop();
    void   next(Driver& driver);
    Status status_progress(Response& response);
    Status state_to_status();
    State  state();

private:
    // TBD: Size to copy, need to check image length
    static constexpr uint32_t CopySize  = sys::flash::config::FirmwareMaxSize;
    static constexpr uint32_t RemapSize = sys::flash::config::RemapSize;
    State                     _state;
    Address                   current_offset;
    Buffer                    read_buffer{};
    static constexpr uint32_t PercentMultiplier = 100;
};

}  // namespace nv::flash