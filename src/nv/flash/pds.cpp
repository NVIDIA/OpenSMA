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
#include "nv/flash/pds.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "nv/common/utils.h"
#include "nv/flash/flash.h"
#include "nv/nv.h"
#include "nv/flash/pds_pwr_smoothing_defaults.h"

using namespace nv::flash;

#if 0
#include "fsl_debug_console.h"
void dump_pds(const PdsInfo& pds_info)
{
    DbgConsole_Printf("Digest: \n");
    for (uint8_t dig : pds_info.digest) {
        DbgConsole_Printf("0x%x ", dig);
    }
    DbgConsole_Printf("\n");

    DbgConsole_Printf("Size:0x%x\nData:\n", pds_info.size);
    for (uint32_t i = 0; i < pds_info._pds_buffer.size(); i++) {
        if (i % 16 == 0 && i) {
            DbgConsole_Printf("\n");
        }
        DbgConsole_Printf("0x%x ", pds_info._pds_buffer[i].data);
    }
    DbgConsole_Printf("\n");
}

void dump_array(std::array<uint8_t, 256>& buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        if (i % 16 == 0 && i) {
            DbgConsole_Printf("\n");
        }
        DbgConsole_Printf("0x%x ", buf[i]);
    }
    DbgConsole_Printf("\n");
}

#endif

void Pds::init(Driver& driver)
{
    _driver = &driver;
    load_pds();
    adjust_pds();
}

Status Pds::read_from_flash(Address address, Buffer& buffer)
{
    if (_driver == nullptr) {
        return Status::Error;
    }
    return _driver->read(address, sizeof(buffer), buffer);
}

Status Pds::write_to_flash(Address address, Buffer& buffer)
{
    if (_driver == nullptr) {
        return Status::Error;
    }
    return _driver->write(address, sizeof(buffer), buffer);
}

Status Pds::erase_to_flash(Address address)
{
    if (_driver == nullptr) {
        return Status::Error;
    }
    return _driver->erase(address);
}

void Pds::adjust_pds()
{
    if (primary.size == PdsSize) {
        return;
    }
    const bool IsUpgrade = primary.size < PdsSize;
    if (IsUpgrade) {
        nv::info("Pds upgrade\n");
        // Pds upgrade, assign the default Pds values to new items
        PdsDataArray table{};
        get_default_value(table);
        std::copy(table.begin() + primary.size,
                  table.end(),
                  primary._pds_buffer.begin() + primary.size);
    }
    // common to upgrade and downgrade
    primary.size = PdsSize;
    update_digest_and_write(primary);
}

/*
For MCXN-236, there is remap feature. The address for two image is different.
For image 0, physical address equals to the virtual address.
For image 1, to access address in bank0, we will need to add remap size.
To access address in bank1, we will need to subtract remap size.
*/
Address Pds::get_pds_address(Address address)
{
    return Flash::get_flash_address(address, _boot_index);
};

Status Pds::get_data(Key key, Data& data)
{
    if (key < Key::PdsStart || key >= Key::PdsEnd) {
        return Status::InvalidParam;
    }
    const uint32_t Index = pds_index(key);
    data                 = primary._pds_buffer.at(Index).data;
    // nv::info("Pds::get_data data:%d \n", data);
    return Status::Ok;
}

Status Pds::set_data(Key key, const Data data)
{
    if (key < Key::PdsStart || key >= Key::PdsEnd) {
        return Status::InvalidParam;
    }

    const uint32_t Index = pds_index(key);
    if (primary._pds_buffer.at(Index).data == data) {
        return Status::Ok;
    }
    // nv::info("Pds::set_data data:%d\n", data);
    primary._pds_buffer.at(Index).data = data;
    update_digest_and_write(primary);
    return Status::Ok;
}

// NOLINTBEGIN(misc-unused-parameters)
void Pds::get_pds_digest(const PdsInfo& pds_info, PdsDigest& digest)
{
    // TBD: Replace it with proper hash algorithm after crypto lib ready
    std::fill(digest.begin(), digest.end(), DigestMagic);
}
// NOLINTEND(misc-unused-parameters)

