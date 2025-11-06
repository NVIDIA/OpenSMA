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
#include "pdk/cmn/console_cpp/pdk-cmn-console-plat.h"

#include <cstdarg>
#include <cstdio>

namespace pdk::cmn::console {

namespace internal {
constexpr size_t BUFFER_SIZE = 256;
}

void plat_print(const char* fmt, va_list args)  // NOLINT(misc-include-cleaner)
{
    // NOLINTBEGIN
    char buffer[internal::BUFFER_SIZE];
    // coverity[cert_err33_c_violation] alllow not checking return value
    vsnprintf(buffer, internal::BUFFER_SIZE, fmt, args);
    buffer[internal::BUFFER_SIZE - 1] = '\0';
    // coverity[cert_err33_c_violation] alllow not checking return value
    fputs(buffer, stdout);
    // NOLINTEND
}

void plat_print(const char* fmt)
{
    // coverity[cert_err33_c_violation] alllow not checking return value
    fputs(fmt, stdout);  // NOLINT(cert-err33-c)
}
}  // namespace pdk::cmn::console
