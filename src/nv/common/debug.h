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
#include <source_location>
#include <type_traits>
#include <utility>

#include "nv/common/console.h"
#include "nv/common/debuglevel.h"
#include "nv/common/system.h"

#ifdef assert
#undef assert
#endif

/**
 * When DbgLevel == DebugLevel::None all of these will be removed from the binary
 * including any strings literals as parameters.  Any non constexpr functions that
 * are called as parameters should be enclosed inside a NV_LAZY(...) for late evaluation,
 * also causing the compiler to cull it.
 */
namespace nv::common {

namespace details {

// some helper code for lazy evaluation of debug message parameters.
template<typename T>
concept is_array = (std::is_array_v<T>);
constexpr auto eval(is_array auto& c)
{
    return static_cast<decltype(&c[0])>(c);
}
constexpr auto eval(auto&& c) -> auto&&
{
    return c;
}
constexpr auto eval(std::invocable auto&& c)
{
    return c();
}

}  // namespace details

// NOLINTBEGIN(*vararg*)
// GBS:BEGIN NO COVERAGE FIXME!!
// This cannot be tested as it aborts

/**
 * Non returning fatal error, with printf style error messaging to console.
 *
 * Requires that DbgLevel is set to DebugLevel::Fatal or higher.
 *
 * @param[in]  exit_value The exit value of the program if running on an emulator or posix host.
 * @param[in]  fmt        Printf style format string.
 * @param[in]  args       Printf style varadic arguments.
 */
template<typename... Args>
[[noreturn]] void fatal(ExitValue exit_value, const char* fmt, Args&&... args)
{
    if constexpr (DbgLevel >= DebugLevel::Fatal) {
        Console::print(
            DebugLevel::Fatal, fmt, details::eval(std::forward<decltype(args)>(args))...);
    }
    System::inst().abort(exit_value);
}

/**
 * Non returning fatal error, with printf style error messaging to console.
 *
 * Requires that DbgLevel is set to DebugLevel::Fatal or higher.  The program will exit with
 * an exit value of ExitValue::Error.
 *
 * @param[in]  fmt        Printf style format string.
 * @param[in]  args       Printf style varadic arguments.
 */
template<typename... Args>
[[noreturn]] void fatal(const char* fmt, Args&&... args)
{
    if constexpr (DbgLevel >= DebugLevel::Fatal) {
        Console::print(
            DebugLevel::Fatal, fmt, details::eval(std::forward<decltype(args)>(args))...);
    }
    System::inst().abort(ExitValue::Error);
}
// GBSEND: NO COVERAGE FIXME!!

/**
 * Printf style error message.
 *
 * @param[in]  fmt        Printf style format string.
 * @param[in]  args       Printf style varadic arguments.
 */
template<typename... Args>
struct error
{
    explicit error(const char* fmt,
                   Args&&... args,
                   const std::source_location& loc = std::source_location::current())
    {
        if constexpr (DbgLevel >= DebugLevel::Error) {
            Console::print(DebugLevel::Error,
                           "%s:%d @ %s()\n",
                           loc.file_name(),
                           loc.line(),
                           loc.function_name());
            Console::print(
                DebugLevel::Error, fmt, details::eval(std::forward<decltype(args)>(args))...);
        }
    }

