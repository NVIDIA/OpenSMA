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

#include <array>
#include <cstdint>

namespace pdk::cmn::logger {
constexpr auto EventDataSize = 8;
using EventData              = std::array<uint8_t, EventDataSize>;

enum class Status : uint32_t
{
    Ok,
    Error,
    Busy,
    Timeout,
    InvalidParam,
};

enum class Level : uint8_t
{
    Unknown,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};
using EventId = uint16_t;

enum class OutputDirection : uint8_t
{
    None    = 0x0,
    Console = 0x1,
    Flash   = 0x2,
    Both    = 0x3,
};

struct EventStructItem
{
    EventId unique_id;
    Level   default_level;
};
}  // namespace pdk::cmn::logger