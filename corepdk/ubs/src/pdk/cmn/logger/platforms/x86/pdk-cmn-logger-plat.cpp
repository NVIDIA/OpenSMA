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
#include "pdk-cmn-logger-plat.h"

#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdio>

#include "pdk-cmn-console-plat.h"

namespace pdk::cmn::logger {

namespace internal {

const char* level_to_string(Level level)
{
    switch (level) {
        case Level::Debug   : return "DEBUG";
        case Level::Info    : return "INFO";
        case Level::Warning : return "WARNING";
        case Level::Error   : return "ERROR";
        case Level::Critical: return "CRITICAL";
        default             : return "UNKNOWN";
    }
}

Status write_to_console(EventId event, Level level, const EventData& data)
{
    std::array<char, 128> buffer{};  // Initialize all elements to zero

    // NOLINTBEGIN
    // coverity[cert_err33_c_violation] alllow not checking return value
    snprintf(buffer.data(),
             buffer.size(),
             "[Logger] Event 0x%04x | Level: %s | Data:",
             static_cast<unsigned int>(event),
             level_to_string(level));
    // NOLINTEND
    pdk::cmn::console::plat_print(static_cast<const char*>(buffer.data()));

    for (const auto& byte : data) {
        // NOLINTBEGIN
        // coverity[cert_err33_c_violation] alllow not checking return value
        snprintf(buffer.data(), buffer.size(), " %02x", static_cast<unsigned char>(byte));
        // NOLINTEND
        pdk::cmn::console::plat_print(static_cast<const char*>(buffer.data()));
    }
    pdk::cmn::console::plat_print("\n");
    return Status::Ok;
}

// TODO: Don't have a good way for writing log to flash on both x86 with freertos and without
// freertos
Status write_to_flash(EventId /*unused*/, Level /*unused*/, const EventData& /*unused*/)
{
    return Status::Ok;
}

}  // namespace internal

Status plat_log_add(EventId event, Level level, EventData data, OutputDirection dir)
{
    Status status = Status::Ok;

    if (dir == OutputDirection::None) {
        return Status::InvalidParam;
    }

    if (static_cast<uint8_t>(dir) & static_cast<uint8_t>(OutputDirection::Console)) {
        internal::write_to_console(event, level, data);
    }

    if (static_cast<uint8_t>(dir) & static_cast<uint8_t>(OutputDirection::Flash)) {
        status = internal::write_to_flash(event, level, data);
    }

    return status;
}

}  // namespace pdk::cmn::logger
