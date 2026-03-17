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

#include "nv/i2c/eeprom_cache.h"
#include "nv/i2c/task.h"
#include "nv/i2c/common.h"
#include "nv/ipc/queue.h"
#include "nv/ipchandler/enums.h"
#include "nv/nv.h"
#include "sys/i2c/utils.h"
#include <bit>
#include <span>
#include <array>
#include <algorithm>

using namespace nv::i2c;

namespace {
constexpr uint8_t LowByteMask = 0xFF;
}  // namespace

template<uint16_t Size>
void EepromCache<Size>::init()
{
    _cache.fill(InvalidData);
    _addr_ptr = 0;
    _dirty_pages.fill(0);
}

template<uint16_t Size>
void EepromCache<Size>::load_from_eeprom()
{
    constexpr uint8_t ChunkSize = PageSize;  // Read one page at a time
    constexpr auto    Port      = nv::ipc::EepromDstPort;
    constexpr auto    Address   = nv::ipc::EepromDstAddress;

    for (uint16_t addr = 0; addr < CacheSize; addr += ChunkSize) {
        std::array<uint8_t, ChunkSize> read_buf{};
        auto                           read_span = std::span<uint8_t>(read_buf);
        if constexpr (Use2ByteAddr) {
            // 2-byte address for EEPROMs > 256 bytes (high byte first)
            std::array<uint8_t, 2> addr_bytes = {
                static_cast<uint8_t>(addr >> 8),          // High byte
                static_cast<uint8_t>(addr & LowByteMask)  // Low byte
            };
            auto write_span = std::span<uint8_t>(addr_bytes);

            if (sys::i2c::i2c_write_read(Port, Address, write_span, read_span)
                == I2cStatus::Ok) {
                std::copy_n(read_buf.begin(), ChunkSize, _cache.begin() + addr);
            }
        }
        else {
            // 1-byte address for 256B EEPROM
            auto addr_byte  = static_cast<uint8_t>(addr);
            auto write_span = std::span<uint8_t>(&addr_byte, 1);

            if (sys::i2c::i2c_write_read(Port, Address, write_span, read_span)
                == I2cStatus::Ok) {
                std::copy(read_buf.begin(), read_buf.end(), _cache.begin() + addr);
            }
        }
    }

    // Clear dirty flags - data is already in EEPROM
    _dirty_pages.fill(0);
}

template<uint16_t Size>
uint8_t EepromCache<Size>::read(uint16_t addr)
{
    return _cache.at(addr % CacheSize);
}

template<uint16_t Size>
void EepromCache<Size>::write(uint16_t addr, uint8_t data)
{
    const uint16_t idx = addr % CacheSize;
    _cache.at(idx)     = data;

    // Mark page as dirty
    const uint16_t page        = idx / PageSize;
    const uint8_t  word_idx    = page / 32;
    const uint8_t  bit_idx     = page % 32;
    _dirty_pages.at(word_idx) |= (1U << bit_idx);
}

template<uint16_t Size>
uint16_t EepromCache<Size>::get_addr_ptr()
{
    return _addr_ptr;
}

template<uint16_t Size>
void EepromCache<Size>::set_addr_ptr(uint16_t addr)
{
    _addr_ptr = addr % CacheSize;
}

template<uint16_t Size>
void EepromCache<Size>::inc_addr_ptr()
{
    _addr_ptr = (_addr_ptr + 1) % CacheSize;
}

template<uint16_t Size>
bool EepromCache<Size>::has_dirty_pages()
{
    for (auto word : _dirty_pages) {
        if (word != 0) {
            return true;
        }
    }
    return false;
}

template<uint16_t Size>
void EepromCache<Size>::clear_dirty()
{
    _dirty_pages.fill(0);
}

template<uint16_t Size>
bool EepromCache<Size>::sync_one_page()
{
    if (!has_dirty_pages()) {
        return false;
    }

    // Find first dirty page
    uint16_t page  = 0;
    bool     found = false;
    for (uint8_t word_idx = 0; word_idx < DirtyWordsCount && !found; word_idx++) {
        if (_dirty_pages.at(word_idx) != 0) {
            for (uint8_t bit_idx = 0; bit_idx < 32; bit_idx++) {
                if (_dirty_pages.at(word_idx) & (1U << bit_idx)) {
                    page  = word_idx * 32 + bit_idx;
                    found = true;
                    break;
                }
            }
        }
    }
    if (!found || page >= NumPages) {
        return false;
    }

    // Build I2C request for page write
    nv::i2c::Task::Request request{};
    request.type = nv::i2c::Task::RequestType::I2cRequest;

    auto* i2c_request = std::bit_cast<nv::i2c::I2cRequest*>(static_cast<void*>(request.data));
    i2c_request->address     = nv::ipc::EepromDstAddress;  // EEPROM address
    i2c_request->read_length = 0;
    i2c_request->src_id      = static_cast<uint8_t>(nv::ipchandler::Id::Unuse);

    const uint16_t start_addr = page * PageSize;

    if constexpr (Use2ByteAddr) {
        // Format: [addr_high, addr_low, data0, data1, ...]
        i2c_request->write_length    = PageSize + 2;  // 2-byte addr + data bytes
        i2c_request->write_buffer[0] = static_cast<uint8_t>(start_addr >> 8);  // High byte
        i2c_request->write_buffer[1] = static_cast<uint8_t>(start_addr & LowByteMask);  // Low
                                                                                        // byte
        for (uint8_t i = 0; i < PageSize; i++) {
            i2c_request->write_buffer.at(2 + i) = _cache.at(start_addr + i);
        }
    }
    else {
        // Format: [addr, data0, data1, ...]
        i2c_request->write_length    = PageSize + 1;  // 1-byte addr + data bytes
        i2c_request->write_buffer[0] = static_cast<uint8_t>(start_addr);
        for (uint8_t i = 0; i < PageSize; i++) {
            i2c_request->write_buffer.at(1 + i) = _cache.at(start_addr + i);
        }
    }

    // Queue to I2C task for destination port
    auto&                    queue = nv::ipc::Queue::make(nv::ipc::EepromI2cQueueId);
    const std::span<uint8_t> item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto                     status = queue.send(item);

    if (status == nv::ipc::Queue::Status::Ok) {
        // Clear dirty bit only on successful queue
        const uint8_t word_idx     = page / 32;
        const uint8_t bit_idx      = page % 32;
        _dirty_pages.at(word_idx) &= ~(1U << bit_idx);
        return true;
    }

    return false;
}

template<uint16_t Size>
bool EepromCache<Size>::use_2byte_addr() const
{
    return Use2ByteAddr;
}

template<uint16_t Size>
uint8_t EepromCache<Size>::page_size() const
{
    return PageSize;
}

template<uint16_t Size>
uint16_t EepromCache<Size>::size() const
{
    return CacheSize;
}

namespace nv::i2c {
template class EepromCache<MinEepromCacheSize>;
template class EepromCache<nv::ipc::EepromSize>;
}  // namespace nv::i2c
