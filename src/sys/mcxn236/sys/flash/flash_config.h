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
#include "corepdk/modules/pldm-fd/src/types/nvtypes.h"
#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace sys::flash::config {

using Address                 = uint32_t;
constexpr uint32_t SectorSize = 0x2000;
constexpr uint32_t PhraseSize = 16;  // smallest size flash could write in one operation,

constexpr Address RemapSize          = 0x80000;
constexpr Address MaxAddress         = 0x100000;
constexpr Address LoggerStartAddress = 0x70000;
constexpr Address FirmwareMaxSize    = 0x60000;

constexpr Address FmcFwAddress = 0x1008000;
constexpr Address FmcFwSize    = 0x8000;

constexpr Address PdsAddressStart = 0xF0000;

// Debug token SPI offset
constexpr Address DebugTokenSpiOffset = 0xEE000;

constexpr Address Slot0FwAddress = 0x0;
constexpr Address Slot1FwAddress = RemapSize;

// valid address range for flash
constexpr std::array<std::pair<Address, Address>, 2> ValidAddressRange = {
    {{0, MaxAddress}, {FmcFwAddress, FmcFwAddress + FmcFwSize}}
};

template<typename... Args>
requires(sizeof...(Args) > 0) && (std::is_unsigned_v<Args> && ...)
bool is_valid_address(const Args... args)
{
    // Calculate the sum of all arguments, checking for overflow
    Address total_address           = 0;
    auto    add_with_overflow_check = [&total_address](const auto value) {
        if (value > std::numeric_limits<Address>::max() - total_address) {
            return false;  // overflow would occur
        }
        total_address += static_cast<Address>(value);
        return true;
    };

    // Check each argument for overflow when adding to total
    if (!(add_with_overflow_check(args) && ...)) {
        return false;
    }

    // check if the total address is in valid address range
    for (const auto& [start, end] : ValidAddressRange) {
        if (total_address >= start && total_address < end) {
            return true;
        }
    }
    return false;
}

constexpr uint16_t McuComponentId = 0xff02;

}  // namespace sys::flash::config
