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

namespace nv {

/// Set of user literal tags for standard integer types.

// NOLINTBEGIN(*-runtime-int)
constexpr auto operator""_u8(unsigned long long n)
{
    // coverity[cert_int31_c_violation] - user literal operator with known usage
    return static_cast<uint8_t>(n);
}
constexpr auto operator""_u16(unsigned long long n)
{
    // coverity[cert_int31_c_violation] - user literal operator with known usage
    return static_cast<uint16_t>(n);
}
constexpr auto operator""_u32(unsigned long long n)
{
    // coverity[cert_int31_c_violation] - user literal operator with known usage
    return static_cast<uint32_t>(n);
}
constexpr auto operator""_i8(unsigned long long n)
{
    // coverity[cert_int31_c_violation] - user literal operator with known usage
    return static_cast<int8_t>(n);
}
constexpr auto operator""_i16(unsigned long long n)
{
    // coverity[cert_int31_c_violation] - user literal operator with known usage
    return static_cast<int16_t>(n);
}
constexpr auto operator""_i32(unsigned long long n)
{
    // coverity[cert_int31_c_violation] - user literal operator with known usage
    return static_cast<int32_t>(n);
}

constexpr auto operator""_bits_sizeof(unsigned long long bits)
{
    return bits / 8;
}
constexpr auto operator""_bit(unsigned long long i)
{
    return static_cast<decltype(i)>(1) << i;
}
// NOLINTEND(*-runtime-int)

};  // namespace nv
