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
#include "nv/spdm/spdm_get_measurement.h"

#include <optional>
#include <ranges>

#include "nv/debugtoken/debugtoken.h"
#include "nv/flash/flash.h"
#include "nv/spdm/task.h"
#include "nv/fw_parser/fw_parser_mcu.h"
#include "sys/spdm/platform_measurement.h"
#include "nv/spdm/spdm_ap_measurement.h"
using namespace nv;

// for importing the funciton of ada pldm library
#ifdef __cplusplus
extern "C" {
#endif
// this function will return 76 bytes(including pldm header)
extern uint32_t ada_get_pldm_device_identity(void* data);
#ifdef __cplusplus
}
#endif

namespace nv::spdm::measurement {

namespace debugtoken {

alignas(8) NV_SHARED_BSS nv::debugtoken::DebugTokenTlvConfig DTconfig = {};     // NOLINT
alignas(8) NV_SHARED_BSS bool dbg_token_nonce_meas_valid              = false;  // NOLINT

}  // namespace debugtoken

namespace {  // declare some helper function for read from flash and calculate hash

// Key revocation bitmask constants
constexpr uint32_t kDebugKeyMask = 0x1;   // Bit 0: key0 is debug key
constexpr uint32_t kProdKeysMask = 0xFE;  // Bits 1-7: key1-7 are production keys

// this function help to check the value will not wrap after add.
template<typename InputT>
std::optional<InputT> check_wrap(InputT input_value, InputT add_value)
{
    if (input_value > std::numeric_limits<InputT>::max() - add_value) {
        return std::nullopt;
    }
    return input_value + add_value;
}

// this function will read the length as the same as read_span size
static std::optional<uint32_t> read_from_flash(nv::flash::Address        read_address,
                                               const std::span<uint8_t>& read_span)
{
    const uint32_t NeedToReadSize = read_span.size();

    if (!check_wrap(read_address, NeedToReadSize)) {  // if address will wrap, return nullopt
        return std::nullopt;
    }
    uint32_t complete_size = 0;
    while (complete_size < NeedToReadSize) {
        const uint32_t ReadSize = complete_size + nv::flash::BufferSize <= NeedToReadSize
                                    ? nv::flash::BufferSize
                                    : NeedToReadSize - complete_size;
        const auto     Chunk    = read_span.subspan(complete_size, ReadSize);
        if (nv::flash::Status::Ok != nv::flash::Flash::read(read_address, Chunk)) {
            // check the flash read success
            return std::nullopt;
        }
        if (!check_wrap(read_address, ReadSize)) {  // if address will wrap, return nullopt
            return std::nullopt;
        }
        read_address  += ReadSize;
        complete_size += ReadSize;
    }
    return complete_size;
}

static bool get_sha384_from_flash(uint32_t start_addr,
                                  uint32_t length,
                                  std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash)
{
    mbedtls_sha512_context ctx;
    constexpr int32_t      UsingSha384 = 1;
    if (!check_wrap(start_addr, length)) {  // if addr wrap, clear the hash value and terminate
                                            // function.
        hash.fill(0);
        return false;
    }

    // check get the function have the mutex
    if (0 != mbedtls_sha512_starts_ret(&ctx, UsingSha384)) {
        hash.fill(0);
        return false;
    }

    for (uint32_t need_to_read_size = length, addr_location = start_addr;
         need_to_read_size != 0;) {
        std::array<uint8_t, nv::flash::BufferSize> read_chunk{};
        const auto ChunkSize = need_to_read_size >= nv::flash::BufferSize
                                 ? nv::flash::BufferSize
                                 : need_to_read_size;

        const std::span<uint8_t> ReadChunkView = std::span<uint8_t>(read_chunk)
                                                     .subspan(0, ChunkSize);

        if (!read_from_flash(addr_location, ReadChunkView)) {
            hash.fill(0);
            return false;
        }
        if (0 != mbedtls_sha512_update_ret(&ctx, read_chunk.data(), ChunkSize)) {
            hash.fill(0);
            return false;
        }
        need_to_read_size -= ChunkSize;
        addr_location     += ChunkSize;
    }

    if (0 != mbedtls_sha512_finish_ret(&ctx, hash.data())) {
        hash.fill(0);
        return false;
    }
    return true;
}
// this function will turn the input into byte view
template<typename InputT>
std::span<uint8_t> get_view_in_byte(InputT& input_data)
{
    static_assert(sizeof(uint8_t) != 0);
    static_assert(sizeof(InputT) / sizeof(uint8_t) != 0);
    return std::span(
        *std::bit_cast<std::array<uint8_t, sizeof(InputT) / sizeof(uint8_t)>*>(&input_data));
}

}  // namespace

bool spdm_check_meas_index_valid(MeasNumT index)
{
    if (index == 0 || index > nv::spdm::measurement::spdm_get_num_meas()) {
        return false;
    }
    return true;
}

// this function only initial the measurement that needs cache
void spdm_init_all_measurement_cache()
{
    auto& measurement_cache = nv::spdm::Task::get_measurement_cache();
    flash::Driver::get_uuid(measurement_cache.measurement_uuid);
    std::memcpy(static_cast<void*>(nv::spdm::measurement::debugtoken::DTconfig.serial.value),
                measurement_cache.measurement_uuid.data(),
                nv::debugtoken::DT_DEV_SER_NUM_SIZE);
    measurement_cache.measurement_rom_patch_cmac = sys::spdm::get_boot_rom_cmac_in_kernel();
    // Note: Serial number is now directly accessed from measurement_cache in TLV functions
};

void spdm_get_fw_header_hash(bool is_active_slot,
                             std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash)
{
    hash.fill(0x0);
    const nv::fw_parser::mcu::ParsingFwType
               fw_type = is_active_slot ? nv::fw_parser::mcu::ParsingFwType::ActiveSlot
                                        : nv::fw_parser::mcu::ParsingFwType::InactiveSlot;
    const auto ImageHeaderHashRangeCheck = nv::fw_parser::mcu::get_fw_metadata_hash_range(
        fw_type);
    if (!ImageHeaderHashRangeCheck.has_value()) {
        hash.fill(0x00);
        return;
    }
    std::array<std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>,
               std::tuple_size_v<
                   std::remove_reference_t<decltype(ImageHeaderHashRangeCheck)>::value_type>>
        temp_hash_array{};
    for (auto const& [HashRangeItem, temp_hash] :
         std::views::zip(*ImageHeaderHashRangeCheck, temp_hash_array)) {
        temp_hash.fill(0x00);

        if (!get_sha384_from_flash(
                HashRangeItem.start_address, HashRangeItem.length, temp_hash)) {
            hash.fill(0x00);
            return;
        }
    }
    if (temp_hash_array.size() > 1) {
        if (nv::spdm::crypto::CryptoStatus::Success
            != nv::spdm::crypto::spdm_hash_data(hash.data(),
                                                temp_hash_array.at(0).data(),
                                                temp_hash_array.size()
                                                    * nv::spdm::crypto::Sha384HashSize)) {
            hash.fill(0x00);
            return;
        }
    }
    else {
        hash = temp_hash_array.at(0);
    }
    return;
}

void spdm_get_fw_image_hash(bool is_active_slot,
                            std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash)
{
    hash.fill(0x00);
    const nv::fw_parser::mcu::ParsingFwType
               fw_type = is_active_slot ? nv::fw_parser::mcu::ParsingFwType::ActiveSlot
                                        : nv::fw_parser::mcu::ParsingFwType::InactiveSlot;
    const auto ImageHashRangeCheck = nv::fw_parser::mcu::get_fw_image_hash_range(fw_type);
    if (!ImageHashRangeCheck.has_value()) {
        hash.fill(0x00);
        return;
    }
    std::array<
        std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>,
        std::tuple_size_v<std::remove_reference_t<decltype(ImageHashRangeCheck)>::value_type>>
        temp_hash_array{};
    for (auto const& [HashRangeItem, temp_hash] :
         std::views::zip(*ImageHashRangeCheck, temp_hash_array)) {
        temp_hash.fill(0x00);

        if (!get_sha384_from_flash(
                HashRangeItem.start_address, HashRangeItem.length, temp_hash)) {
            hash.fill(0x00);
            return;
        }
    }

    if (temp_hash_array.size() > 1) {
        if (nv::spdm::crypto::CryptoStatus::Success
            != nv::spdm::crypto::spdm_hash_data(hash.data(),
                                                temp_hash_array.at(0).data(),
                                                temp_hash_array.size()
                                                    * nv::spdm::crypto::Sha384HashSize)) {
            hash.fill(0x00);
            return;
        }
    }
    else {
        hash = temp_hash_array.at(0);
    }
    return;
}

void spdm_get_fw_version(bool is_active_slot, MeasurementFirmwareVersionT& fw_version)
{
    const auto FwType = is_active_slot ? nv::fw_parser::mcu::ParsingFwType::ActiveSlot
                                       : nv::fw_parser::mcu::ParsingFwType::InactiveSlot;
    const auto FirmwareVersionCheck = nv::fw_parser::mcu::get_firmware_version(FwType);
    // parsing fw fail
    if (!FirmwareVersionCheck) {
        return;
    }
    fw_version.build = FirmwareVersionCheck->build;
    fw_version.major = FirmwareVersionCheck->major;
    fw_version.minor = FirmwareVersionCheck->minor;
    fw_version.patch = FirmwareVersionCheck->patch;
    return;
}

uint64_t spdm_get_fw_security_version(bool is_active_slot)
{
    const auto FwType = is_active_slot == true
                          ? nv::fw_parser::mcu::ParsingFwType::ActiveSlot
                          : nv::fw_parser::mcu::ParsingFwType::InactiveSlot;

    auto security_version_check = nv::fw_parser::mcu::get_security_version(FwType);
    if (!security_version_check.has_value()) {
        constexpr uint64_t DefaultSnvValue = std::numeric_limits<uint64_t>::max();
        return DefaultSnvValue;
    }
    // this mask should remove when next version of spdm measurement block
    return *security_version_check;
}

/*
 *  spdm_get_measurement()
 *
 *  This function get/update the fresh measurement value and save into the buffer.
 *
 *  [in] index - the measurement type according to MeasNumT
 *  [in|out] buffer - pointer to data buffer to hold measurement data
 *
 *  Returns: void
 */
void spdm_get_measurement(MeasNumT index, void* buffer)
{
    if (!spdm_check_meas_index_valid(index)) {
        return;
    }

    // all measurement should implement its own code for get measurement data.
    switch (index) {
        case MeasVersion: {
            MeasurementVersionValueT& input_version = *std::bit_cast<MeasurementVersionValueT*>(
                buffer);
            input_version = nv::spdm::measurement::MeasurementVersion;
            break;
        }
        case MeasFuses: {
            auto& hash_data = *std::bit_cast<
                std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>*>(buffer);
            auto get_meas_fuses =
                [](std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash) {
                    auto const FuseIndexsMask = sys::spdm::get_fuse_index_mask_pair();

                    std::array<uint32_t, FuseIndexsMask.size()> data_to_hash{};
                    for (auto const [Index, EfuseAddrMaskPair] :
                         std::views::enumerate(std::as_const(FuseIndexsMask))) {
                        auto const& [EfuseAddr, EfuseMask] = EfuseAddrMaskPair;  // get the fuse
                                                                                 // address and
                                                                                 // mask
                        if (Index < 0
                            || nv::flash::Status::Ok
                                   != nv::flash::Flash::read_efuse(EfuseAddr,
                                                                   data_to_hash.at(Index))) {
                            hash.fill(0);
                            return;
                        }
                        data_to_hash.at(Index) &= EfuseMask;
                    }
                    auto& data_to_hash_byte_array = *std::bit_cast<
                        std::array<uint8_t, sizeof(data_to_hash)>*>(data_to_hash.data());
                    if (nv::spdm::crypto::CryptoStatus::Success
                        != nv::spdm::crypto::spdm_hash_data(hash.data(),
                                                            data_to_hash_byte_array.data(),
                                                            data_to_hash_byte_array.size())) {
                        hash.fill(0);
                        return;
                    }
                };
            get_meas_fuses(hash_data);
        } break;
        case MeasRollbackFuses: {
            std::array<uint8_t, 4>&
                rollback_fuse_data = *std::bit_cast<std::array<uint8_t, 4>*>(buffer);
            const std::span<uint8_t> RollbackFuseSpan(rollback_fuse_data);
            constexpr uint32_t       SecureFirmwareVersionCfpaOffset = 0x08;
            nv::flash::Flash::read_cfpa(RollbackFuseSpan, SecureFirmwareVersionCfpaOffset);
        } break;
        case MeasKeyRevocationFuses: {
            std::array<uint8_t, 4>&
                image_key_revoke_data = *std::bit_cast<std::array<uint8_t, 4>*>(buffer);
            const std::span<uint8_t> ImageKeyRevokeSpan(image_key_revoke_data);
            constexpr uint32_t       ImageKeyRevokeCfpaOffset = 0x18;
            // read from cfpa
            nv::flash::Flash::read_cfpa(ImageKeyRevokeSpan, ImageKeyRevokeCfpaOffset);
            // read from otp
            {
                uint32_t           otp_data                   = 0;
                constexpr uint32_t ImageKeyRovocationOptIndex = 2;
                constexpr uint32_t ImageKeyRovocationOptMask  = 0x0000ff00u;
                if (nv::flash::Flash::read_efuse(ImageKeyRovocationOptIndex, otp_data)
                    == nv::flash::Status::Ok) {
                    image_key_revoke_data.at(3) = ((otp_data & ImageKeyRovocationOptMask)
                                                   >> 8u);
                }
                else {
                    nv::info("ImageKeyRovocationOptIndex read fail\n");
                }
            }
        } break;
        case MeasMcuActiveFirmwareSecurityVersion: {
            auto& svn = *std::bit_cast<uint64_t*>(buffer);
            svn       = spdm_get_fw_security_version(true);
        } break;
        case MeasMcuInctiveFirmwareSecurityVersion: {
            auto& svn = *std::bit_cast<uint64_t*>(buffer);
            svn       = spdm_get_fw_security_version(false);
        } break;
        case MeasMcxnUuid: {
            auto& measurement_cache = nv::spdm::Task::get_measurement_cache();
            auto& input_uuid = *std::bit_cast<decltype(&measurement_cache.measurement_uuid)>(
                buffer);
            input_uuid = measurement_cache.measurement_uuid;
            break;
        }
        case MeasMcuActiveFirmwareHeaderHash: {
            auto& hash_data = *std::bit_cast<
                std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>*>(buffer);
            spdm_get_fw_header_hash(true, hash_data);
        } break;
        case MeasMcuInactiveFirmwareHeaderHash: {
            auto& hash_data = *std::bit_cast<
                std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>*>(buffer);
            spdm_get_fw_header_hash(false, hash_data);
        } break;
        case MeasMcuActiveFirmwareVersion: {
            auto& fw_version = *std::bit_cast<MeasurementFirmwareVersionT*>(buffer);
            spdm_get_fw_version(true, fw_version);
        } break;
        case MeasMcuInactiveFirmwareVersion: {
            auto& fw_version = *std::bit_cast<MeasurementFirmwareVersionT*>(buffer);
            spdm_get_fw_version(false, fw_version);
        } break;
        case MeasMcuBootStatus: {
            // not define yet, use hard code now.
            constexpr uint8_t SuccessBootStatusCode = 0xffu;
            auto&             boot_status           = *std::bit_cast<uint8_t*>(buffer);
            boot_status                             = SuccessBootStatusCode;
        } break;
        case MeasDebugTokenTlvConfiguration: {
            auto& debug_token_tlv_config = *std::bit_cast<nv::debugtoken::DebugTokenTlvConfig*>(
                buffer);
            get_debug_token_tlv_config(debug_token_tlv_config);
        } break;
        case MeasDebugTokenStatus: {
            auto& debug_token_status = *std::bit_cast<nv::debugtoken::DebugTokenStatsT*>(
                buffer);
            get_debug_token_status(debug_token_status);
        } break;
        case MeasDeviceIdentifiers: {
            constexpr uint32_t PldmHeaderSize = 3;
            constexpr uint32_t
                PldmIdentityRequseSizeWithoutHeader = MeasInfos.at(MeasDeviceIdentifiers)
                                                          .meas_size;
            // pldm tx size, ada will all-zero it.
            constexpr uint32_t                         MaxPldmIdentityBuffer = 2048;
            std::array<uint8_t, MaxPldmIdentityBuffer> device_identity_arr{};
            uint32_t device_identity_size = ada_get_pldm_device_identity(
                device_identity_arr.data());
            // check the size of get Identifier return ada.
            if (device_identity_size != PldmIdentityRequseSizeWithoutHeader + PldmHeaderSize) {
                nv::info("device_identity_size not match %d\n", device_identity_size);
            }
            // take the device without header
            auto& output_buffer = *std::bit_cast<
                std::array<uint8_t, PldmIdentityRequseSizeWithoutHeader>*>(buffer);
            auto copy_identity_view = device_identity_arr
                                    | std::ranges::views::drop(PldmHeaderSize)
                                    | std::ranges::views::take(
                                          PldmIdentityRequseSizeWithoutHeader);
            std::copy(
                copy_identity_view.begin(), copy_identity_view.end(), output_buffer.begin());

        } break;
        case MeasApFirmwareHash: {
            auto& hash_data = *std::bit_cast<
                std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>*>(buffer);
            nv::spdm::ap_measurement::get_ap_firmware_hash(hash_data);
        } break;
        case MeasApRollbackFuses: {
            auto& ap_rollback_fuse_value = *std::bit_cast<uint32_t*>(buffer);
            nv::spdm::ap_measurement::get_ap_rollback_fuses(ap_rollback_fuse_value);
        } break;
        case MeasApKeyRevocationFuses: {
            auto& key_revocation_fuse_value = *std::bit_cast<uint32_t*>(buffer);
            nv::spdm::ap_measurement::get_ap_key_revocation_fuses(key_revocation_fuse_value);
        } break;
        case MeasApMetadataHash: {
            auto& hash_data = *std::bit_cast<
                std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>*>(buffer);
            nv::spdm::ap_measurement::get_ap_metadata_hash(hash_data);
        } break;
        case MeasApFirmwareSecurityVersion: {
            auto& security_version = *std::bit_cast<uint64_t*>(buffer);
            nv::spdm::ap_measurement::get_ap_firmware_security_version(security_version);
        } break;
        case MeasApFirmwareVersion: {
            auto& firmware_version = *std::bit_cast<nv::fw_parser::ap::ApFwVersion*>(buffer);
            nv::spdm::ap_measurement::get_ap_firmware_version(firmware_version);
        } break;
        case MeasApBootStatus: {
            auto& authenticated_status = *std::bit_cast<uint8_t*>(buffer);
            nv::spdm::ap_measurement::get_ap_authenticated_status(authenticated_status);
        } break;

        case MeasApType: {
            auto& ap_type = *std::bit_cast<std::array<uint8_t, 4>*>(buffer);
            nv::spdm::ap_measurement::get_ap_type(ap_type);
            break;
        }
        case MeasRomPatchCmac: {
            auto& measurement_rom_patch_cmac = *std::bit_cast<std::array<uint8_t, 16>*>(buffer);
            measurement_rom_patch_cmac.fill(0);
            // TODO: remove this after testing
            // measurement_rom_patch_cmac       = nv::spdm::Task::get_measurement_cache()
            //                                  .measurement_rom_patch_cmac;
            break;
        }
        case MeasReservedIndex20:
        case MeasReservedIndex22:
        case MeasReservedIndex23:
        case MeasReservedIndex24:
        case MeasReservedIndex25:
        case MeasReservedIndex34:
        case MeasReservedIndex35:
        case MeasReservedIndex36:
        case MeasReservedIndex37:
        case MeasReservedIndex40:
        case MeasReservedIndex42:
        case MeasReservedIndex45: {
            using Reserved1b                     = std::array<uint8_t, 1>;
            constexpr uint8_t RawBitDefaultValue = 0xff;
            Reserved1b&       input_reserved     = *std::bit_cast<Reserved1b*>(buffer);
            input_reserved.fill(RawBitDefaultValue);
            break;
        }
        case MeasReservedIndex4:
        case MeasReservedIndex5:
        case MeasReservedIndex6:
        case MeasReservedIndex7:
        case MeasReservedIndex8:
        case MeasReservedIndex10:
        case MeasReservedIndex11:
        case MeasReservedIndex12:
        case MeasReservedIndex29:
        case MeasReservedIndex30:
        case MeasReservedIndex32:
        case MeasReservedIndex33:
        case MeasReservedIndex46:
        case MeasReservedIndex47:
        case MeasReservedIndex48:
        case MeasReservedIndex49: {
            using Reserved48b = std::array<uint8_t,
                                           nv::spdm::crypto::HashSizeT::Sha384HashSize>;

            constexpr uint8_t DigestDefaultValue = 0x00;
            Reserved48b&      input_reserved     = *std::bit_cast<Reserved48b*>(buffer);

            input_reserved.fill(DigestDefaultValue);
            break;
        }
        default: break;
    }
}

const MeasInfoT& spdm_get_meas_record_info(MeasNumT index)
{
    if (!spdm_check_meas_index_valid(index)) {
        return MeasInfos.at(0);
    }
    return MeasInfos.at(index);
};

DtStatusT verify_dbg_token_fields(
    const std::array<uint8_t, nv::debugtoken::DT_MCU_FW_VER_SIZE>&  mcu_fw_ver,
    const std::array<uint8_t, nv::debugtoken::DT_DEV_SER_NUM_SIZE>& dev_ser_num,
    const std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE>&       nonce,
    MeasNumT                                                        meas_num,
    uint32_t                                                        token_ver,
    uint16_t                                                        agent_ver)
{
    // validate parameters
    if (mcu_fw_ver.size() != nv::debugtoken::DT_MCU_FW_VER_SIZE) {
        return DtStatusMcuFwVerSizeMismatch;
    }
    if (dev_ser_num.size() != nv::debugtoken::DT_DEV_SER_NUM_SIZE) {
        return DtStatusDevSerNumSizeMismatch;
    }
    if (nonce.size() != nv::debugtoken::DT_NONCE_SIZE) {
        return DtStatusNonceSizeMismatch;
    }

    // Only support TLV format now
    if (meas_num != MeasDebugTokenTlvConfiguration) {
        return DtStatusFailBadTokenVer;
    }

    // TLV format verification
    using namespace nv::debugtoken;

    // Verify agent version directly (avoid spdm_get_measurement call)
    if (token_ver == nv::debugtoken::TLV_AGENT_CHECK_TOKEN_VER) {
        // Verify agent version against expected constant
        if (agent_ver != DT_AGENT_VER) {
            return DtStatusFailBadAgentVer;
        }
    }

    // Verify device serial number using shared DTconfig
    if (memcmp(
            dev_ser_num.data(),
            static_cast<const void*>(nv::spdm::measurement::debugtoken::DTconfig.serial.value),
            DT_DEV_SER_NUM_SIZE)
        != 0) {
        return DtStatusFailBadDevSerNum;
    }

    // Debug token has nonce in PDS, need to handle differently
    if (!debugtoken::dbg_token_nonce_meas_valid) {
        update_dbg_token_nonce_meas();
    }

    // Verify nonce using shared DTconfig
    if (memcmp(
            nonce.data(),
            static_cast<const void*>(nv::spdm::measurement::debugtoken::DTconfig.nonce.value),
            DT_NONCE_SIZE)
        != 0) {
        return DtStatusFailBadNonce;
    }

    // Mark meas record as invalid and update PDS
    debugtoken::dbg_token_nonce_meas_valid = false;
    if (!set_pds_dbg_token_nonce_valid(DBG_TOKEN_NONCE_INVALID)) {
        return DtStatusFailNonceValidityUpdate;
    }

    return DtStatusSuccess;
}

/*
 *  is_pds_dbg_token_valid()
 *
 *  This function checks if the debug token nonce in the PDS is valid.
 *  The flag used is to differentiate from an erased PDS field.
 *
 *  Returns: true if the nonce is valid
 */
bool is_pds_dbg_token_nonce_valid()
{
    bool     isValid = false;
    uint32_t data    = 0;

    // read the flag from the pds, treat get PDS error case as invalid
    if (flash::Flash::get_data(nv::flash::Key::PdsDbgTokenNonceValid, data)
        == flash::Status::Ok) {
        isValid = (data == nv::debugtoken::DBG_TOKEN_NONCE_VALID);
    }

    return isValid;
}

/*
 *  update_dbg_token_nonce_meas()
 *
 *  This helper function updates the debug token nonce in the static memory
 *  measurement record from the PDS.  If the nonce in the PDS is invalid,
 *  a new one will be generated.
 *
 *  Returns: void
 */
void update_dbg_token_nonce_meas()
{
    // check if pds nonce is valid
    bool pds_valid                                           = false;
    bool success                                             = false;
    bool nonce_status                                        = true;
    pds_valid                                                = is_pds_dbg_token_nonce_valid();
    std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE> nonce = {};

    if (pds_valid) {
        success = get_pds_dbg_token_nonce(nonce);
    }
    // get a new nonce if pds is not valid or failed getting nonce from pds
    if (!pds_valid || !success) {
        // get a new nonce
        spdm_get_and_save_dbg_token_nonce(MeasDebugTokenTlvConfiguration, nonce_status);
    }

    // mark the nonce in static memory measurement record as valid or invalid
    debugtoken::dbg_token_nonce_meas_valid = nonce_status;

    // Update DTconfig with the current nonce if we successfully got it
    if (success && pds_valid) {
        std::copy(nonce.begin(),
                  nonce.end(),
                  static_cast<uint8_t*>(debugtoken::DTconfig.nonce.value));
    }

    return;
}

/*
 *  set_pds_dbg_token_nonce_valid()
 *
 *  This function sets the flag in PDS indicating whether or not the debug
 *  token nonce in PDS is valid.
 *
 *  [in] valid - flag indicating whether or not the debug token nonce is valid
 *
 *  Returns: true if no error
 */
bool set_pds_dbg_token_nonce_valid(uint32_t valid)
{
    auto pds_status = flash::Flash::set_data(nv::flash::Key::PdsDbgTokenNonceValid, valid);

    return (pds_status == flash::Status::Ok);
}

/*
 *  set_pds_dbg_token_nonce()
 *
 *  This function saves the passed in nonce into the PDS.
 *  Should only be called after new nonce is generated by the crypto engine
 *  and the mutex has been locked to make the PDS save atomic.
 *
 *  [in] nonce
 *
 *  Returns: true if no error
 */
bool set_pds_dbg_token_nonce(const std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE>& nonce)
{
    constexpr size_t NONCE_OFFSET_1 = 4;
    constexpr size_t NONCE_OFFSET_2 = 8;
    constexpr size_t NONCE_OFFSET_3 = 12;

    flash::Data nonce_data1 = 0;
    flash::Data nonce_data2 = 0;
    flash::Data nonce_data3 = 0;
    flash::Data nonce_data4 = 0;

    std::memcpy(&nonce_data1, nonce.data(), 4);
    std::memcpy(&nonce_data2, nonce.data() + NONCE_OFFSET_1, 4);
    std::memcpy(&nonce_data3, nonce.data() + NONCE_OFFSET_2, 4);
    std::memcpy(&nonce_data4, nonce.data() + NONCE_OFFSET_3, 4);

    // Update the PDS with the four parts
    flash::Status pds_status = flash::Flash::set_data(nv::flash::Key::PdsDbgTokenNonce1,
                                                      nonce_data1);
    if (pds_status != nv::flash::Status::Ok) {
        return false;
    }

    pds_status = flash::Flash::set_data(nv::flash::Key::PdsDbgTokenNonce2, nonce_data2);
    if (pds_status != nv::flash::Status::Ok) {
        return false;
    }

    pds_status = flash::Flash::set_data(nv::flash::Key::PdsDbgTokenNonce3, nonce_data3);
    if (pds_status != nv::flash::Status::Ok) {
        return false;
    }

    pds_status = flash::Flash::set_data(nv::flash::Key::PdsDbgTokenNonce4, nonce_data4);
    if (pds_status != nv::flash::Status::Ok) {
        return false;
    }

    return true;
}

/*
 *  spdm_get_and_save_dbg_token_nonce()
 *
 *  This function gets a random number from the crypto engine and populates
 *  the nonce field for the debug token and saves the nonce into the PDS.
 *
 *  [in]  meas_num       - should be MeasDebugTokenConfiguration
 *  [out] nonce_status   - true if nonce programming succeeds, false otherwise
 *
 *  Returns: void
 */
void spdm_get_and_save_dbg_token_nonce(MeasNumT meas_num, bool& nonce_status)
{
    std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE> nonce = {};
    nonce_status                                             = false;

    // validate parameters
    if (meas_num != MeasDebugTokenTlvConfiguration) {
        return;
    }

    // get the nonce in the local buffer first, that way we avoid having
    // to lock two mutex semaphores, crypto and spdm, at the same time.
    // Locking two at a time should not be a problem but it is easier to
    // avoid a deadlock if we only lock one mutex at a time
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);

