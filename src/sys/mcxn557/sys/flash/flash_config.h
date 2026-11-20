/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#if !defined(CPU_MCXN557T)
#error "sys/mcxn557 flash_config.h is only for the MCXN557T (2 MB) platform"
#endif

namespace sys::flash::config {

using Address                 = uint32_t;
constexpr uint32_t SectorSize = 0x2000;
constexpr uint32_t PhraseSize = 16;  // smallest size flash could write in one operation,

// MCXN557 2 MB layout (SPS "2MB MCU Layout"). The flash remap (CMPA FLASH_REMAP_SIZE = 23)
// swaps two 768 KB windows: Slot 0 at 0x000000 and Slot 1 at the bank 1 base 0x100000. Each
// window holds code (688 KB), config (64 KB) and the PQC signature (16 KB). The rest of bank 0
// is bank-fixed and shared by both slots:
//   0x0C0000 L3 certificate  (8 KB)
//   0x0C2000 PQC certificate (16 KB)
//   0x0C6000 Padding         (8 KB)
//   0x0C8000 Debug token     (16 KB)
//   0x0CC000 Log metadata    (8 KB)
//   0x0CE000 Log entries     (96 KB)
//   0x0E6000 Fault entries   (8 KB)
//   0x0E8000 Reserved        (32 KB)
//   0x0F0000 PDS             (64 KB)
// 0x1C0000 - 0x200000 in bank 1 is reserved.
constexpr Address RemapSize          = 0xC0000;  // size of each remap window
constexpr Address MaxAddress         = 0x200000;
constexpr Address LoggerStartAddress = 0xCC000;  // log metadata 8 KB, then log entries
constexpr Address LogEntryFlashSize  = 0x18000;  // 96 KB; fault entries follow at 0xE6000
constexpr Address FirmwareMaxSize    = 0xC0000;  // code + config + PQC signature

constexpr Address FmcFwAddress = 0x1008000;
constexpr Address FmcFwSize    = 0x8000;

constexpr Address PdsAddressStart = 0xF0000;

constexpr Address L3CertAddress  = 0xC0000;
constexpr Address PqcCertAddress = 0xC2000;
constexpr Address PqcCertSize    = 0x4000;

// Debug token SPI offset
constexpr Address DebugTokenSpiOffset = 0xC8000;
constexpr Address DebugTokenSize      = 0x4000;

constexpr Address Slot0FwAddress = 0x0;
constexpr Address Slot1FwAddress = 0x100000;

static_assert(FirmwareMaxSize <= RemapSize, "firmware must fit in the remap window");
static_assert(L3CertAddress >= RemapSize, "L3 certificate must be outside the remap window");
static_assert(L3CertAddress + SectorSize <= PqcCertAddress,
              "L3 certificate must end before the PQC certificate");
static_assert(PqcCertAddress + PqcCertSize <= DebugTokenSpiOffset,
              "PQC certificate must end before the debug token");
static_assert(DebugTokenSpiOffset + DebugTokenSize <= LoggerStartAddress,
              "debug token must end before the log metadata");
static_assert(LoggerStartAddress + SectorSize + LogEntryFlashSize + SectorSize
                  <= PdsAddressStart,
              "log and fault entries must end before PDS");

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