    explicit error(int         enum_err,
                   const char* fmt,
                   Args&&... args,
                   const std::source_location& loc = std::source_location::current())
    {
        if constexpr (DbgLevel >= DebugLevel::Error) {
            Console::print(DebugLevel::Error,
                           "Status:%d @ %s:%d @ %s()\n",
                           enum_err,
                           loc.file_name(),
                           loc.line(),
                           loc.function_name());
            Console::print(
                DebugLevel::Error, fmt, details::eval(std::forward<decltype(args)>(args))...);
        }
    }
};
template<typename... Args>
error(const char*&&, Args&&...) -> error<Args...>;

/**
 * Printf style warning message.
 *
 * @param[in]  fmt        Printf style format string.
 * @param[in]  args       Printf style varadic arguments.
 */
inline const auto warn = [](const char* fmt, auto&&... args) {
    if constexpr (DbgLevel >= DebugLevel::Warn) {
        Console::print(
            DebugLevel::Warn, fmt, details::eval(std::forward<decltype(args)>(args))...);
    }
};

/**
 * Printf style debug message.
 *
 * @param[in]  fmt        Printf style format string.
 * @param[in]  args       Printf style varadic arguments.
 */
inline const auto debug = [](const char* fmt, auto&&... args) {
    if constexpr (DbgLevel >= DebugLevel::Debug) {
        Console::print(
            DebugLevel::Debug, fmt, details::eval(std::forward<decltype(args)>(args))...);
    }
};

/**
 * Printf style info message.
 *
 * @param[in]  fmt        Printf style format string.
 * @param[in]  args       Printf style varadic arguments.
 */
inline const auto info = [](const char* fmt, auto&&... args) {
    if constexpr (DbgLevel >= DebugLevel::Info) {
        Console::print(DebugLevel::Info,
                       fmt,
                       // coverity[cert_str34_c_violation] - intentional variadic template usage
                       details::eval(std::forward<decltype(args)>(args))...);
    }
};

#if defined(CPU_MCXN547VDF)
/**
 * Printf style debug message.
 *
 * @param[in]  fmt        Printf style format string.
 * @param[in]  args       Printf style varadic arguments.
 */
inline const auto debug_core_wrapper = [](const char* fmt, auto&&... args) {
    if constexpr (DbgLevel >= DebugLevel::Debug) {
        Console::print_core(
            DebugLevel::Debug, fmt, details::eval(std::forward<decltype(args)>(args))...);
    }
};
#endif

/**
 * Assertion and printf style message.
 *
 * @param[in]  cond       will assert if false
 * @param[in]  fmt        Printf style format string.
 * @param[in]  args       Printf style varadic arguments.
 * @param[in]  loc        auto source_location, do not change this
 */
void assert(bool        cond,
            const char* fmt,
            auto&&... args,
            const std::source_location& loc = std::source_location::current())
{
    if constexpr (DbgLevel >= DebugLevel::Error) {
        if (!cond) {
            Console::print(DebugLevel::Error,
                           "assert @ %s:%d @ %s()\n",
                           loc.file_name(),
                           loc.line(),
                           loc.function_name());
            Console::print(
                DebugLevel::Error, fmt, details::eval(std::forward<decltype(args)>(args))...);
            System::inst().abort(ExitValue::Error);
        }
    }
}
/**
 * Assertion.
 *
 * @param[in]  cond       will assert if false
 * @param[in]  loc        auto source_location, do not change this
 */
inline void assert(bool cond, const std::source_location& loc = std::source_location::current())
{
    if constexpr (DbgLevel >= DebugLevel::Error) {
        if (!cond) {
            Console::print(DebugLevel::Error,
                           "assert @ %s:%d @ %s()\n",
                           loc.file_name(),
                           loc.line(),
                           loc.function_name());
            System::inst().abort(ExitValue::Error);
        }
    }
}

/**
 * Assertion even if DbgLevel is set to None.
 *
 * @param[in]  cond       will assert if false
 * @param[in]  loc        auto source_location, do not change this
 */
inline void always_assert(bool                        cond,
                          const std::source_location& loc = std::source_location::current())
{
    if (!cond) {
        Console::print(DebugLevel::Error,
                       "assert @ %s:%d @ %s()\n",
                       loc.file_name(),
                       loc.line(),
                       loc.function_name());
        System::inst().abort(ExitValue::Error);
    }
}

#define NV_ALWAYS_ASSERT(cond, ...)                                                            \
    if (!(cond)) {                                                                             \
        nv::common::fatal(                                                                     \
            "%s:%d - %s - %s\n", __FILE__, __LINE__, #cond __VA_OPT__(, ) __VA_ARGS__);        \
    }

#ifdef NV_DEBUG
#ifdef NV_UNITTEST
#define NV_ASSERT(cond, ...)                                                                   \
    if (DbgLevel != DebugLevel::None && !(cond)) {                                             \
        nv::common::error(                                                                     \
            "%s:%d - %s - %s\n", __FILE__, __LINE__, #cond __VA_OPT__(, ) __VA_ARGS__);        \
    }
#else
#define NV_ASSERT(cond, ...)                                                                   \
    if (DbgLevel != DebugLevel::None && !(cond)) {                                             \
        nv::common::fatal(                                                                     \
            "%s:%d - %s - %s\n", __FILE__, __LINE__, #cond __VA_OPT__(, ) __VA_ARGS__);        \
    }
#endif  // NV_UNITTEST
#define NV_COMMON_LAZY(x)                                                                      \
    [&] {                                                                                      \
        return x;                                                                              \
    }

#else
#define NV_ASSERT(cond, ...)                                                                   \
    {}
#define NV_COMMON_LAZY(...) ((void)0)
// NOLINTEND(*vararg*)
#endif

#ifdef __clang__
#pragma clang final(NV_ASSERT)
#pragma clang final(NV_ALWAYS_ASSERT)
#endif
}  // namespace nv::common
