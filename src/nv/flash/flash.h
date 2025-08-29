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
#include "sys/flash/otp.h"

#include "nv/bootloader.h"
#include "nv/flash/common.h"
#include "nv/flash/datastore.h"
#include "nv/ipc/queue.h"
namespace nv::flash {

class Flash
{
public:
    static Status read(Address                   address,
                       const std::span<uint8_t>& buffer,
                       nv::ipc::Queue::Usecs     timeout = nv::ipc::Queue::Usecs::max());
    static Status write(Address                         address,
                        const std::span<const uint8_t>& buffer,
                        nv::ipc::Queue::Usecs           timeout = nv::ipc::Queue::Usecs::max());
    static Status erase(Address               address,
                        nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    static Status background_copy_start();

    static Status
    background_copy_query(ProgressPercent&      progress,
                          nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    static Status
    get_data(Key key, Data& data, nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    static Status set_data(Key                   key,
                           const Data&           data,
                           nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    static Status get_data_from_kernel(Key key, Data& data);

    static Status set_data_from_kernel(Key key, const Data& data);

    static Address get_flash_address(Address                            address,
                                     nv::bootloader::Driver::ImageIndex boot_index);

    // Only used for logging fault
    static Status write_phrase(Address address, const std::span<uint8_t>& buffer);
    static Status check_all_erased(uint32_t address, uint32_t length);
    static Status static_erase(uint32_t address);

    // cfpa
    static Status read_cfpa(const std::span<uint8_t>& buffer,
                            uint32_t                  offset,
                            nv::ipc::Queue::Usecs     timeout = nv::ipc::Queue::Usecs::max());

    static Status read_cmpa(const std::span<uint8_t>& buffer,
                            uint32_t                  offset,
                            nv::ipc::Queue::Usecs     timeout = nv::ipc::Queue::Usecs::max());

    static Status
    write_secure_fw_version(uint32_t              sec_version,
                            nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());
    static Status
    write_key_revoke(uint32_t              key_version,
                     nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    static Status
    read_secure_fw_version(uint32_t&             sec_version,
                           nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    static Status read_key_revoke(uint32_t&             key_revoke,
                                  nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());
    static Status
    read_cfpa_customer(const std::span<uint8_t>& buffer,
                       uint32_t                  offset,
                       nv::ipc::Queue::Usecs     timeout = nv::ipc::Queue::Usecs::max());
    // otp operation
    static Status read_efuse(Address               address,
                             uint32_t&             data,
                             nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());
    static Status program_efuse(Address               address,
                                const uint32_t        data,
                                nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());
    // otp operation
    static Status get_life_cycle(sys::flash::LifeCycleStatus& data,
                                 nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    static Status read_crc(uint32_t&             data,
                           nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    // Only used for logging fault
    static Status init_on_fault();

#if 0
    static Status
    write_cfpa_customer(const std::span<uint8_t>& buffer,
                        uint32_t                  offset,
                        nv::ipc::Queue::Usecs     timeout = nv::ipc::Queue::Usecs::max());
#endif
};

}  // namespace nv::flash
