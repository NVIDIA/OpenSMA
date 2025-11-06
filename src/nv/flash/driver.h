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
#include <span>

#include "nv/common/uuid.h"
#include "nv/flash/common.h"
#include "sys/flash/driver.h"

namespace nv::flash {

class Driver : protected sys::flash::Driver
{
public:
    Driver() { api_select = ApiSelect::End; };

    nv::flash::Status init(ApiSelect cur_select);

    nv::flash::Status read(uint32_t address, uint32_t length, Buffer& buffer);

    nv::flash::Status write(uint32_t address, uint32_t length, const Buffer& buffer);

    nv::flash::Status erase(uint32_t address);

    static Status write_phrase(uint32_t address, uint32_t length, const Buffer& buffer);
    static Status check_all_erased(uint32_t address, uint32_t length);
    static Status static_erase(uint32_t address);
    static Status get_uuid(nv::common::Uuid& uuid);

    // cfpa APIs
    nv::flash::Status read_cfpa(const std::span<uint8_t>& buffer, uint32_t offset);
    nv::flash::Status
    read_cfpa_customer(const std::span<uint8_t>& buffer, uint8_t page_index, uint32_t offset);
    nv::flash::Status increase_cfpa_secure_version(uint32_t          sec_version,
                                                   KeyRollbackSelect key_rollback_select);
    nv::flash::Status increase_cfpa_image_key_revoke(uint32_t          key_revoke,
                                                     KeyRollbackSelect key_rollback_select);
    nv::flash::Status
            write_cfpa_customer(const Buffer& buffer, uint8_t page_index, uint32_t offset);
    uint8_t get_cfpa_write_page();

    // cmpa API
    nv::flash::Status read_cmpa(const std::span<uint8_t>& buffer, uint32_t offset);
    // otp APIs
    nv::flash::Status init_efuse();
    nv::flash::Status read_efuse(uint32_t& data, uint32_t offset);
    nv::flash::Status program_efuse(uint32_t data, uint32_t offset);
    nv::flash::Status read_crc(uint32_t& data);
    nv::flash::Status read_life_cycle(sys::flash::LifeCycleStatus& data);

    // Only used for logging fault
    static Status init_on_fault();
};

}  // namespace nv::flash