    auto ret = mbedtls_ctr_drbg_random(&ctr_drbg, nonce.data(), nv::debugtoken::DT_NONCE_SIZE);
    if (ret != 0) {
        info("mbedtls_ctr_drbg_random failed\n");
        return;
    }

    // now copy the nonce into PDS
    bool success = false;
    success      = set_pds_dbg_token_nonce(nonce);
    if (success) {
        // mark the pds debug token nonce as valid
        if (!set_pds_dbg_token_nonce_valid(nv::debugtoken::DBG_TOKEN_NONCE_VALID)) {
            info("SetDbgTokenNonceError\n");
        }
        else {
            // nonce is programmed successfully along with the validity flag.
            nonce_status = true;
        }
    }

    return;
}

/*
 *  gen_and_save_dbg_token_nonce()
 *
 *  This function generates a new debug token nonce and saves it in the
 *  static measurement memory and in the PDS.
 *
 *  Returns: True if nonce programing succeeds. False otherwise
 */
bool gen_and_save_dbg_token_nonce()
{
    // call the internal function to generate nonce and save it
    bool status = false;
    spdm_get_and_save_dbg_token_nonce(MeasDebugTokenTlvConfiguration, status);

    return status;
}

/*
 * get_pds_dbg_token_nonce()
 *
 * This function retrieves the debug token nonce saved in the PDS
 *
 * Parameters:
 *   - nonce: Reference to a std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE> that will store
 * the nonce.
 *
 * Returns:
 *   - true:  If all nonce parts are successfully retrieved and assembled.
 *   - false: If any part retrieval fails or if the nonce is all zeros.
 */
