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
#include <stdint.h>
#include <array>
#include "nv/ipc/queue.h"

namespace nv::i2c {

constexpr uint16_t MinEepromCacheSize = 1;

template<uint16_t Size>
class EepromCache final
{
public:
    static constexpr uint16_t CacheSize       = (Size == 0) ? MinEepromCacheSize : Size;
    static constexpr uint8_t  PageSize        = (CacheSize <= 256) ? 8 : 32;
    static constexpr bool     Use2ByteAddr    = (CacheSize > 256);
    static constexpr uint16_t NumPages        = (CacheSize + PageSize - 1) / PageSize;
    static constexpr uint8_t  InvalidData     = 0xFF;
    static constexpr uint8_t  DirtyWordsCount = (NumPages + 31) / 32;

    using Table     = std::array<uint8_t, CacheSize>;
    using DirtyBits = std::array<uint32_t, DirtyWordsCount>;

    EepromCache() = default;

    void     init();
    void     load_from_eeprom();
    uint8_t  read(uint16_t addr);
    void     write(uint16_t addr, uint8_t data);
    uint16_t get_addr_ptr();
    void     set_addr_ptr(uint16_t addr);
    void     inc_addr_ptr();
    bool     has_dirty_pages();
    void     clear_dirty();
    bool     sync_one_page();
    bool     use_2byte_addr() const;
    uint8_t  page_size() const;
    uint16_t size() const;

private:
    Table     _cache;
    uint16_t  _addr_ptr;
    DirtyBits _dirty_pages;
};

}  // namespace nv::i2c