void Pds::reset_pds(PdsInfo& pds_info)
{
    nv::info("Pds::reset_pds\n");
    pds_info = {};
    get_default_value(pds_info._pds_buffer);
    pds_info.size = PdsSize;
    update_digest_and_write(pds_info);
}

void Pds::get_default_value(PdsDataArray& table)
{
    nv::info("Pds::get_default_value\n");
    auto set_default = [&](Key key, Data value) {
        table.at(pds_index(key)).data = value;
    };
    set_default(Key::PdsBootableSlot0, 0);
    set_default(Key::PdsBootableSlot1, 0);
    set_default(Key::PdsUpdateState, 0);
    set_default(Key::PdsUpdateSlot, 0);
    set_default(Key::PdscfpaCustomerLastUpdated, 0);
    set_default(Key::PdsDbgTokenNonceValid, UINT8_MAX);
    set_default(Key::PdsDbgTokenNonce1, 0);
    set_default(Key::PdsDbgTokenNonce2, 0);
    set_default(Key::PdsDbgTokenNonce3, 0);
    set_default(Key::PdsDbgTokenNonce4, 0);
    set_default(Key::PdsBackgroundSetup, 0);
    set_default(Key::PdsBackgroundSetupOneTime, 0);

    // set default value for simultaneous fuse addresses
    for (auto i = static_cast<uint32_t>(Key::PdsOtpSimultaneousFuseAddress0);
         i <= static_cast<uint32_t>(Key::PdsOtpSimultaneousFuseAddress81);
         i++) {
        set_default(static_cast<Key>(i), 0);
    }

    // SoC Power Smoothing default settings
    // These defaults are used on first boot or when PDS is reset
    set_default(Key::PdsSoCPowerSmoothEnabled, nv_pds_default_soc_offset_policy_enabled());
    set_default(Key::PdsSoCPowerSmoothCurrentPresetIndex, 0);  // Default preset 0
    set_default(Key::PdsSoCPowerBrakeEnabled, nv_pds_default_soc_power_brake_enabled());
    set_default(Key::PdsMaxACPowerRampRate, 0);  // 0 W/s (disabled)

    // Leak Detection thresholds (0 = use config.h defaults)
    for (auto k = static_cast<uint32_t>(Key::PdsLeakDetSlot0Part0);
         k <= static_cast<uint32_t>(Key::PdsLeakDetSlot3Part1);
         k++) {
        set_default(static_cast<Key>(k), 0);
    }
    set_default(Key::PdsSoCThermBrakeEnabled, nv_pds_default_soc_therm_brake_enabled());

    for (auto k = static_cast<uint32_t>(Key::PdsPwrSmoothCalib0);
         k <= static_cast<uint32_t>(Key::PdsPwrSmoothCalib14);
         k++) {
        set_default(static_cast<Key>(k), 0);  // 0.0f until factory write
    }
}

void Pds::load_pds()
{
    nv::info("Pds::load_pds \n");
    PdsInfo secondary{};
    auto    read_status_primary   = read_pds(PdsPrimaryAddress, primary);
    auto    read_status_secondary = read_pds(PdsSecondaryAddress, secondary);

    (void)recover_corrupted_power_info(primary);
    (void)recover_corrupted_power_info(secondary);
    write_pds(PdsPrimaryAddress, primary);
    write_pds(PdsSecondaryAddress, secondary);

    auto primary_is_valid   = read_status_primary == Status::Ok ? is_valid(primary) : false;
    auto secondary_is_valid = read_status_secondary == Status::Ok ? is_valid(secondary) : false;

    if (primary_is_valid && secondary_is_valid) {
        nv::info("Pds: load from primary\n");
    }
    else if (!primary_is_valid && secondary_is_valid) {
        nv::info("Pds: primary corrupt, recovery from secondary\n");
        primary = secondary;
        write_pds(PdsPrimaryAddress, primary);
    }
    else if (primary_is_valid && !secondary_is_valid) {
        nv::info("Pds: secondary corrupt, recovery from primary\n");
        secondary = primary;
        write_pds(PdsSecondaryAddress, secondary);
    }
    else {
        nv::info("Pds: Reset to default\n");
        reset_pds(primary);
    }
}

