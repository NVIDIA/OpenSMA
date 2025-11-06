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

namespace nv::common {

/**
 * UUID array in big endian format with a fast conversion to string.
 */
class [[gnu::packed]] Uuid : public std::array<uint8_t, 0x10>
{
public:
    constexpr static auto ByteLength = 37;
    using StringType                 = std::array<char, ByteLength>;
    /**
     * Fast convert to the canonical textual representation.
     *
     * @return string representation as a null perminated char array.
     */
    StringType to_string() const
    {
        decltype(to_string()) ret{};
        auto                  dst = ret.begin();
        auto                  src = cbegin();
        for (int i = 0; i < 4; i++) {
            dst = to_hex(dst, *src++);
        }
        for (int i = 0; i < 3; i++) {
            *dst++ = '-';
            dst    = to_hex(dst, *src++);
            dst    = to_hex(dst, *src++);
        }
        *dst++ = '-';
        for (int i = 0; i < 6; i++) {
            dst = to_hex(dst, *src++);
        }
        *dst = '\0';
        return ret;
    }

private:
    /**
     * Convert to hex a printf styled '%02x'
     *
     * @param[in,out] dst   destination string.
     * @param[in]     val   value to be converted to hexadecimal.
     * @return returns a point to the next character.
     */
    StringType::iterator to_hex(StringType::iterator dst, uint8_t val) const
    {
        static constexpr std::array digits{
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
        constexpr auto LowerNibbleMask = 0x0Fu;
        constexpr auto UpperNibbleMask = 0xF0u;

        dst[1] = digits.at(val & LowerNibbleMask);
        dst[0] = digits.at((val & UpperNibbleMask) >> 4u);
        return dst + 2;
    }
};

static_assert(sizeof(Uuid) == 0x10);

}  // namespace nv::common
