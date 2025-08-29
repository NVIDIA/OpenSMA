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
#include <type_traits>

#include "nv/common/debug.h"
#include "nv/common/utils.h"

namespace nv::common::enum_ops {

/// Permit a cast free simple bitwise And operator on enum class
template<typename T>
requires(std::is_enum_v<T> && std::is_unsigned_v<std::underlying_type_t<T>>)
constexpr auto operator&(T a, T b)
{
    auto v = static_cast<T>(to_underlying(a) & to_underlying(b));
    return v;
}

/// Permit a cast free simple bitwise Or operator on enum class tagged with Begin/End members.
template<typename T>
requires(std::is_enum_v<T> && std::is_unsigned_v<std::underlying_type_t<T>>)
constexpr auto operator|(T a, T b)
{
    auto v = static_cast<T>(to_underlying(a) | to_underlying(b));
    return v;
}

/// Helper for iterating over enum classes with tagged Begin/End members.
template<typename T>
requires(std::is_enum_v<T>)
constexpr const auto operator++(T& a, int)
{
    T old{a};
    a = static_cast<T>(to_underlying(a) + 1);
    NV_ASSERT(a >= T::Begin && a <= T::End);
    return old;
}

/// Helper for iterating over enum classes with tagged Begin/End members.
template<typename T>
requires(std::is_enum_v<T>)
constexpr const auto operator--(T& a, int)
{
    T old{a};
    a = static_cast<T>(to_underlying(a) - 1);
    NV_ASSERT(a >= T::Begin && a <= T::End);
    return old;
}

}  // namespace nv::common::enum_ops
