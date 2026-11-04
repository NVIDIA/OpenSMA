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

#include <bit>
#include <limits>
#include <optional>

#include "nv/flash/flash.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/vrot/interface/interface.h"
#include NV_IPC_CONFIG_H  // for nv::vrot::ApList

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

namespace {

std::optional<nv::fw_parser::ap::ApFwMetadata::TbsData>
read_authenticated_active_metadata(nv::vrot::ApId ap_id)
{
    const auto                        key = ap_id == nv::vrot::ApId::AP0
                                              ? nv::flash::Key::NpdsActiveApFwAuthenticateData
                                          : ap_id == nv::vrot::ApId::AP1
                                              ? nv::flash::Key::NpdsActiveAp1FwAuthenticateData
                                              : nv::flash::Key::NpdsInvalid;
    nv::secure_boot::AuthenticateData authenticate_data{};
    if (nv::flash::Flash::get_ap_fw_authenticate_data(authenticate_data, key)
            != nv::flash::Status::Ok
        || authenticate_data.ap_auth_result != nv::spdm::crypto::CryptoStatus::Success) {
        return std::nullopt;
    }
    return authenticate_data.ap_metadata_tbs_data;
}

}  // namespace

void get_ap_type(std::array<uint8_t, 4>& ap_type)
{
    ap_type = NonApType;

    for (const auto& ap : nv::vrot::ApList) {
        ap_type = ap_type_string(ap.type);
        break;
    }
}

void get_ap_firmware_hash(nv::vrot::ApId                                         ap_id,
                          std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash)
{
    hash.fill(0);
    const auto metadata = read_authenticated_active_metadata(ap_id);
    if (!metadata || metadata->ap_fw_images_count == 0U
        || metadata->ap_fw_images_count > metadata->hash_table.size()) {
        return;
    }

    if (metadata->ap_fw_images_count == 1U) {
        hash = metadata->hash_table.front().hash;
        return;
    }

    nv::spdm::crypto::Sha384Context hash_ctx{};
    if (!hash_ctx.init()) {
        return;
    }

    for (uint8_t index = 0; index < metadata->ap_fw_images_count; ++index) {
        const auto& entry = metadata->hash_table.at(index);
        if (!hash_ctx.update(entry.hash.data(), entry.hash.size())) {
            return;
        }
    }

    if (!hash_ctx.finish(hash.data())) {
        hash.fill(0);
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

void get_ap_firmware_security_version(nv::vrot::ApId ap_id, uint64_t& security_version)
{
    security_version    = std::numeric_limits<uint64_t>::max();
    const auto metadata = read_authenticated_active_metadata(ap_id);
    if (metadata) {
        security_version = metadata->sec_version;
    }
}

void get_ap_metadata_hash(nv::vrot::ApId                                         ap_id,
                          std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash)
{
    hash.fill(0);
    const auto metadata = read_authenticated_active_metadata(ap_id);
    if (!metadata) {
        return;
    }

    nv::spdm::crypto::Sha384Context hash_ctx{};
    if (!hash_ctx.init()
        || !hash_ctx.update(std::bit_cast<const uint8_t*>(&(*metadata)), sizeof(*metadata))
        || !hash_ctx.finish(hash.data())) {
        hash.fill(0);
    }
}

void get_ap_firmware_version(nv::vrot::ApId                  ap_id,
                             nv::fw_parser::ap::ApFwVersion& firmware_version)
{
    firmware_version    = {};
    const auto metadata = read_authenticated_active_metadata(ap_id);
    if (metadata) {
        firmware_version = metadata->fw_version;
    }
}

void get_ap_boot_status(nv::vrot::ApId ap_id, uint8_t& boot_status)
{
    boot_status = static_cast<uint8_t>(nv::fw_parser::ap::ApFwStatus::Not_Supported);
    if (!nv::vrot::find_ap(ap_id, nv::vrot::ApList)) {
        return;
    }

    const auto key = ap_id == nv::vrot::ApId::AP0 ? nv::flash::Key::NpdsAp0FwStatus
                   : ap_id == nv::vrot::ApId::AP1 ? nv::flash::Key::NpdsAp1FwStatus
                                                  : nv::flash::Key::NpdsInvalid;
    if (key == nv::flash::Key::NpdsInvalid) {
        return;
    }

    nv::flash::Data data{};
    if (nv::flash::Flash::get_data(key, data) == nv::flash::Status::Ok) {
        constexpr uint32_t ApFwStatusMask = 0xff;
        boot_status = static_cast<uint8_t>(static_cast<uint32_t>(data) & ApFwStatusMask);
    }
}

void get_ap_chip_id(nv::vrot::ApId ap_id, nv::vrot::ApMeasurementChipId& chip_id)
{
    chip_id.fill(std::numeric_limits<uint8_t>::max());
    if (const auto ap = nv::vrot::find_ap(ap_id, nv::vrot::ApList);
        ap
        && nv::vrot::measurement::get_chip_id(*ap, chip_id) != nv::vrot::ApOpErrCode::Success) {
        chip_id.fill(std::numeric_limits<uint8_t>::max());
    }
}

void get_ap_fuse_mask(nv::vrot::ApId ap_id, uint32_t& fuse_mask)
{
    fuse_mask = std::numeric_limits<uint32_t>::max();
    if (const auto ap = nv::vrot::find_ap(ap_id, nv::vrot::ApList);
        ap
        && nv::vrot::measurement::get_fuse_mask(*ap, fuse_mask)
               != nv::vrot::ApOpErrCode::Success) {
        fuse_mask = std::numeric_limits<uint32_t>::max();
    }
}

}  // namespace nv::spdm::ap_measurement
