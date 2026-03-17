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

// This header contains definitions to assist with fixed-point arithmetic

#pragma once

#include <cassert>
#include <cstdint>
#include <limits>

namespace nv {

// Fixed-point type aliases with naming convention:
// <sign>FXP<bits above radix>_<bits below radix>
using SFXP32_0  = int32_t;
using SFXP22_10 = int32_t;
using SFXP12_20 = int32_t;
using SFXP1_31  = uint32_t;

using UFXP8_0 = uint8_t;

constexpr uint32_t sfxp32_0_to_sfxp22_10_lshift = 10;
constexpr uint32_t sfxp1_31_to_sfxp22_10_lshift = 21;

// Returns whether `val` can be represented as a 32-bit signed integer
consteval bool in_s32_range(long double val)
{
    return val <= std::numeric_limits<int32_t>::max()
        && val >= std::numeric_limits<int32_t>::min();
}

// Converts `val` to a fixed-point 32-bit signed integer where `radix_bits` is
// the number of bits below the radix. The number of bits above the radix is
// 32 - `radix_bits`.
template<uint32_t _radix_bits>
requires(_radix_bits < 32)
consteval int32_t to_s32fxp(long double val)
{
    const long double multiplier = 1U << _radix_bits;
    const long double result     = val * multiplier;
    assert(in_s32_range(result));
    const long double round_offset = val >= 0.0 ? 0.5 : -0.5;
    return static_cast<int32_t>(result + round_offset);
}

consteval SFXP32_0 to_sfxp32_0(long double val)
{
    return SFXP32_0{to_s32fxp<0>(val)};
}

consteval SFXP22_10 to_sfxp22_10(long double val)
{
    return SFXP22_10{to_s32fxp<10>(val)};
}

consteval SFXP12_20 to_sfxp12_20(long double val)
{
    return SFXP12_20{to_s32fxp<20>(val)};
}

constexpr SFXP22_10 sfxp32_0_to_sfxp22_10(SFXP32_0 val)
{
    return val << 10;
}

constexpr SFXP32_0 sfxp22_10_to_sfxp32_0(SFXP22_10 val)
{
    return val >> 10;
}

constexpr SFXP22_10 sfxp12_20_to_sfxp22_10(SFXP12_20 val)
{
    return val >> 10;
}

constexpr SFXP22_10 sfxp1_31_to_sfxp22_10(SFXP1_31 val)
{
    return val >> sfxp1_31_to_sfxp22_10_lshift;
}

constexpr float sfxp22_10_to_float(SFXP22_10 val)
{
    return static_cast<float>(val) / (1U << 10);
}

// Multiply two SFXP22_10 values and return SFXP22_10 result.
// When multiplying (a/2^10) × (b/2^10), the result is scaled by 2^20.
// This function corrects the scale by shifting right 10 bits.
// NOTE: Callers must ensure no overflow using static_assert with no_s32_multiply_overflow()
constexpr SFXP22_10 sfxp22_10_multiply(SFXP22_10 a, SFXP22_10 b)
{
    return (a * b) >> 10;
}

// Returns whether the product of `a` and `b` can be represented as a 32-bit
// signed integer (i.e. no overflow).
consteval bool no_s32_multiply_overflow(int32_t a, int32_t b)
{
    const long double result = static_cast<long double>(a) * static_cast<long double>(b);
    return in_s32_range(result);
}

// Returns the minimimum left shift to apply to one of the operands of a signed
// 32-bit multiply to prevent overflow.
consteval uint32_t s32_mul_overflow_prevent_shift(int32_t a, int32_t b)
{
    uint32_t shift = 0;
    while (!no_s32_multiply_overflow(a, b >> shift)) {
        ++shift;
        assert(shift < 32);
    }
    return shift;
}

}  // namespace nv
