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
#include "nv/bootloader.h"
#include "nv/flash/datastore.h"
#include "nv/flash/driver.h"

namespace nv::flash {

struct [[gnu::packed]] PdsInfo
{
    PdsDigest    digest;
    uint32_t     size;
    PdsDataArray _pds_buffer;
};

constexpr Address PdsAddressStart     = sys::flash::config::PdsAddressStart;
constexpr Address PdsPrimaryAddress   = PdsAddressStart;
constexpr Address PdsSecondaryAddress = PdsPrimaryAddress + 0x2000;

class Pds
{
public:
    Pds() : _boot_index(nv::bootloader::Driver::current_boot_index()){};
    void   init(nv::flash::Driver& driver);
    Status get_data(Key key, Data& data);
    Status set_data(Key key, const Data data);

private:
    PdsInfo                            primary{};
    nv::bootloader::Driver::ImageIndex _boot_index;
    Status                             update_digest_and_write(PdsInfo& pds_info);
    void get_pds_digest(const PdsInfo& pds_info, PdsDigest& digest);
    void load_pds();

    void reset_pds(PdsInfo& pds_info);
    void adjust_pds();
    bool is_valid(const PdsInfo& pds_info);
    void get_default_value(PdsDataArray& pds_buffer);

    Status read_pds(Address address, PdsInfo& pds_info);
    Status write_pds(Address address, const PdsInfo& pds_info);

    Status read_from_flash(Address address, nv::flash::Buffer& buffer);
    Status write_to_flash(Address address, nv::flash::Buffer& buffer);
    Status erase_to_flash(Address address);

    Address get_pds_address(Address address);

    nv::flash::Driver* _driver = nullptr;

    constexpr static uint8_t DigestMagic = 0x5A;  // TBD: Remove after crypto lib ready

    // WAR 5956416
    Status recover_corrupted_power_info(PdsInfo& pds_info);
};
}  // namespace nv::flash
