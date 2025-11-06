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
#include <span>

#include "nv/flash/flash.h"
#include "nv/logger/common.h"

namespace nv::logger {

constexpr uint8_t  FaultDataSize      = 48;
constexpr uint32_t FaultVersionSize   = 7;
constexpr uint8_t  DumpHeadMagicFault = 0x9F;
constexpr uint8_t  DumpTailMagicFault = 0xF9;
using FaultBuffer                     = std::array<uint8_t, FaultDataSize>;
enum class Fault : uint8_t
{
    Hard = 0x00,
    Bus,
    Memory,
    Sovf,
    Usage,
    Secure,
    RuntimeWdt,
    EccHandle,
    RuntimeWdtAdditional,
    StackChkFail,
    CpuTime,
    SecVio,
};

enum class ExcReturn : uint32_t
{
    HandlerModeMSP    = 0xFFFFFFF1,
    ThreadModeMSP     = 0xFFFFFFF9,
    HandlerModeMSPFlt = 0xFFFFFFE1,
    ThreadModeMSPFlt  = 0xFFFFFFE9,
    ThreadModePSP     = 0xFFFFFFFD,
    ThreadModePSPFlt  = 0xFFFFFFED,
};

struct [[gnu::packed]] FaultItem
{
    Fault              event;       //  1 byte
    FwVersion          version;     //  7 byte
    uint8_t            core_id;     //  1 byte
    uint8_t            reserve[7];  //  7 byte
    FaultBuffer        data;
    std::span<uint8_t> to_span() const
    {
        return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
    }
};
constexpr uint32_t FaultEntrySize = sizeof(FaultItem);

static_assert(FaultEntrySize == 64, "size of FaultItem is not 64");

constexpr uint32_t FaultEntryNum = nv::flash::SectorSize / FaultEntrySize;

class FaultLogger
{
public:
    static bool
    fault(Fault fault, const FaultBuffer& data = {}, uint8_t size = 0, uint8_t core_id = 0);
    static uint32_t get_fatal_download_size();
};

}  // namespace nv::logger