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

namespace nv::ut::details {

/// Configuration
struct Config
{
    bool verbose        = false;                   ///< extra debug output
    bool quiet          = false;                   ///< do not print any unittest progress info.
    bool keep_going     = NV_UNITTEST_KEEP_GOING;  ///< do not stop on fail
    int  width          = 120;                     ///< Printer default output width
    int  src_dump_lines = 1;                       ///< number of source lines to dump on fail
    bool save_baseline  = false;                   ///< save the current run as a new baseline
    Config() noexcept   = default;
};

}  // namespace nv::ut::details
