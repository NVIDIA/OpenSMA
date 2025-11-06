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

#include "nv/common/preproc.h"
#include "nv/common/utils.h"
#include "nv/nv.h"

namespace nv::common {

/**
 * A lightweight, no exceptions, variant type for holding expected value or an error code.
 *
 * Expected to be used with NV_TRY, NV_FOE macros.
 *
 * @code
 * Expected<int, Status> get_integer_from_flash(intptr_t address) {
 *     if (flash_is_initialized() == false) return Status::NotInitialized;
 *     if (address < 0 || address >= MaxAddress) return Status::OutOfRange;
 *
 *     return internal_read_int(address);
 * }
 *
 * auto id = NV_TRY( get_integer_from_flash(0x12340000) );
 * do_something_with_integer(id);
 *
 * @endcode
 */
// clang-format off
template<typename T, typename E> 
requires(!std::is_same_v<T, E> && std::is_enum_v<E>)
class Expected final 
{
public:
    constexpr Expected() = delete;
    NV_COMMON_COPY_MOVE_CE(Expected, default);

    // NOLINTBEGIN(*-explicit-*)
    constexpr Expected(T&& value) : _value(std::move(value)), _has_value(true) {}
    constexpr Expected(E&& error) : _error(std::move(error)) {}
    // NOLINTEND(*-explicit-*)
    //
    constexpr explicit Expected(const T& value) : _value(value), _has_value(true) {}
    constexpr explicit Expected(const E& error) : _error(error) {}

    constexpr ~Expected() = delete;
    constexpr ~Expected() requires(std::is_trivially_destructible_v<T>) {
        if (_has_value) { _value.~T(); } // NOLINT
    }

    /// Return true if we how a value, not an error code.
    constexpr bool has_value() const noexcept { return _has_value; }

    // NOLINTBEGIN(*-union-access)
    /// Access the contained value, following a test for a its existence.
    T& value() & { assert(has_value()); return _value; }
    constexpr const T&  value() const &  { assert(has_value()); return _value; }
    constexpr const T&& value() const && { assert(has_value()); return std::move(_value); }

    /// Access the error code.
    constexpr E error() const { assert(!has_value()); return _error; }
    // NOLINTEND(*-union-access)

    /** 
     * This permits the following pattern:
     * @code
     * auto ret = some_function_returning_an_expected();
     * if (!ret) { // handle failure; }
     * @endcode
     */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /** 
     * This permits the following pattern:
     * @code
     * auto ret = some_function_returning_an_expected();
     * if (ret == Status::SpecificError) { // handle failure; }
     * @endcode
     */
    constexpr bool operator==(E&& err) const noexcept { return !has_value() && error() == std::move(err); }
    constexpr bool operator==(const E& err) const noexcept { return !has_value() && error() == err; }

    /** 
     * This permits the following pattern:
     * @code
     * auto ret = some_function_returning_an_expected();
     * if (ret == magic_number) { // do something with value; }
     * @endcode
     */
    constexpr bool operator==(T&& val) const noexcept { return has_value() && value() == std::move(val); }
    constexpr bool operator==(const T& val) const noexcept { return has_value() && value() == val; }

    constexpr T& operator->() { return value(); }
    constexpr T& operator*()  { return value(); }
    constexpr const T& operator*() const { return value(); }
    constexpr const T& operator->() const { return value(); }

private:
    union {
        T _value;
        E _error;
    };
    bool _has_value = false;
};
// clang-format on

/**
 * Boilerplate for typical return on error usage of Expected.
 *
 * If an error occurs, we return from the current function, returing the error value. Otherwise,
 * we unwrap the value.
 *
 * @code
 * auto value = NV_COMMON_TRY(function_returning_expected());
 * // value is now the unwrapped expected value.
 * @endcode
 */
#define NV_COMMON_TRY(call)                                                                    \
    ({                                                                                         \
        auto&& tmp = (call);                                                                   \
        if (!tmp) [[unlikely]]                                                                 \
            return tmp.error();                                                                \
        std::move(tmp);                                                                        \
    }).value()

#define NV_COMMON_TRY_PR(call, ...)                                                            \
    ({                                                                                         \
        auto&& tmp = (call);                                                                   \
        if (!tmp) [[unlikely]] {                                                               \
            nv::error(__VA_ARGS__);                                                            \
            return tmp.error();                                                                \
        }                                                                                      \
        std::move(tmp);                                                                        \
    }).value()

#define NV_COMMON_TRY_TR(call, tr, ...)                                                        \
    ({                                                                                         \
        auto&& tmp = (call);                                                                   \
        if (!tmp) [[unlikely]] {                                                               \
            nv::error(__VA_ARGS__);                                                            \
            return tr(tmp.error());                                                            \
        }                                                                                      \
        std::move(tmp);                                                                        \
    }).value()

#define NV_COMMON_TRY_ROUTER(...)                                                              \
    NV_VA_GET_4TH(__VA_ARGS__, NV_COMMON_TRY_TR, NV_COMMO_TRY_PR, NV_COMMON_TRY, )

}  // namespace nv::common
