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
#include <cstring>
#include "corepdk/ubs/src/pdk/cmn/console_cpp/pdk-cmn-console-plat.h"
#include "nv/logger/uart_string.h"

namespace pdk::cmn::console {
void plat_print(const char* fmt)
{
    if (strchr(fmt, '%') == nullptr) {
        // workaround for no arguments
        va_list dummy;
        std::memset(&dummy, 0, sizeof(dummy));
        nv::logger::format_to_logger(fmt, dummy);
    }
}

void plat_print(const char* fmt, va_list args)
{
    nv::logger::format_to_logger(fmt, args);
}
}  // namespace pdk::cmn::console

// C-Wrapper for Ada
extern "C" void cmn_console_plat_print(const char* msg)
{
    pdk::cmn::console::plat_print(msg);
}