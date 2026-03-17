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
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "corepdk/ubs/src/pdk/cmn/log/plat.h"
#include "nv/logger/log.h"
#include "nv/logger/uart_string.h"

namespace pdk::cmn::log::plat {

namespace {
constexpr bool kPersistentUsesMagicNumber = std::is_same_v<internal::SourceLocationPersistent,
                                                           internal::MagicNumberType>;

static_assert(kPersistentUsesMagicNumber,
              "MCU platform expects persistent source locations as MagicNumberType");

// Map pdk::cmn::log::DebugLevel to nv::logger::Level
nv::logger::Level to_nv_level(DebugLevel level)
{
    switch (level) {
        case DebugLevel::None : return nv::logger::Level::Unknown;
        case DebugLevel::Fatal: return nv::logger::Level::Critical;
        case DebugLevel::Error: return nv::logger::Level::Error;
        case DebugLevel::Warn : return nv::logger::Level::Warning;
        case DebugLevel::Debug: return nv::logger::Level::Debug;
        case DebugLevel::Info : return nv::logger::Level::Info;
        default               : return nv::logger::Level::Unknown;
    }
}

// Map pdk::cmn::log::Status from nv::logger::Status
Status from_nv_status(nv::logger::Status s)
{
    switch (s) {
        case nv::logger::Status::Ok          : return Status::Ok;
        case nv::logger::Status::Timeout     : return Status::Timeout;
        case nv::logger::Status::Error       :
        case nv::logger::Status::InvalidParam: return Status::Fatal;
        default                              : return Status::Unknown;
    }
}
}  // namespace

// Printf-style console logging
// NOLINTBEGIN
Status log(DebugLevel                             level,
           Usecs                                  timeout,
           const internal::SourceLocationConsole& sloc,
           const char*                            fmt,
           ...)
{
    if (strchr(fmt, '%') == nullptr) {
        va_list dummy;
        std::memset(&dummy, 0, sizeof(dummy));
        nv::logger::format_to_logger(fmt, dummy);
    }
    else {
        va_list va;
        va_start(va, fmt);
        nv::logger::format_to_logger(fmt, va);
        va_end(va);
    }
    return Status::Ok;
}
// NOLINTEND

// Printf-style persistent logging
// NOLINTBEGIN
Status log_p(DebugLevel                                level,
             Usecs                                     timeout,
             const internal::SourceLocationPersistent& sloc,
             const char*                               fmt,
             ...)
{
    // No printf style log for persistent logging in mcu
    return Status::Ok;
}
// NOLINTEND

// Event console logging
Status log(DebugLevel                             level,
           Usecs                                  timeout,
           const internal::SourceLocationConsole& sloc,
           const Event&                           event)
{
    const auto data_lo = static_cast<uint32_t>(event.data & 0xFFFFFFFFull);
    const auto data_hi = static_cast<uint32_t>(event.data >> 32);
    auto       data    = nv::logger::data_from_two_u32(data_lo, data_hi);

    const auto nv_status = nv::logger::Logger::add(event.id,
                                                   to_nv_level(level),
                                                   data,
                                                   nv::logger::OutputDirection::Console,
                                                   false,
                                                   nv::ipc::Queue::Usecs(timeout),
                                                   sloc.id);

    return from_nv_status(nv_status);
}

// Event persistent logging
Status log_p(DebugLevel                                level,
             Usecs                                     timeout,
             const internal::SourceLocationPersistent& sloc,
             const Event&                              event)
{
    const auto data_lo = static_cast<uint32_t>(event.data & 0xFFFFFFFFull);
    const auto data_hi = static_cast<uint32_t>(event.data >> 32);
    auto       data    = nv::logger::data_from_two_u32(data_lo, data_hi);

    const auto nv_status = nv::logger::Logger::add(event.id,
                                                   to_nv_level(level),
                                                   data,
                                                   nv::logger::OutputDirection::Flash,
                                                   false,
                                                   nv::ipc::Queue::Usecs(timeout),
                                                   sloc.id);

    return from_nv_status(nv_status);
}

void putc(int ch)
{
    std::array<char, 2> buf{
        {static_cast<char>(ch), '\0'}
    };
    va_list dummy;
    std::memset(&dummy, 0, sizeof(dummy));
    nv::logger::format_to_logger(buf.data(), dummy);
}

void puts(const char* str)
{
    if (str) {
        va_list dummy;
        std::memset(&dummy, 0, sizeof(dummy));
        nv::logger::format_to_logger(str, dummy);
    }
}

void flush()
{
    // UART output is typically unbuffered on MCU; no-op
}

Status clear()
{
    // Console clear not supported on MCU
    return Status::Ok;
}

Status clear_p()
{
    const auto nv_status = nv::logger::Logger::clean_requset();
    return from_nv_status(nv_status);
}

}  // namespace pdk::cmn::log::plat

// C-Wrapper for Ada
extern "C" void cmn_log_plat_putc(int ch)
{
    pdk::cmn::log::plat::putc(ch);
}

extern "C" void cmn_log_plat_puts(const char* str)
{
    pdk::cmn::log::plat::puts(str);
}