bool Pds::is_valid(const PdsInfo& pds_info)
{
    PdsDigest digest;
    get_pds_digest(pds_info, digest);
    return pds_info.digest == digest;
}

Status Pds::update_digest_and_write(PdsInfo& pds_info)
{
    // Calculate digest
    PdsDigest digest;
    get_pds_digest(pds_info, digest);
    pds_info.digest = digest;
    write_pds(PdsPrimaryAddress, pds_info);
    write_pds(PdsSecondaryAddress, pds_info);

    return Status::Ok;
}

Status Pds::write_pds(Address address, const PdsInfo& pds_info)
{
    // Flash erase
    Address cur_address = get_pds_address(address);
    auto    status      = erase_to_flash(cur_address);
    if (status != Status::Ok) {
        return Status::Error;
    }

    // Flash write
    Buffer   write_buffer{};
    uint32_t remain_size = sizeof(pds_info);
    Address  cur_offset  = 0;

    while (remain_size > 0) {
        const uint32_t CopySize = std::min(remain_size, BufferSize);
        std::memcpy(static_cast<void*>(&write_buffer[0]),
                    static_cast<void*>(std::bit_cast<uint8_t*>(&pds_info) + cur_offset),
                    CopySize);
        auto status = write_to_flash(cur_address, write_buffer);
        if (status != Status::Ok) {
            return Status::Error;
        }
        remain_size = nv::common::sub(remain_size, CopySize);
        cur_address = nv::common::add(cur_address, CopySize);
        cur_offset  = nv::common::add(cur_offset, CopySize);
    }
    return Status::Ok;
}

Status Pds::read_pds(Address address, PdsInfo& pds_info)
{
    // Flash read
    Buffer   read_buffer;
    uint32_t remain_size = sizeof(pds_info);
    Address  cur_offset  = 0;
    Address  cur_address = get_pds_address(address);

    while (remain_size > 0) {
        const uint32_t CopySize = std::min(remain_size, BufferSize);
        auto           status   = read_from_flash(cur_address, read_buffer);
        if (status != Status::Ok) {
            return Status::Error;
        }
        std::memcpy(static_cast<void*>(std::bit_cast<uint8_t*>(&pds_info) + cur_offset),
                    static_cast<void*>(&read_buffer[0]),
                    CopySize);
        remain_size = nv::common::sub(remain_size, CopySize);
        cur_address = nv::common::add(cur_address, CopySize);
        cur_offset  = nv::common::add(cur_offset, CopySize);
    }

    return Status::Ok;
}

// WAR 5956416
Status Pds::recover_corrupted_power_info(PdsInfo& pds_info)
{
    bool data_valid = true;
    for (auto i = static_cast<uint32_t>(Key::PdsSoCPowerSmoothEnabled);
         i <= static_cast<uint32_t>(Key::PdsAdcCalibrationComplete);
         i++) {
        const uint32_t idx = pds_index(static_cast<Key>(i));
        if (i == static_cast<uint32_t>(Key::PdsSoCPowerSmoothCurrentPresetIndex)) {
            if (pds_info._pds_buffer.at(idx).data != 0 && pds_info._pds_buffer.at(idx).data != 1
                && pds_info._pds_buffer.at(idx).data != 2
                && pds_info._pds_buffer.at(idx).data != 3) {
                data_valid = false;
                break;
            }
            continue;
        }
        else if (i == static_cast<uint32_t>(Key::PdsMaxACPowerRampRate)) {
            // Max AC ramp rate is uint32_t W/s, not a boolean setting.
            continue;
        }
        else if (pds_info._pds_buffer.at(idx).data != 0
                 && pds_info._pds_buffer.at(idx).data != 1) {
            data_valid = false;
            break;
        }
    }

    if (!data_valid) {
        for (auto i = static_cast<uint32_t>(Key::PdsSoCPowerSmoothEnabled);
             i <= static_cast<uint32_t>(Key::PdsAdcCalibrationData25);
             i++) {
            const uint32_t idx                = pds_index(static_cast<Key>(i));
            pds_info._pds_buffer.at(idx).data = 0;
        }
    }

    return Status::Ok;
}
