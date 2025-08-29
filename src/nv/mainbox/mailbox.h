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

namespace nv::mainbox {

constexpr uint32_t MainBoxMemorySize = 0x100;

constexpr uint32_t MainBoxMemoryStart = 0x20002300;

enum class MainBoxMemoryType : uint8_t
{
    Begin       = 0,
    ElsSelfTest = Begin,
    FaultWdtRecords,
    End,
};

struct MainBoxMemoryDesc
{
    MainBoxMemoryType type;
    uint32_t          offset;
    uint32_t          size;
};

constexpr inline std::array<MainBoxMemoryDesc, 2> MainBoxMemoryDescs = {
    MainBoxMemoryDesc{    MainBoxMemoryType::ElsSelfTest,  0x0, 0x10},
    MainBoxMemoryDesc{MainBoxMemoryType::FaultWdtRecords, 0x10, 0x10}
};

static_assert(MainBoxMemoryDescs[0].size + MainBoxMemoryDescs[1].size <= MainBoxMemorySize,
              "MainBoxMemoryDescs size mismatch -- Size too large");
static_assert(MainBoxMemoryDescs.back().offset + MainBoxMemoryDescs.back().size
                  <= MainBoxMemorySize,
              "MainBoxMemoryDescs size mismatch -- Exceed memory size");

};  // namespace nv::mainbox