bool get_pds_dbg_token_nonce(std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE>& nonce)
{
    flash::Data nonce_data1 = 0;
    flash::Data nonce_data2 = 0;
    flash::Data nonce_data3 = 0;
    flash::Data nonce_data4 = 0;

    flash::Status pds_status = flash::Flash::get_data(nv::flash::Key::PdsDbgTokenNonce1,
                                                      nonce_data1);
    if (pds_status != flash::Status::Ok) {
        return false;
    }

    pds_status = flash::Flash::get_data(nv::flash::Key::PdsDbgTokenNonce2, nonce_data2);
    if (pds_status != flash::Status::Ok) {
        return false;
    }

    pds_status = flash::Flash::get_data(nv::flash::Key::PdsDbgTokenNonce3, nonce_data3);
    if (pds_status != flash::Status::Ok) {
        return false;
    }

    pds_status = flash::Flash::get_data(nv::flash::Key::PdsDbgTokenNonce4, nonce_data4);
    if (pds_status != flash::Status::Ok) {
        return false;
    }

    if (nonce_data1 == 0 && nonce_data2 == 0 && nonce_data3 == 0 && nonce_data4 == 0) {
        info("Nonce data is all zero");
        return false;
    }

    std::memcpy(&nonce[0], &nonce_data1, sizeof(flash::Data));
    std::memcpy(&nonce[sizeof(flash::Data)], &nonce_data2, sizeof(flash::Data));
    std::memcpy(&nonce[2 * sizeof(flash::Data)], &nonce_data3, sizeof(flash::Data));
    std::memcpy(&nonce[3 * sizeof(flash::Data)], &nonce_data4, sizeof(flash::Data));

    return true;
}

