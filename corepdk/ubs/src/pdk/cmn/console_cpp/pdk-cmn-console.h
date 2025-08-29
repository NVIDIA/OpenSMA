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
#include <cstdarg>

#include "pdk-cmn-console-plat.h"

namespace pdk::cmn::console {

namespace internal {

constexpr const char* RED         = "\x1b[0;31m";
constexpr const char* ORANGE      = "\x1b[0;33m";
constexpr const char* BOLD_RED    = "\x1b[1;31m";
constexpr const char* BOLD_ORANGE = "\x1b[1;33m";
constexpr const char* NORMAL      = "\x1b[0m";
constexpr const char* DIM         = "\x1b[2m";
}  // namespace internal

// coverity[cert_dcl50_cpp_violation] Allow Defining a C-style variadic function
inline void fatal(const char* msg, ...)
{
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::BOLD_RED);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("[FATAL] ");

    va_list args;
    va_start(args, msg);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(msg, args);
    va_end(args);

    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::NORMAL);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("\n");
}
// coverity[cert_dcl50_cpp_violation] Allow Defining a C-style variadic function
inline void error(const char* msg, ...)
{
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::RED);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("[ERROR] ");

    va_list args;
    va_start(args, msg);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(msg, args);
    va_end(args);

    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::NORMAL);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("\n");
}
// coverity[cert_dcl50_cpp_violation] Allow Defining a C-style variadic function
inline void warning(const char* msg, ...)
{
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::BOLD_ORANGE);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("[WARNING] ");

    va_list args;
    va_start(args, msg);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(msg, args);
    va_end(args);

    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::NORMAL);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("\n");
}
// coverity[cert_dcl50_cpp_violation] Allow Defining a C-style variadic function
inline void debug(const char* msg, ...)
{
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::ORANGE);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("[DEBUG] ");

    va_list args;
    va_start(args, msg);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(msg, args);
    va_end(args);

    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::NORMAL);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("\n");
}
// coverity[cert_dcl50_cpp_violation] Allow Defining a C-style variadic function
inline void info(const char* msg, ...)
{
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::DIM);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("[INFO] ");

    va_list args;
    va_start(args, msg);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(msg, args);
    va_end(args);

    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print(internal::NORMAL);
    // coverity[cert_exp47_c_violation] don't check va_list usage in plat_print
    plat_print("\n");
}
}  // namespace pdk::cmn::console
