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

namespace nv {

/// Global debug levels in increasing verbosity.
enum class DebugLevel
{
    None,   ///< No debug output, all debug strings removed from binary.
    Fatal,  ///< Fatal only messages.
    Error,  ///< Error and Fatal messages only.
    Warn,   ///< Warning, Error, and Fatal messages.
    Debug,  ///< Verbose debug messaging.
    Info,   ///< Maximum level of debug messaging that will have a heavy impact on performance.
};

// Allow projects to provide a lightweight per-build debug-console config header
// (e.g. defining NV_UART_INSTANCE) without including the full project config.
#if defined(NV_DEBUGCONSOLE_CONFIG_H)
#include NV_DEBUGCONSOLE_CONFIG_H
#endif

#if defined(NV_UART_INSTANCE) && (NV_UART_INSTANCE == 0xfe)
[[maybe_unused]] constexpr inline auto DbgLevel = DebugLevel::None;
#elif defined(NV_DEBUG) && NV_DEBUG > 0
[[maybe_unused]] constexpr inline auto DbgLevel = DebugLevel::Info;
#else
[[maybe_unused]] constexpr inline auto DbgLevel = DebugLevel::None;
#endif

}  // namespace nv
