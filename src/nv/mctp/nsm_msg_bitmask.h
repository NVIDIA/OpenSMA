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

#include <array>
#include <cstdint>

namespace nv::mctp::nsm_msg {

// Nsm Constants
static constexpr uint8_t NvMctpSupportedNum      = 32;
static constexpr uint8_t NvMctpEventSupportedNum = 8;
static constexpr uint8_t NsmT5SuppErrorTyesNum   = NvMctpEventSupportedNum;

// For NSM Messages and Command Codes bitmasks
static constexpr void set_bit(std::array<uint8_t, NvMctpSupportedNum>& bitmask, uint8_t pos)
{
    const size_t byte_index  = pos / 8;
    const size_t bit_offset  = pos % 8;
    bitmask.at(byte_index)  |= static_cast<uint8_t>((1U << bit_offset) & UINT8_MAX);
}

// For NSM Messages and Command Codes bitmasks
static constexpr void unset_bit(std::array<uint8_t, NvMctpSupportedNum>& bitmask, uint8_t pos)
{
    const size_t byte_index  = pos / 8;
    const size_t bit_offset  = pos % 8;
    bitmask.at(byte_index)  &= static_cast<uint8_t>(~((1U << bit_offset) & UINT8_MAX));
}

static constexpr uint8_t get_bit(const std::array<uint8_t, NvMctpSupportedNum>& bitmask,
                                 uint8_t                                        pos)
{
    const size_t byte_index = pos / 8;
    if (byte_index < bitmask.size()) {
        const size_t bit_offset = pos % 8;
        auto         mask       = static_cast<uint8_t>((1U << bit_offset) & UINT8_MAX);
        auto         bit        = static_cast<uint8_t>(bitmask.at(byte_index) & mask);
        return bit;
    }
    return 0;  // not set
}

[[maybe_unused]] static constexpr bool
is_bit_set(const std::array<uint8_t, NvMctpSupportedNum>& bitmask, uint8_t pos)
{
    auto bit = get_bit(bitmask, pos);
    return bit != 0;
}

//--------- NSM Events bitmask, using NvMctpEventSupportedNum instead

// For NSM Events
static constexpr void set_bit(std::array<uint8_t, NvMctpEventSupportedNum>& bitmask,
                              uint8_t                                       pos)
{
    const size_t byte_index  = pos / 8;
    const size_t bit_offset  = pos % 8;
    bitmask.at(byte_index)  |= static_cast<uint8_t>((1U << bit_offset) & UINT8_MAX);
}

// For NSM Events
static constexpr void unset_bit(std::array<uint8_t, NvMctpEventSupportedNum>& bitmask,
                                uint8_t                                       pos)
{
    const size_t byte_index  = pos / 8;
    const size_t bit_offset  = pos % 8;
    bitmask.at(byte_index)  &= static_cast<uint8_t>(~((1U << bit_offset) & UINT8_MAX));
}

static constexpr uint8_t get_bit(const std::array<uint8_t, NvMctpEventSupportedNum>& bitmask,
                                 uint8_t                                             pos)
{
    const size_t byte_index = pos / 8;
    if (byte_index < bitmask.size()) {
        const size_t bit_offset = pos % 8;
        auto         mask       = static_cast<uint8_t>((1U << bit_offset) & UINT8_MAX);
        auto         bit        = static_cast<uint8_t>(bitmask.at(byte_index) & mask);
        return bit;
    }
    return 0;  // not set
}

[[maybe_unused]] static constexpr bool
is_bit_set(const std::array<uint8_t, NvMctpEventSupportedNum>& bitmask, uint8_t pos)
{
    auto bit = get_bit(bitmask, pos);
    return bit != 0;
}

}  // namespace nv::mctp::nsm_msg
