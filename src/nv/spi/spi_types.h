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
#include <stdint.h>
#include <array>

#include "nv/common/literals.h"

namespace nv::spi::bm {

using nv::operator""_bit;

constexpr size_t BufferSize = 512;
using Buffer                = std::array<uint8_t, BufferSize>;

enum class SpiStatus : uint8_t
{
    Ok,
    Busy,
    EventTimeout,
    EventClearFail,
    EventXferFail,
    Error,
};

enum class CsPins : uint8_t
{
    Cs0 = 0,
    Cs1 = 1,
};

enum SpiFlags : uint8_t
{
    NoFlag     = 0x00U,
    NoResp     = 0_bit,
    CsAssert   = 1_bit,
    CsDeassert = 2_bit,
    AllFlags   = NoResp | CsAssert | CsDeassert,
};
inline constexpr SpiFlags operator|(SpiFlags a, SpiFlags b)
{
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<SpiFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline constexpr SpiFlags operator&(SpiFlags a, SpiFlags b)
{
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<SpiFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline constexpr SpiFlags operator~(SpiFlags a)
{
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<SpiFlags>(static_cast<uint8_t>(~static_cast<uint8_t>(a) & AllFlags));
}
inline constexpr SpiFlags& operator|=(SpiFlags& a, SpiFlags b)
{
    return a = a | b;
}
inline constexpr SpiFlags& operator&=(SpiFlags& a, SpiFlags b)
{
    return a = a & b;
}

struct SpiResult
{
    SpiStatus status;
    size_t    read_length;
    Buffer    buffer;
    SpiFlags  flags;
};

}  // namespace nv::spi::bm
