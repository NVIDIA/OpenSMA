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
#include <bit>
#include <limits>
#include <type_traits>
#include <utility>
#include <cstddef>

/// Rule of Five mixin
#define NV_COMMON_COPY_MOVE(Klass, Op)                                                         \
    Klass(Klass&&) noexcept                 = Op;                                              \
    Klass(const Klass&) noexcept            = Op;                                              \
    Klass& operator=(const Klass&) noexcept = Op;                                              \
    Klass& operator=(Klass&&) noexcept      = Op

/// Rule of Five mixin constexpr
#define NV_COMMON_COPY_MOVE_CE(Klass, Op)                                                      \
    constexpr Klass(Klass&&) noexcept                 = Op;                                    \
    constexpr Klass(const Klass&) noexcept            = Op;                                    \
    constexpr Klass& operator=(const Klass&) noexcept = Op;                                    \
    constexpr Klass& operator=(Klass&&) noexcept      = Op

namespace nv::common {

struct Empty
{};

/// Inherit from this to disable a class's ability to be copied.
class NonCopyable
{
public:
    NV_COMMON_COPY_MOVE_CE(NonCopyable, delete);

protected:
    constexpr NonCopyable() = default;
    ~NonCopyable()          = default;
};

/// Returns true if integer is a power of two.
template<typename T>
requires(std::is_integral_v<T>)
constexpr bool is_power_of_2(T x) noexcept
{
    return std::has_single_bit(x);
}

template<typename T>
requires(std::is_integral_v<T>)
constexpr bool is_32B_align(T x) noexcept
{
    // return std::has_single_bit(x);
    return (x & 0x1f) == 0;
}

template<typename T, typename U>
requires(std::is_integral_v<T> && std::is_integral_v<U>)
constexpr T align_to(T value, U alignment) noexcept
{
    if (!is_power_of_2(alignment)) {
        return std::numeric_limits<T>::max();  // meaning align_to failed
    }
    // Overflow case
    if (value > (std::numeric_limits<T>::max()) - (alignment - 1)) {
        return std::numeric_limits<T>::max();  // meaning align_to failed
    }
    return static_cast<T>((value + alignment - 1) & ~(alignment - 1)
                          & std::numeric_limits<T>::max());
}

/// Range check for Begin/End tagged enums
template<typename T>
requires(std::is_enum_v<T>)
constexpr bool is_in_range(T e) noexcept
{
    return e >= T::Begin && e < T::End;
}

/// Cast to an enums underlying type.
template<typename T>
requires(std::is_enum_v<T>)
constexpr auto to_underlying(T e) noexcept
{
    return static_cast<std::underlying_type_t<T>>(e);
}

/// Return an integer with a single bit set.
template<typename T>
requires(std::is_integral_v<T>)
constexpr T bit(T b)
{
    using UnsignedT = std::make_unsigned_t<T>;
    return std::bit_cast<T>(static_cast<UnsignedT>(1ULL) << b);
}

template<typename T>
constexpr std::underlying_type_t<T> bit(T b)
{
    using U               = std::underlying_type_t<T>;
    const auto shift      = to_underlying(b);
    const auto num_digits = std::numeric_limits<U>::digits;

    if (shift >= num_digits) {
        return 0;
    }

    U result   = 1;
    result   <<= shift;
    return result;
}

/// Return the mask of an integer with a single bit set.
template<typename T>
requires(std::is_integral_v<T>)
constexpr T mask(T b)
{
    return ~bit(b);
}

template<typename T>
requires(std::is_enum_v<T>)
constexpr auto bit(T b) -> std::underlying_type_t<T>
{
    using U               = std::underlying_type_t<T>;
    const auto shift      = to_underlying(b);
    const auto num_digits = std::numeric_limits<U>::digits;

    if (shift >= num_digits) {
        return 0;
    }

    U result   = 1;
    result   <<= shift;
    return result;
}

template<typename T, class = std::enable_if_t<std::is_unsigned<T>::value>>
constexpr inline T sub(const T& a, const T& b)
{
    if (a < b) {
        return 0u;
    }
    return a - b;
}

template<typename T, class = std::enable_if_t<std::is_unsigned<T>::value>>
constexpr inline T add(const T& a, const T& b)
{
    if (a > std::numeric_limits<T>::max() - b) {
        return std::numeric_limits<T>::max();
    }
    return a + b;
}

template<typename T, class = std::enable_if_t<std::is_unsigned<T>::value>>
constexpr inline T mul(const T& a, const T& b)
{
    if (b == 0) {
        return 0;
    }
    if (a > std::numeric_limits<T>::max() / b) {
        return std::numeric_limits<T>::max();
    }
    return a * b;
}

/// Helper struct for common::IgnoreReturn.
struct IgnoreReturnSink
{
    template<typename T>
    [[gnu::always_inline]] constexpr auto& operator=(T&& v) const noexcept  // NOLINT
    {
        if (v) {}
        return *this;
    }
};

enum FirmwareState
{
    Unknown              = 0,
    Activated            = 1,
    PendingActivation    = 2,
    Staged               = 3,
    WriteInProgress      = 4,
    Inactive             = 5,
    FailedAuthentication = 6,
};

/**
 * Assign to this to purposely ignore a return value with zero overhead.
 * @code{cpp}
 * nv::common::IgnoreReturn = release_semaphore();
 * @endcode
 */
constexpr inline IgnoreReturnSink IgnoreReturn{};

/// Round a floating value to nearest integer.
template<typename U, typename T = int>
T rint(U&& val)
{
    return static_cast<T>(std::forward(val) + 0.5);
}

}  // namespace nv::common
