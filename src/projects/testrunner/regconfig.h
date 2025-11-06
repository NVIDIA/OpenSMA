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

// TODO: Auto generate this file with compiler script from <platform>.yaml

#pragma once
#include <cstdint>

#include "nv/reg_table/common.h"
#include "nv/reg_ds/common.h"

// Register Table config
namespace nv::reg_table {

constexpr uint32_t TableIdentifier = 0x5A5A5A5A;

// Register table entries for this project
enum class TableEntry : uint8_t
{
    COMMON_TABLE_ENTRY_IDS BeginSpecial = 254,

    // GPIO events and status
    ThermOvertN = BeginSpecial,
    EndSpecial,
};

// size of the register table for this project
constexpr std::size_t TableSize = static_cast<std::size_t>(TableEntry::EndCommon)
                                - static_cast<std::size_t>(TableEntry::BeginCommon)
                                + static_cast<std::size_t>(TableEntry::EndSpecial)
                                - static_cast<std::size_t>(TableEntry::BeginSpecial) + 1;

// Event reason codes
enum class EventReason : uint16_t
{
    COMMON_REASON_CODES
};

}  // namespace nv::reg_table

// Register Table downstream config
namespace nv::reg_ds {

// Defines the unique set of downstream API handles supported by this project
enum class Handle : uint16_t
{
    COMMON_DS_HANDLES

        // Info API
        InfoEntryCount = InfoCommonHandleCount,

    // Event API
    EventEntryCount = EventCommonHandleCount,

    // GPIO API
    GpioThermOvertN = GpioCommonHandleCount,
    GpioEntryCount,

    NotImplemented
};
constexpr std::size_t InfoSize  = static_cast<std::size_t>(Handle::InfoEntryCount);
constexpr std::size_t EventSize = static_cast<std::size_t>(Handle::EventEntryCount);
constexpr std::size_t GpioSize  = static_cast<std::size_t>(Handle::GpioEntryCount);

}  // namespace nv::reg_ds
