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

#include <cstdint>

namespace nv::reg_table {

#define COMMON_TABLE_ENTRY_IDS                                                                 \
    BeginCommon = 1, TableIdentifier = BeginCommon, FlashUsageAndSize, RamUsageAndSize,        \
    BootTime, CpuUsage, WatchdogResetInt, EndCommon, Timestamp = 255,

#define COMMON_REASON_CODES WatchdogReset = 1,

#define COMMON_TABLE_ENTRIES                                                                   \
    {TableEntry::TableIdentifier,                                                              \
     0,                                                                                        \
     reg_ds::Handle::InfoGetTableIdentifier,                                                   \
     ControlBitsDef,                                                                           \
     DsApiType::Info,                                                                          \
     0},                                                                                       \
        {TableEntry::FlashUsageAndSize,                                                        \
         0,                                                                                    \
         reg_ds::Handle::InfoGetFlashUsageAndSize,                                             \
         ControlBitsDef,                                                                       \
         DsApiType::Info,                                                                      \
         0},                                                                                   \
        {TableEntry::RamUsageAndSize,                                                          \
         0,                                                                                    \
         reg_ds::Handle::InfoGetRamUsageAndSize,                                               \
         ControlBitsDef,                                                                       \
         DsApiType::Info,                                                                      \
         0},                                                                                   \
        {TableEntry::BootTime,                                                                 \
         0,                                                                                    \
         reg_ds::Handle::InfoGetBootTime,                                                      \
         ControlBitsDef,                                                                       \
         DsApiType::Info,                                                                      \
         0},                                                                                   \
        {TableEntry::CpuUsage,                                                                 \
         0,                                                                                    \
         reg_ds::Handle::InfoGetCpuUsage,                                                      \
         ControlBitsDef,                                                                       \
         DsApiType::Info,                                                                      \
         0},                                                                                   \
        {TableEntry::WatchdogResetInt,                                                         \
         0,                                                                                    \
         reg_ds::Handle::EventWatchdogResetInt,                                                \
         ControlBitsRas,                                                                       \
         DsApiType::Event,                                                                     \
         0},

}  // namespace nv::reg_table
