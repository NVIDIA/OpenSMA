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
#include "nv/spdm/spdm_ap_measurement.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/flash/flash.h"
#include "nv/vrot/interface/types.h"
#include NV_IPC_CONFIG_H  // for nv::vrot::ApList
#include <bit>

namespace {
constexpr std::array<uint8_t, 4> NonApType = {'N', 'O', 'N', 0x00};
constexpr std::array<uint8_t, 4> CpdApType = {'C', 'P', 'D', 0x00};
constexpr std::array<uint8_t, 4> LpuApType = {'L', 'P', 'U', 0x00};

constexpr std::array<uint8_t, 4> ap_type_string(nv::vrot::ApType type)
{
    switch (type) {
        case nv::vrot::ApType::Cpld: return CpdApType;
        case nv::vrot::ApType::Lpu : return LpuApType;
        default                    : return NonApType;
    }
}
}  // namespace

namespace nv::spdm::ap_measurement {
static_assert(nv::vrot::ApList.size() <= 1,
              "SPDM AP measurements currently support at most one AP in ApList");

void get_ap_type(std::array<uint8_t, 4>& ap_type)
{
    ap_type = NonApType;

    for (const auto& ap : nv::vrot::ApList) {
        ap_type = ap_type_string(ap.type);
        break;
    }
}

void get_ap_firmware_hash(std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash)
{
    hash.fill(0);
    auto fw_images_count = nv::fw_parser::ap::get_ap_fw_images_count(
        nv::fw_parser::ap::ParsingApFwType::UpdateSlot);

    nv::spdm::crypto::Sha384Context hash_ctx{};
    if (!hash_ctx.init() || !fw_images_count) {
        return;
    }

    // if there is only one image, just read the hash from the flash
    if (*fw_images_count == 1) {
        auto hash_table_entry = nv::fw_parser::ap::get_ap_hash_table_entry(
            nv::fw_parser::ap::ParsingApFwType::UpdateSlot, 0);
        if (!hash_table_entry) {
            return;
        }
        hash = hash_table_entry->hash;
        return;
    }

    // if there are multiple images, read the hash from the flash
    for (int i = 0; i < *fw_images_count; i++) {
        auto hash_table_entry = nv::fw_parser::ap::get_ap_hash_table_entry(
            nv::fw_parser::ap::ParsingApFwType::UpdateSlot, i);
        if (!hash_table_entry
            || !hash_ctx.update(hash_table_entry->hash.data(), hash_table_entry->hash.size())) {
            return;
        }
    }
    if (!hash_ctx.finish(hash.data())) {
        return;
    }
}

void get_ap_rollback_fuses(uint32_t& rollback_fuse_value)
{
    if (nv::flash::Flash::read_secure_fw_version(rollback_fuse_value,
                                                 nv::flash::KeyRollbackSelect::Ap0)
        != nv::flash::Status::Ok) {
        // set the default value to the max value
        rollback_fuse_value = std::numeric_limits<
            std::remove_reference_t<decltype(rollback_fuse_value)>>::max();
    }
}

void get_ap_key_revocation_fuses(uint32_t& key_revocation_fuse_value)
{
    if (nv::flash::Flash::read_key_revoke(key_revocation_fuse_value,
                                          nv::flash::KeyRollbackSelect::Ap0)
        != nv::flash::Status::Ok) {
        // set the default value to the max value
        key_revocation_fuse_value = std::numeric_limits<
            std::remove_reference_t<decltype(key_revocation_fuse_value)>>::max();
    }
}

void get_ap_firmware_security_version(uint64_t& security_version)
{
    // set the default value to the max value
    security_version = std::numeric_limits<
        std::remove_reference_t<decltype(security_version)>>::max();
    auto ap_fw_sec = nv::fw_parser::ap::get_ap_sec_version(
        nv::fw_parser::ap::ParsingApFwType::UpdateSlot);
    if (!ap_fw_sec) {
        return;
    }
    security_version = *ap_fw_sec;
}

void get_ap_metadata_hash(std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash)
{
    hash.fill(0);
    auto ap_metadata_data = nv::fw_parser::ap::get_ap_metadata_data(
        nv::fw_parser::ap::ParsingApFwType::UpdateSlot);
    if (!ap_metadata_data) {
        return;
    }
    nv::spdm::crypto::Sha384Context hash_ctx{};
    if (!hash_ctx.init()
        || !hash_ctx.update(std::bit_cast<uint8_t*>(&(*ap_metadata_data)),
                            sizeof(*ap_metadata_data))) {
        return;
    }
    if (!hash_ctx.finish(hash.data())) {
        return;
    }
}

void get_ap_firmware_version(nv::fw_parser::ap::ApFwVersion& firmware_version)
{
    firmware_version = nv::fw_parser::ap::ApFwVersion{};

    auto ap_fw_version = nv::fw_parser::ap::get_ap_fw_version(
        nv::fw_parser::ap::ParsingApFwType::UpdateSlot);
    if (!ap_fw_version) {
        return;
    }
    firmware_version = *ap_fw_version;
}

void get_ap_authenticated_status(uint8_t& authenticated_status)
{
    nv::flash::Data data{};
    (void)nv::flash::Flash::get_data(nv::flash::Key::NpdsAp0FwStatus, data);
    constexpr uint32_t ApFwStatusMask = 0xFF;
    authenticated_status = static_cast<uint8_t>(static_cast<uint32_t>(data) & ApFwStatusMask);
}
}  // namespace nv::spdm::ap_measurement