// Function declarations
void get_debug_token_tlv_config(nv::debugtoken::DebugTokenTlvConfig& config);
nv::debugtoken::LifecycleState get_device_lifecycle_state();

// Get device lifecycle state value based on key revocation status
nv::debugtoken::LifecycleState get_device_lifecycle_state()
{
    using nv::debugtoken::LifecycleState;

    // Read key revocation status from OTP/CFPA
    uint32_t key_revoke = 0;
    auto     status     = nv::flash::Flash::read_key_revoke(key_revoke,
                                                    nv::flash::KeyRollbackSelect::Mcu);

    if (status == nv::flash::Status::Ok) {
        // Check revoke status similar to glacier project logic
        const bool debug_key_revoked = (key_revoke & kDebugKeyMask) != 0;  // key0 is debug key
        const bool any_prod_key_revoked = (key_revoke & kProdKeysMask) != 0;  // key1-7 are prod
                                                                              // keys

        if (!debug_key_revoked && !any_prod_key_revoked) {
            // No keys revoked - initial/manufacturing state
            return LifecycleState::Manufacturing;
        }
        else if (debug_key_revoked) {
            return LifecycleState::Production;
        }
        else {
            // Some prod keys revoked but debug key not revoked - debug state
            return LifecycleState::Debug;
        }
    }
    else {
        // If we can't read OTP, default to initial state
        return LifecycleState::Manufacturing;
    }
}

