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

#include <array>
#include <cstddef>
#include <cstdint>
#include <source_location>

#include "nv/ipc/wire_format.h"
#include "sys/ipc_bm/driver.h"

namespace nv::core1_logger_detail {

constexpr std::size_t EventDataSize = 8;
using EventData                     = std::array<uint8_t, EventDataSize>;

enum class Level : uint8_t
{
    Unknown,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

struct Event
{
    uint16_t unique_id;
    Level    default_level;
};

constexpr uint64_t FnvOffset = 14695981039346656037ull;
constexpr uint64_t FnvPrime  = 1099511628211ull;

constexpr const char* extract_simple_function_name(const char* full_func_name)
{
    if (!full_func_name) {
        return full_func_name;
    }

    const char* paren_pos = nullptr;
    for (auto p = full_func_name; *p != '\0'; ++p) {
        if (*p == '(') {
            paren_pos = p;
            break;
        }
    }
    if (!paren_pos) {
        return full_func_name;
    }

    const char* last_scope = nullptr;
    for (auto p = full_func_name; p < paren_pos; ++p) {
        if (*p == ':' && *(p + 1) == ':' && (p + 2) < paren_pos) {
            last_scope = p + 2;
        }
    }
    return last_scope ? last_scope : full_func_name;
}

constexpr uint16_t footprint(const std::source_location& location)
{
    constexpr uint8_t  HashSeparator = 0xFFu;
    constexpr uint16_t HashMask      = 0x0FFFu;

    uint64_t hash = FnvOffset;
    for (auto p = location.file_name(); *p != 0; ++p) {
        hash ^= static_cast<unsigned char>(*p == '\\' ? '/' : *p);
        hash *= FnvPrime;
    }
    hash ^= HashSeparator;
    hash *= FnvPrime;

    const auto function = extract_simple_function_name(location.function_name());
    for (auto p = function; *p != 0 && *p != '(' && *p != ' '; ++p) {
        hash ^= static_cast<unsigned char>(*p);
        hash *= FnvPrime;
    }
    return static_cast<uint16_t>(hash & HashMask);
}

template<typename QueueId = nv::ipc_bm::QueueId>
inline void log(Event                       event,
                Level                       fallback_level,
                const EventData&            data,
                const std::source_location& location)
{
    // Template + if constexpr so projects without QueueId::Core1Log still compile.
    if constexpr (requires { QueueId::Core1Log; }) {
        if (!nv::ipc_bm::Driver::isInitialized()) {
            return;
        }

        const Level             level = event.default_level == Level::Unknown ? fallback_level
                                                                              : event.default_level;
        nv::ipc_bm::CoreLogWire item{.event     = event.unique_id,
                                     .level     = static_cast<uint8_t>(level),
                                     .reserved0 = 0,
                                     .data      = {},
                                     .footprint = footprint(location),
                                     .reserved1 = {}};
        for (std::size_t index = 0; index < data.size(); ++index) {
            item.data[index] = data[index];
        }

        (void)nv::ipc_bm::Driver::sendToQueue(
            QueueId::Core1Log, reinterpret_cast<const uint8_t*>(&item), sizeof(item));
    }
}

}  // namespace nv::core1_logger_detail

namespace nv::logger {

using CoreEventData = core1_logger_detail::EventData;

struct CoreEvent
{
    static constexpr core1_logger_detail::Event IpcTaskInitSuccess = {
        0x0201, core1_logger_detail::Level::Info};

    // Dedicated IDs so the host parser does not decode Core1 LPSPI payloads as
    // Core0 SpiBind {taskID, flexcomm} or SPB mailbox events.
    static constexpr core1_logger_detail::Event SpiBmError = {
        0x0b08, core1_logger_detail::Level::Error};
    static constexpr core1_logger_detail::Event SpiBmTimeout = {
        0x0b09, core1_logger_detail::Level::Error};
    static constexpr core1_logger_detail::Event SpiBindBm  = {0x0b0a,
                                                              core1_logger_detail::Level::Info};
    static constexpr core1_logger_detail::Event SpiBmTrace = {
        0x0b0b, core1_logger_detail::Level::Error};

    CoreEvent()                            = delete;
    CoreEvent(const CoreEvent&)            = delete;
    CoreEvent& operator=(const CoreEvent&) = delete;
};

inline void info(core1_logger_detail::Event  event,
                 const CoreEventData&        data     = {},
                 const std::source_location& location = std::source_location::current())
{
    core1_logger_detail::log(event, core1_logger_detail::Level::Info, data, location);
}

inline void error(core1_logger_detail::Event  event,
                  const CoreEventData&        data     = {},
                  const std::source_location& location = std::source_location::current())
{
    core1_logger_detail::log(event, core1_logger_detail::Level::Error, data, location);
}

}  // namespace nv::logger
