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
#include <array>
#include "nv/common/utils.h"
namespace nv::mainbox {

constexpr uint32_t MainBoxMemorySize = 0x100;

constexpr uint32_t MainBoxMemoryStart = 0x20002300;

enum class MainBoxMemoryType : uint8_t
{
    Begin       = 0,
    ElsSelfTest = Begin,
    FaultWdtRecords,
    BoardSerialNumber,
    UsbPortReset,
    End,
};

struct MainBoxMemoryDesc
{
    MainBoxMemoryType type;
    uint32_t          offset;
    uint32_t          size;
};

constexpr inline std::array<MainBoxMemoryDesc, 5> MainBoxMemoryDescs = {
    MainBoxMemoryDesc{      MainBoxMemoryType::ElsSelfTest,  0x0, 0x10},
    MainBoxMemoryDesc{  MainBoxMemoryType::FaultWdtRecords, 0x10, 0x10},
    MainBoxMemoryDesc{MainBoxMemoryType::BoardSerialNumber, 0x20, 0x10},
    MainBoxMemoryDesc{     MainBoxMemoryType::UsbPortReset, 0x30, 0x10},
};

static_assert(MainBoxMemoryDescs[0].size + MainBoxMemoryDescs[1].size <= MainBoxMemorySize,
              "MainBoxMemoryDescs size mismatch -- Size too large");
static_assert(MainBoxMemoryDescs.back().offset + MainBoxMemoryDescs.back().size
                  <= MainBoxMemorySize,
              "MainBoxMemoryDescs size mismatch -- Exceed memory size");

inline void write_mailbox(const MainBoxMemoryType& type, std::span<const uint8_t> data)
{
    // coverity[cert_ctr50_cpp_violation] -- won't out-of-bound access
    auto& desc = MainBoxMemoryDescs[static_cast<uint8_t>(type)];
    if (data.size() > desc.size) {
        return;
    }
    uint32_t address = MainBoxMemoryStart + desc.offset;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    memcpy(reinterpret_cast<uint8_t*>(address), data.data(), data.size());
}

inline void read_mailbox(const MainBoxMemoryType& type, std::span<uint8_t> data)
{
    // coverity[cert_ctr50_cpp_violation] -- won't out-of-bound access
    auto& desc = MainBoxMemoryDescs[static_cast<uint8_t>(type)];
    if (data.size() > desc.size) {
        return;
    }
    uint32_t address = MainBoxMemoryStart + desc.offset;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    memcpy(data.data(), reinterpret_cast<uint8_t*>(address), data.size());
}

inline void write_mailbox_u32(MainBoxMemoryType type, uint32_t value)
{
    // coverity[cert_ctr50_cpp_violation] -- won't out-of-bound access
    auto& desc = MainBoxMemoryDescs[static_cast<uint8_t>(type)];
    if (sizeof(value) > desc.size) {
        return;
    }
    uint32_t address = MainBoxMemoryStart + desc.offset;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    memcpy(reinterpret_cast<uint8_t*>(address), &value, sizeof(value));
}

inline void read_mailbox_u32(MainBoxMemoryType type, uint32_t& value)
{
    // coverity[cert_ctr50_cpp_violation] -- won't out-of-bound access
    auto&    desc    = MainBoxMemoryDescs[static_cast<uint8_t>(type)];
    uint32_t address = nv::common::add(MainBoxMemoryStart, desc.offset);
    // NOLINTNEXTLINE(*-reinterpret-cast)
    memcpy(&value, reinterpret_cast<uint8_t*>(address), sizeof(value));
}

};  // namespace nv::mainbox