// this should be modify after debug functionality done
void get_debug_token_status(nv::debugtoken::DebugTokenStatsT& status)
{
    // TODO:: fill the real data
    status.currently_installed = 0;
    return;
}

void get_debug_token_tlv_config(nv::debugtoken::DebugTokenTlvConfig& config)
{
    using namespace nv::debugtoken;

    // Initialize TLV header
    config.header.identifier = TlvMagicNumber;
    config.header.version    = TlvVersion;
    config.header.size       = TlvTokenRequestSize;
    std::fill(std::begin(config.header.reserved), std::end(config.header.reserved), 0);

    // Initialize device type TLV with default values
    config.device_type = TlvDeviceType{};  // Uses default constructor values

    // Initialize nonce TLV with default values, then fill nonce data
    config.nonce = TlvNonce{};  // Uses default constructor values
    std::array<uint8_t, DT_NONCE_SIZE> nonce_data = {};
    if (get_pds_dbg_token_nonce(nonce_data)) {
        std::copy(
            nonce_data.begin(), nonce_data.end(), static_cast<uint8_t*>(config.nonce.value));
    }
    else {
        std::fill(std::begin(config.nonce.value), std::end(config.nonce.value), 0);
    }

    // Initialize device serial number TLV with default values, then update value
    config.serial           = TlvDeviceSerialNumber{};  // Uses default constructor values
    auto& measurement_cache = nv::spdm::Task::get_measurement_cache();
    std::copy(measurement_cache.measurement_uuid.begin(),
              measurement_cache.measurement_uuid.begin()
                  + std::min(measurement_cache.measurement_uuid.size(),
                             static_cast<size_t>(DT_DEV_SER_NUM_SIZE)),
              static_cast<uint8_t*>(config.serial.value));

    // Initialize firmware version TLV with default values, then update value
    config.firmware_version = TlvFirmwareVersion{};  // Uses default constructor values
    constexpr uint8_t           ByteMask = 0xffu;
    MeasurementFirmwareVersionT mcu_fw_ver{0, 0, 0, 0};
    spdm_get_fw_version(true, mcu_fw_ver);
    // little endian
    config.firmware_version.value[0] = static_cast<uint8_t>(mcu_fw_ver.build & ByteMask);
    config.firmware_version.value[1] = static_cast<uint8_t>(mcu_fw_ver.patch & ByteMask);
    config.firmware_version.value[2] = static_cast<uint8_t>(mcu_fw_ver.minor & ByteMask);
    config.firmware_version.value[3] = static_cast<uint8_t>(mcu_fw_ver.major & ByteMask);

    // Initialize agent version TLV with default values, then update value
    config.agent_version       = TlvAgentVersion{};  // Uses default constructor values
    config.agent_version.value = DT_AGENT_VER;

    // Initialize lifecycle state TLV with default values, then update value
    config.lifecycle_state       = TlvLifecycleState{};  // Uses default constructor values
    config.lifecycle_state.value = static_cast<uint8_t>(get_device_lifecycle_state());

    return;
}

}  // namespace nv::spdm::measurement