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
#include "nv/flash/driver.h"
#include "sys/flash/otp.h"
#include <algorithm>
#include <thread>

#include "nv/nv.h"
// NOLINTBEGIN

using namespace nv::flash;
using namespace std::chrono;

namespace sys::flash {
std::array<uint8_t, sys::flash::FlashSizeInBytes> _buffer{};
}

Status Driver::init(ApiSelect cur_select)
{
    return Status::Ok;
}

Status Driver::read(uint32_t address, uint32_t length, Buffer& buffer)
{
    auto itr = sys::flash::_buffer.begin() + address;
    std::copy(itr, itr + length, buffer.begin());
    std::this_thread::sleep_for(2ms);
    return Status::Ok;
}

Status Driver::write(uint32_t address, uint32_t length, const Buffer& buffer)
{
    auto itr = sys::flash::_buffer.begin() + address;
    std::copy(buffer.begin(), buffer.end(), itr);
    std::this_thread::sleep_for(2ms);
    return Status::Ok;
}

Status Driver::erase(uint32_t address)
{
    std::fill(sys::flash::_buffer.begin() + address,
              sys::flash::_buffer.begin() + address + SectorSize,
              0xFF);
    std::this_thread::sleep_for(2ms);
    return Status::Ok;
}

Status Driver::write_phrase(uint32_t address, uint32_t length, const Buffer& buffer)
{
    if (length % sys::flash::PhraseSize != 0 || address % sys::flash::PhraseSize != 0) {
        return Status::InvalidParam;
    }

    auto itr = sys::flash::_buffer.begin() + address;
    std::copy(buffer.begin(), buffer.end(), itr);
    std::this_thread::sleep_for(2ms);
    return Status::Ok;
}

Status Driver::check_all_erased(uint32_t address, uint32_t length)
{
    if (length % sys::flash::PhraseSize != 0 || address % sys::flash::PhraseSize != 0) {
        return Status::InvalidParam;
    }

    for (uint32_t i = 0; i < length; i++) {
        if (sys::flash::_buffer[address + i] != 0xFF) {
            return Status::Error;
        }
    }
    std::this_thread::sleep_for(2ms);
    return Status::Ok;
}

Status Driver::static_erase(uint32_t address)
{
    if (address % SectorSize != 0) {
        return Status::InvalidParam;
    }
    std::fill(sys::flash::_buffer.begin() + address,
              sys::flash::_buffer.begin() + address + SectorSize,
              0xFF);
    std::this_thread::sleep_for(2ms);
    return Status::Ok;
}

Status Driver::get_uuid(nv::common::Uuid& uuid)
{
    for (uint8_t i = 0; i < 16; i++) {
        uuid[i] = i;
    }

    return Status::Ok;
}

uint8_t Driver::get_cfpa_write_page()
{
    return 0;
}

nv::flash::Status Driver::read_cfpa(const std::span<uint8_t>& buffer, uint32_t offset)
{
    std::fill(buffer.begin(), buffer.end(), 0);
    return Status::Ok;
}

nv::flash::Status Driver::read_cmpa(const std::span<uint8_t>& buffer, uint32_t offset)
{
    std::fill(buffer.begin(), buffer.end(), 0);
    return Status::Ok;
}

nv::flash::Status Driver::read_cfpa_customer(const std::span<uint8_t>& buffer,
                                             uint8_t                   page_index,
                                             uint32_t                  offset)
{
    std::fill(buffer.begin(), buffer.end(), 0);
    return Status::Ok;
}

nv::flash::Status Driver::increase_cfpa_secure_version(uint32_t          sec_version,
                                                       KeyRollbackSelect key_rollback_select)
{
    return Status::Ok;
}

nv::flash::Status Driver::increase_cfpa_image_key_revoke(uint32_t          key_revoke,
                                                         KeyRollbackSelect key_rollback_select)
{
    return Status::Ok;
}

nv::flash::Status
Driver::write_cfpa_customer(const Buffer& buffer, uint8_t page_index, uint32_t offset)
{
    return Status::Ok;
}

nv::flash::Status Driver::read_efuse(uint32_t& data, uint32_t offset)
{
    return Status::Ok;
}

nv::flash::Status Driver::program_efuse(const uint32_t data, uint32_t offset)
{
    return Status::Ok;
}

nv::flash::Status Driver::read_crc(uint32_t& data)
{
    return Status::Ok;
}

nv::flash::Status Driver::init_efuse()
{
    return Status::Ok;
}

Status Driver::init_on_fault()
{
    return Status::Ok;
}
nv::flash::Status Driver::read_life_cycle([[maybe_unused]] sys::flash::LifeCycleStatus& data)
{
    auto flash_status = Status::Ok;

    return flash_status;
}
// NOLINTEND
