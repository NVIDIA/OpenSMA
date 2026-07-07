/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/vrot/platform/lpu.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include "nv/common/utils.h"
#include "nv/crypto/key_clear_guard.h"
#include "nv/crypto/key_wrap.h"
#include "nv/flash/datastore.h"
#include "nv/flash/flash.h"
#include "nv/logger/log.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/spi/ext_flash.h"
#include "nv/vrot/interface/interface.h"
#include "nv/vrot/platform/lpu_flash_layout.h"
#include NV_IPC_CONFIG_H

void claim_lpu_spi_flash();
void release_lpu_spi_flash();
void hold_lpu_reset();
void release_lpu_reset();

namespace nv::vrot {

namespace {

constexpr bool HasLpu = has_ap_type(ApType::Lpu, ApList);
static_assert(!HasLpu || LpuFlashLayout::FlashSize != 0U,
              "LPU projects must define NV_VROT_LPU_FLASH_SIZE_BYTES in project config.h");
static_assert(!HasLpu || (LpuFlashLayout::FlashSize % LpuFlashLayout::SlotCount) == 0U,
              "LPU flash size must split evenly into logical slots");
static_assert(!HasLpu || LpuFlashLayout::SlotSize > LpuFlashLayout::MetadataRegionSize,
              "LPU slot size must be larger than AP metadata");

constexpr auto    MetadataRegionSize = LpuFlashLayout::MetadataRegionSize;
constexpr auto    ImageSize          = LpuFlashLayout::ImageSize;
constexpr uint8_t Slot0              = 0;
constexpr uint8_t Slot1              = 1;

[[maybe_unused]] bool        is_uds_provisioned();
[[maybe_unused]] bool        is_cek_provisioned();
[[maybe_unused]] bool        is_ecid_provisioned();
[[maybe_unused]] ApOpErrCode get_uds_slot_num(uint8_t& slot_num);
[[maybe_unused]] ApOpErrCode provision_all();
[[maybe_unused]] ApOpErrCode provision_new_uds();
[[maybe_unused]] ApOpErrCode lock_lpu_debug_port();
[[maybe_unused]] ApOpErrCode read_lpu_debug_port_status(uint8_t& debug_port_status);
ApOpErrCode                  read_lpu_provision_status(nv::flash::Data& provisioned);
ApOpErrCode                  set_lpu_provision_status_bit(LpuProvisionState bit);
[[maybe_unused]] uint8_t     to_lpu_provision_completion_code(ApOpErrCode status);
[[maybe_unused]] bool        is_secure_mode_lock();
[[maybe_unused]] ApOpErrCode patch_encrypted_lpu_fw_header(const lpu::PointerBlock& block,
                                                           std::span<uint8_t>       data);

struct SlotPartitions
{
    uint8_t image;
    uint8_t metadata;
};

[[maybe_unused]] constexpr ApOpErrCode from_ext_flash(nv::spi::ExtFlash::Status status)
{
    using Status = nv::spi::ExtFlash::Status;
    switch (status) {
        case Status::Ok          : return ApOpErrCode::Success;
        case Status::InvalidParam: return ApOpErrCode::InvalidParam;
        case Status::Timeout     : return ApOpErrCode::Timeout;
        case Status::Busy        : return ApOpErrCode::Busy;
        case Status::IdMismatch  :
        case Status::Error       :
        default                  : return ApOpErrCode::Fail;
    }
}

[[maybe_unused]] constexpr ApOpErrCode from_lpu_layout(lpu::ErrorCode status)
{
    return status == lpu::ErrorCode::Ok ? ApOpErrCode::Success
                                        : ApOpErrCode::ImageLayoutInvalid;
}

[[maybe_unused]] constexpr bool in_region(uint32_t offset, size_t len, uint32_t region_size)
{
    return offset <= region_size && len <= static_cast<size_t>(region_size - offset);
}

constexpr bool has_dual_bank()
{
    return LpuFlashLayout::SlotCount > 1U;
}

[[maybe_unused]] bool resolve_slot_partitions(uint8_t slot, SlotPartitions& partitions)
{
    if (!has_dual_bank() || slot == ApSlotUseActive || slot == ApSlotUseUpdate) {
        slot = Slot0;
    }
    switch (slot) {
        case Slot0:
            partitions = SlotPartitions{
                .image    = LpuFlashPartitionIndex::Slot0Image,
                .metadata = LpuFlashPartitionIndex::Slot0Metadata,
            };
            return true;
        case Slot1:
            partitions = SlotPartitions{
                .image    = LpuFlashPartitionIndex::Slot1Image,
                .metadata = LpuFlashPartitionIndex::Slot1Metadata,
            };
            return true;
        default: return false;
    }
}

[[maybe_unused]] constexpr bool is_valid_uds_slot(uint8_t uds_slot)
{
    return (uds_slot & static_cast<uint8_t>(~lpu::HeaderUdsIdxMask)) == 0U;
}

}  // namespace

ApOpErrCode LpuOps::hold_reset(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        hold_lpu_reset();
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::pre_authenticate(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        claim_lpu_spi_flash();
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::post_authenticate(const ApInfo& /*ap*/,
                                      nv::spdm::crypto::CryptoStatus /*result*/)
{
    if constexpr (HasLpu) {
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::release_reset(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        release_lpu_spi_flash();
        release_lpu_reset();
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::check_booted(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::read_metadata(const ApInfo& /*ap*/,
                                  uint8_t            slot,
                                  uint32_t           metadata_offset,
                                  std::span<uint8_t> data)
{
    if constexpr (HasLpu) {
        if (!in_region(metadata_offset, data.size(), MetadataRegionSize)) {
            return ApOpErrCode::InvalidParam;
        }
        if (data.empty()) {
            return ApOpErrCode::Success;
        }
        SlotPartitions partitions{};
        if (!resolve_slot_partitions(slot, partitions)) {
            return ApOpErrCode::InvalidParam;
        }
        return from_ext_flash(
            nv::spi::ExtFlash::inst().read(partitions.metadata, metadata_offset, data));
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::read_fw_data([[maybe_unused]] const ApInfo& ap,
                                 uint8_t                        slot,
                                 uint32_t                       fw_data_offset,
                                 std::span<uint8_t>             data)
{
    if constexpr (HasLpu) {
        if (!in_region(fw_data_offset, data.size(), ImageSize)) {
            return ApOpErrCode::InvalidParam;
        }
        if (data.empty()) {
            return ApOpErrCode::Success;
        }
        SlotPartitions partitions{};
        if (!resolve_slot_partitions(slot, partitions)) {
            return ApOpErrCode::InvalidParam;
        }
        return from_ext_flash(
            nv::spi::ExtFlash::inst().read(partitions.image, fw_data_offset, data));
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::write_metadata(const ApInfo& /*ap*/,
                                   uint8_t                  slot,
                                   uint32_t                 metadata_offset,
                                   std::span<const uint8_t> data)
{
    if constexpr (HasLpu) {
        if (!in_region(metadata_offset, data.size(), MetadataRegionSize)) {
            return ApOpErrCode::InvalidParam;
        }
        if (data.empty()) {
            return ApOpErrCode::Success;
        }
        SlotPartitions partitions{};
        if (!resolve_slot_partitions(slot, partitions)) {
            return ApOpErrCode::InvalidParam;
        }
        return from_ext_flash(
            nv::spi::ExtFlash::inst().write(partitions.metadata, metadata_offset, data));
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::write_fw_data([[maybe_unused]] const ApInfo& ap,
                                  uint8_t                        slot,
                                  uint32_t                       fw_data_offset,
                                  std::span<uint8_t>             data,
                                  bool                           background_copy)
{
    if constexpr (HasLpu) {
        if (!in_region(fw_data_offset, data.size(), ImageSize)) {
            return ApOpErrCode::InvalidParam;
        }
        if (data.empty()) {
            return ApOpErrCode::Success;
        }

        SlotPartitions partitions{};
        if (!resolve_slot_partitions(slot, partitions)) {
            return ApOpErrCode::InvalidParam;
        }

        if (fw_data_offset == 0U) {
            if (background_copy) {
                // background_copy_begin() runs for every chunk; reset erase tracking once.
                nv::spi::ExtFlash::inst().clear_last_known_write();
            }
            else {
                const auto header = lpu::parse_pointer_header(data);
                if (!header.ok()) {
                    return from_lpu_layout(header.error);
                }

                if (lpu::is_encrypted(header.block.metadata.is_encrypted)) {
                    // PLDM can provide a 4 KiB span; ExtFlash::write() splits it into pages.
                    if (const auto status = patch_encrypted_lpu_fw_header(header.block, data);
                        status != ApOpErrCode::Success) {
                        return status;
                    }
                }
                else if (is_secure_mode_lock()) {
                    return ApOpErrCode::PlaintextImageNotAllowed;
                }
            }
        }

        return from_ext_flash(
            nv::spi::ExtFlash::inst().write(partitions.image, fw_data_offset, data));
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::fw_update_prepare(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        claim_lpu_spi_flash();
        nv::spi::ExtFlash::inst().clear_last_known_write();
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::background_copy_begin(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        claim_lpu_spi_flash();
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

bool LpuOps::supports_background_copy(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        return has_dual_bank();
    }
    return false;
}

ApOpErrCode LpuOps::background_copy_regions(const ApInfo&                     ap,
                                            uint8_t                           source_slot,
                                            std::span<ApBackgroundCopyRegion> regions,
                                            std::size_t&                      region_count,
                                            uint32_t&                         total_size)
{
    if constexpr (HasLpu) {
        region_count = 0;
        total_size   = 0;
        if (!has_dual_bank()) {
            return ApOpErrCode::NotSupported;
        }
        if (source_slot > Slot1 || regions.size() < ApBackgroundCopyMaxRegions
            || ap.metadata_size == 0U) {
            return ApOpErrCode::InvalidParam;
        }

        std::array<uint8_t, lpu::BlockSize> pointer_data{};
        if (const auto status = read_fw_data(ap, source_slot, 0U, pointer_data);
            status != ApOpErrCode::Success) {
            return status;
        }

        const auto pointer = lpu::parse_pointer_block(pointer_data);
        if (!pointer.ok()) {
            return from_lpu_layout(pointer.error);
        }

        const auto firmware_size = lpu::get_firmware_data_size(pointer.block);
        if (!firmware_size.ok()) {
            return from_lpu_layout(firmware_size.error);
        }

        const uint32_t sector_size  = nv::spi::ExtFlash::inst().sector_size();
        const uint32_t image_extent = nv::common::align_to(firmware_size.size, sector_size);
        if (sector_size == 0U || image_extent == std::numeric_limits<uint32_t>::max()
            || image_extent == 0U || image_extent > ap.metadata_offset) {
            return ApOpErrCode::Fail;
        }

        const uint32_t metadata_end = nv::common::add(ap.metadata_offset, ap.metadata_size);
        if (metadata_end > ap.fw_size || metadata_end > LpuFlashLayout::SlotSize) {
            return ApOpErrCode::Fail;
        }

        regions[0] = {.offset = 0, .size = image_extent};
        regions[1] = {.offset = ap.metadata_offset, .size = ap.metadata_size};

        region_count = 2;
        if (nv::common::add(regions[0].offset, regions[0].size) >= regions[1].offset) {
            const uint32_t merged_end = std::max(
                nv::common::add(regions[0].offset, regions[0].size),
                nv::common::add(regions[1].offset, regions[1].size));
            regions[0].size = merged_end - regions[0].offset;
            region_count    = 1;
        }

        for (std::size_t i = 0; i < region_count; ++i) {
            total_size = nv::common::add(total_size, regions[i].size);
        }
        if (total_size == 0U) {
            return ApOpErrCode::Fail;
        }

        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::background_copy_end(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        release_lpu_spi_flash();
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::fw_update_callback(const ApInfo& ap, nv::spdm::crypto::CryptoStatus status)
{
    if constexpr (HasLpu) {
        release_lpu_spi_flash();
        nv::info("LPU fw callback ap=%u status=%u bg=%u\n",
                 static_cast<unsigned>(ap.id),
                 static_cast<unsigned>(status),
                 static_cast<unsigned>(supports_background_copy(ap)));
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::set_debug_token_feature(const ApInfo& /*ap*/,
                                            DebugTokenFeature /*feature*/,
                                            bool /*enable*/)
{
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::ap_provision(const ApInfo& /*ap*/,
                                 uint8_t                  sub_command,
                                 std::span<const uint8_t> data,
                                 uint8_t&                 ap_completion_code)
{
    ap_completion_code = static_cast<uint8_t>(lpu::ProvisionCompletionCode::GeneralError);

    if constexpr (HasLpu) {
        const auto  command = static_cast<lpu::ProvisionSubCommand>(sub_command);
        ApOpErrCode status  = ApOpErrCode::Fail;

        switch (command) {
            case lpu::ProvisionSubCommand::ProvisionAll:
                if (data.size() != lpu::ProvisionAllDataSize) {
                    return ApOpErrCode::InvalidParam;
                }
                status = provision_all();
                break;
            case lpu::ProvisionSubCommand::ProvisionNewUds:
                if (data.size() != lpu::ProvisionNewUdsDataSize) {
                    return ApOpErrCode::InvalidParam;
                }
                status = provision_new_uds();
                break;
            case lpu::ProvisionSubCommand::LockDebugPort:
                if (data.size() != lpu::LockDebugPortDataSize) {
                    return ApOpErrCode::InvalidParam;
                }
                status = lock_lpu_debug_port();
                break;
            default: return ApOpErrCode::InvalidParam;
        }

        ap_completion_code = to_lpu_provision_completion_code(status);
        return status;
    }
    return ApOpErrCode::NotSupported;
}

bool LpuOps::is_uds_provisioned()
{
    if constexpr (HasLpu) {
        return ::nv::vrot::is_uds_provisioned();
    }
    return false;
}

bool LpuOps::is_cek_provisioned()
{
    if constexpr (HasLpu) {
        return ::nv::vrot::is_cek_provisioned();
    }
    return false;
}

bool LpuOps::is_ecid_provisioned()
{
    if constexpr (HasLpu) {
        return ::nv::vrot::is_ecid_provisioned();
    }
    return false;
}

ApOpErrCode LpuOps::set_ap_provision_status(const ApInfo& /*ap*/)
{
    if constexpr (HasLpu) {
        if (::nv::vrot::is_uds_provisioned()) {
            if (const auto status = set_lpu_provision_status_bit(
                    LpuProvisionState::UdsProvisioned);
                status != ApOpErrCode::Success) {
                return status;
            }
        }
        if (::nv::vrot::is_cek_provisioned()) {
            if (const auto status = set_lpu_provision_status_bit(
                    LpuProvisionState::CekProvisioned);
                status != ApOpErrCode::Success) {
                return status;
            }
        }
        if (::nv::vrot::is_ecid_provisioned()) {
            if (const auto status = set_lpu_provision_status_bit(
                    LpuProvisionState::EcIdProvisioned);
                status != ApOpErrCode::Success) {
                return status;
            }
        }
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::query_ap_provision_status(const ApInfo& /*ap*/,
                                              uint8_t  sub_command,
                                              uint8_t& provision_info)
{
    if constexpr (HasLpu) {
        switch (static_cast<lpu::QueryApProvisionStatusSubCommand>(sub_command)) {
            case lpu::QueryApProvisionStatusSubCommand::ProvisionState: {
                nv::flash::Data provisioned = 0U;
                if (const auto status = read_lpu_provision_status(provisioned);
                    status != ApOpErrCode::Success) {
                    return status;
                }
                provision_info = static_cast<uint8_t>(provisioned);
                return ApOpErrCode::Success;
            }
            case lpu::QueryApProvisionStatusSubCommand::UdsSlotNum: {
                const auto status = ::nv::vrot::get_uds_slot_num(provision_info);
                if (status == ApOpErrCode::FlashDataInvalid) {
                    provision_info = lpu::UdsSlotNumNotProvisioned;
                    return ApOpErrCode::Success;
                }
                return status;
            }
            case lpu::QueryApProvisionStatusSubCommand::DebugPortStatus:
                return read_lpu_debug_port_status(provision_info);
            default: return ApOpErrCode::InvalidParam;
        }
    }
    provision_info = 0U;
    return ApOpErrCode::NotSupported;
}

ApOpErrCode LpuOps::request_authentication(const ApInfo& ap, uint8_t& auth_request_id)
{
    if constexpr (HasLpu) {
        auto status = nv::spdm::crypto::authenticate_ap_firmware(
            ap,
            nv::fw_parser::ap::ParsingApFwType::UpdateSlot,
            auth_request_id,
            nv::ipc::TaskId::Begin);
        return (status == nv::spdm::crypto::CryptoStatus::Success) ? ApOpErrCode::Success
                                                                   : ApOpErrCode::Fail;
    }
    return ApOpErrCode::NotSupported;
}

uint8_t LpuOps::get_write_fail_retry(const ApInfo& /*ap*/)
{
    return 0;
}

namespace {
using nv::spdm::crypto::CryptoStatus;

constexpr size_t UdsKeySize  = 32U;
constexpr size_t CekKeySize  = 32U;
constexpr size_t EcIdKeySize = 8U;
// NIST SP800-38F KW (RFC 3394) wraps an n-byte key into n + 8 output bytes
// (the 8-byte ICV). CEK is 32 bytes, so the UDS-KW-wrapped CEK is 40 bytes.
constexpr size_t UdsWrappedCekSize = CekKeySize + 8U;

using UdsKey  = std::array<uint8_t, UdsKeySize>;
using CekKey  = std::array<uint8_t, CekKeySize>;
using EcIdKey = std::array<uint8_t, EcIdKeySize>;

constexpr std::array<uint16_t, 4> InvalidUdsPatterns = {
    0x0000U,
    0xFFFFU,
    0x5555U,
    0xAAAAU,
};

// Reserved internal-flash sector populated at manufacture time by the
// provisioning SB4 `load` command. The MCU XIP-maps internal flash at the
// canonical base, so this address is directly readable.
constexpr uintptr_t CekPlaintextFlashAddr = 0xEA000U;

namespace cfpa_customer {

constexpr size_t  UdsWrappedKeySize        = 84U;
constexpr size_t  UdsCfpaCustomerDataSize  = 96U;
constexpr size_t  UdsActiveSlotNumSize     = 1U;
constexpr size_t  CekWrappedKeySize        = 84U;
constexpr size_t  CekCfpaCustomerDataSize  = 96U;
constexpr size_t  EcIdCfpaCustomerDataSize = 16U;
constexpr uint8_t LastReservedUdsSlotNum   = 1U;
constexpr uint8_t FirstUdsSlotNum          = LastReservedUdsSlotNum + 1U;
constexpr uint8_t MaxUdsSlotNum            = 7U;
struct [[gnu::packed]] WrappedUds
{
    std::array<uint8_t, UdsWrappedKeySize> wrapped_key{};
    uint8_t                                valid{};
    uint8_t                                active_slot_num{};
    std::array<uint8_t, UdsCfpaCustomerDataSize - UdsWrappedKeySize - 1U - UdsActiveSlotNumSize>
        reserved{};
};

struct [[gnu::packed]] WrappedCek
{
    std::array<uint8_t, CekWrappedKeySize>                                wrapped_key{};
    uint8_t                                                               valid{};
    std::array<uint8_t, CekCfpaCustomerDataSize - CekWrappedKeySize - 1U> reserved{};
};

struct [[gnu::packed]] EcId
{
    std::array<uint8_t, EcIdKeySize>                                 ecid{};
    uint8_t                                                          valid{};
    std::array<uint8_t, EcIdCfpaCustomerDataSize - EcIdKeySize - 1U> reserved{};
};

struct [[gnu::packed]] CfpaCustomerData
{
    WrappedUds wrapped_uds;
    WrappedCek wrapped_cek;
    EcId       ecid;
};

constexpr uint32_t WrappedUdsOffset = static_cast<uint32_t>(
    offsetof(CfpaCustomerData, wrapped_uds));
constexpr uint32_t WrappedCekOffset = static_cast<uint32_t>(
    offsetof(CfpaCustomerData, wrapped_cek));
constexpr uint32_t EcIdOffset  = static_cast<uint32_t>(offsetof(CfpaCustomerData, ecid));
constexpr uint8_t  ValidMarker = 1U;

}  // namespace cfpa_customer

static_assert(sizeof(cfpa_customer::CfpaCustomerData) <= nv::flash::BufferSize);
static_assert((UdsKeySize % sizeof(uint16_t)) == 0U);
static_assert(sizeof(cfpa_customer::WrappedUds) == cfpa_customer::UdsCfpaCustomerDataSize);
static_assert(sizeof(cfpa_customer::WrappedCek) == cfpa_customer::CekCfpaCustomerDataSize);

// Bytewise view of a PUF-wrapped CFPA struct, for KeyClearGuard and CFPA I/O.
std::span<uint8_t> to_span(cfpa_customer::WrappedUds& wrapped_uds)
{
    return {std::bit_cast<uint8_t*>(&wrapped_uds), sizeof(wrapped_uds)};
}
std::span<uint8_t> to_span(cfpa_customer::WrappedCek& wrapped_cek)
{
    return {std::bit_cast<uint8_t*>(&wrapped_cek), sizeof(wrapped_cek)};
}
std::span<uint8_t> to_span(cfpa_customer::EcId& ecid)
{
    return {std::bit_cast<uint8_t*>(&ecid), sizeof(ecid)};
}

ApOpErrCode read_puf_wrapped_key_from_cfpa(std::span<uint8_t> dest, uint32_t offset)
{
    if (nv::flash::Flash::read_cfpa_customer(dest, offset) != nv::flash::Status::Ok) {
        return ApOpErrCode::FlashReadFailure;
    }
    return ApOpErrCode::Success;
}

bool is_invalid_uds_word(uint16_t word)
{
    return std::any_of(InvalidUdsPatterns.begin(),
                       InvalidUdsPatterns.end(),
                       [word](uint16_t pattern) { return word == pattern; });
}

bool has_invalid_uds_pattern(const UdsKey& uds_key)
{
    for (size_t offset = 0U; offset < uds_key.size(); offset += sizeof(uint16_t)) {
        const auto low_byte  = static_cast<uint16_t>(uds_key.at(offset));
        const auto high_byte = static_cast<uint16_t>(uds_key.at(offset + 1U)) << 8U;
        const auto word      = static_cast<uint16_t>(low_byte | high_byte);
        if (is_invalid_uds_word(word)) {
            return true;
        }
    }
    return false;
}

ApOpErrCode generate_valid_uds_key(UdsKey& uds_key)
{
    bool is_valid = false;
    while (!is_valid) {
        if (nv::spdm::crypto::trng_generate(std::span<uint8_t>(uds_key.data(), uds_key.size()))
            != CryptoStatus::Success) {
            return ApOpErrCode::HardwareSdkFailure;
        }
        if (!has_invalid_uds_pattern(uds_key)) {
            is_valid = true;
        }
    }
    return ApOpErrCode::Success;
}

// Read-modify-write the entire CFPA customer region: read the current
// `CfpaCustomerData`, overlay `data` at `offset` within that struct, then
// write the whole struct back at offset 0. This preserves every other slot
// across CFPA page rotations -- a partial write would leave other slots on
// the old active page and they would disappear once the new page takes
// over. Caller is responsible for setting the record's valid byte in `data`
// before calling.
ApOpErrCode program_cfpa_customer_data_to_mcu(std::span<uint8_t> data, uint32_t offset)
{
    const nv::crypto::KeyClearGuard data_guard(data);
    // clang-tidy can't see that record is mutated through record_span (a
    // uint8_t* alias). The flash read fills it, memcpy overlays at offset,
    // and KeyClearGuard zeros it — all via the span, not the named identifier.
    // NOLINTNEXTLINE(misc-const-correctness)
    cfpa_customer::CfpaCustomerData record{};
    auto record_span = std::span<uint8_t>(std::bit_cast<uint8_t*>(&record), sizeof(record));
    const nv::crypto::KeyClearGuard record_span_guard(record_span);

    if (offset > record_span.size() || data.size() > record_span.size() - offset) {
        return ApOpErrCode::InvalidParam;
    }

    if (nv::flash::Flash::read_cfpa_customer(record_span, 0) != nv::flash::Status::Ok) {
        return ApOpErrCode::FlashReadFailure;
    }

    std::memcpy(record_span.data() + offset, data.data(), data.size());

    if (nv::flash::Flash::write_cfpa_customer(record_span, 0) != nv::flash::Status::Ok) {
        return ApOpErrCode::FlashWriteFailure;
    }
    return ApOpErrCode::Success;
}

[[maybe_unused]] bool is_uds_provisioned()
{
    cfpa_customer::WrappedUds       wrapped_uds{};
    const nv::crypto::KeyClearGuard wrapped_uds_guard(to_span(wrapped_uds));
    if (read_puf_wrapped_key_from_cfpa(to_span(wrapped_uds), cfpa_customer::WrappedUdsOffset)
        != ApOpErrCode::Success) {
        return false;
    }
    return wrapped_uds.valid == cfpa_customer::ValidMarker;
}

[[maybe_unused]] bool is_cek_provisioned()
{
    cfpa_customer::WrappedCek       wrapped_cek{};
    const nv::crypto::KeyClearGuard wrapped_cek_guard(to_span(wrapped_cek));
    if (read_puf_wrapped_key_from_cfpa(to_span(wrapped_cek), cfpa_customer::WrappedCekOffset)
        != ApOpErrCode::Success) {
        return false;
    }
    return wrapped_cek.valid == cfpa_customer::ValidMarker;
}

[[maybe_unused]] bool is_ecid_provisioned()
{
    cfpa_customer::EcId             ecid{};
    const nv::crypto::KeyClearGuard ecid_guard(to_span(ecid));
    if (nv::flash::Flash::read_cfpa_customer(to_span(ecid), cfpa_customer::EcIdOffset)
        != nv::flash::Status::Ok) {
        return false;
    }
    return ecid.valid == cfpa_customer::ValidMarker;
}

[[maybe_unused]] ApOpErrCode get_uds_slot_num(uint8_t& slot_num)
{
    cfpa_customer::WrappedUds       wrapped_uds{};
    const nv::crypto::KeyClearGuard wrapped_uds_guard(to_span(wrapped_uds));
    if (read_puf_wrapped_key_from_cfpa(to_span(wrapped_uds), cfpa_customer::WrappedUdsOffset)
        != ApOpErrCode::Success) {
        return ApOpErrCode::FlashReadFailure;
    }

    // Check if the UDS is provisioned
    if (wrapped_uds.valid != cfpa_customer::ValidMarker) {
        return ApOpErrCode::FlashDataInvalid;
    }

    // UDS is provisioned, return the active slot number
    slot_num = wrapped_uds.active_slot_num;
    return ApOpErrCode::Success;
}

ApOpErrCode read_cek_from_flash(CekKey& cek_out)
{
    // The plaintext CEK sector lives in internal flash at CekPlaintextFlashAddr.
    // Direct XIP read from a non-privileged task (MCTP) traps on MPU, so route
    // through the Flash task which runs privileged.
    if (nv::flash::Flash::read(static_cast<uint32_t>(CekPlaintextFlashAddr),
                               std::span<uint8_t>(cek_out.data(), cek_out.size()))
        != nv::flash::Status::Ok) {
        return ApOpErrCode::FlashReadFailure;
    }
    constexpr uint8_t FlashEmptyValue = 0xFFU;
    // Erased flash means the CEK data was not provisioned.
    if (std::all_of(cek_out.begin(), cek_out.end(), [](uint8_t byte) {
            return byte == static_cast<uint8_t>(FlashEmptyValue);
        })) {
        return ApOpErrCode::FlashDataInvalid;
    }
    return ApOpErrCode::Success;
}

ApOpErrCode erase_cek_in_flash()
{
    // The plaintext CEK source sector is no longer needed once the wrapped
    // CEK has been programmed to CFPA; erase it so the plaintext can't be
    // recovered from flash later (e.g. via blhost or a debug probe).
    // Route through Flash task — direct (static) erase from MCTP context
    // traps on MPU just like the XIP read does.
    if (nv::flash::Flash::erase(static_cast<uint32_t>(CekPlaintextFlashAddr))
        != nv::flash::Status::Ok) {
        return ApOpErrCode::FlashWriteFailure;
    }
    return ApOpErrCode::Success;
}

ApOpErrCode read_ecid_from_lpu(EcIdKey& ecid_out)
{
    std::fill(ecid_out.begin(), ecid_out.end(), 0U);
    // TODO: Platform team to implement this.
    return ApOpErrCode::Success;
}

// Read the PUF-wrapped key record at `offset` from CFPA and unwrap into
// `key_out`. WrappedUds and WrappedCek share the same wrapped-key placement
// (84-byte puf_wrapped_key at offset 0), so one helper covers both.
ApOpErrCode load_key(uint32_t offset, std::span<uint8_t> key_out)
{
    static_assert(sizeof(cfpa_customer::WrappedUds) == sizeof(cfpa_customer::WrappedCek));
    static_assert(cfpa_customer::UdsWrappedKeySize == cfpa_customer::CekWrappedKeySize);

    // key_out is an output buffer owned by the caller; the caller is
    // responsible for guarding it. On failure we leave whatever bytes
    // puf_unwrap wrote — caller's KeyClearGuard zeros them at scope exit.
    std::array<uint8_t, sizeof(cfpa_customer::WrappedUds)> puf_wrapped_record{};
    const nv::crypto::KeyClearGuard puf_wrapped_record_guard(puf_wrapped_record);

    if (const auto status = read_puf_wrapped_key_from_cfpa(puf_wrapped_record, offset);
        status != ApOpErrCode::Success) {
        return status;
    }
    if (nv::spdm::crypto::puf_unwrap(std::span<const uint8_t>(puf_wrapped_record.data(),
                                                              cfpa_customer::UdsWrappedKeySize),
                                     key_out)
        != CryptoStatus::Success) {
        return ApOpErrCode::HardwareSdkFailure;
    }
    return ApOpErrCode::Success;
}

// Compose a UDS-KW-wrapped CEK from the CFPA-resident PUF-wrapped UDS and
// CEK records: read both, PUF-unwrap each to plaintext, then NIST SP800-38F
// (RFC 3394) KW-wrap the 32-byte plaintext CEK using the 32-byte plaintext
// UDS as KEK. The KW output is CekKeySize + 8 = 40 bytes.
[[maybe_unused]] ApOpErrCode
load_uds_wrapped_cek(std::span<uint8_t, UdsWrappedCekSize> uds_wrapped_cek_out)
{
    if constexpr (HasLpu) {
        // No explicit provision-status check: if either CFPA slot is unset,
        // its bytes are 0xFF/0x00 and PUF_Unwrap fails its integrity check,
        // surfacing as HardwareSdkFailure below. Same behavior as the
        // original code path.
        CekKey                          cek_plain{};
        const nv::crypto::KeyClearGuard cek_plain_guard{std::span<uint8_t>(cek_plain)};
        if (const auto status = load_key(cfpa_customer::WrappedCekOffset,
                                         std::span<uint8_t>(cek_plain));
            status != ApOpErrCode::Success) {
            return status;
        }

        UdsKey                          uds_plain{};
        const nv::crypto::KeyClearGuard uds_plain_guard{std::span<uint8_t>(uds_plain)};
        if (const auto status = load_key(cfpa_customer::WrappedUdsOffset,
                                         std::span<uint8_t>(uds_plain));
            status != ApOpErrCode::Success) {
            return status;
        }

        // uds_wrapped_cek_out is an output buffer owned by the caller; the
        // caller is responsible for guarding it. On failure we leave whatever
        // KW wrote — caller's KeyClearGuard zeros it at scope exit.
        if (nv::crypto::nist_sp800_38f_kw_wrap(
                uds_plain, std::span<const uint8_t>(cek_plain), uds_wrapped_cek_out)
            != nv::crypto::Status::Success) {
            return ApOpErrCode::CryptoFailure;
        }
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::NotSupported;
}

[[maybe_unused]] bool is_secure_mode_lock()
{
    // TODO: Read the LPU secure-mode fuse when that platform API is available.
    // Until then, keep the update policy permissive so both cleartext and
    // encrypted LPU images are accepted.
    return false;
}

[[maybe_unused]] ApOpErrCode patch_encrypted_lpu_fw_header(const lpu::PointerBlock& block,
                                                           std::span<uint8_t>       data)
{
    std::array<uint8_t, lpu::WrappedKeyPayloadSize> wrapped_cek{};
    const nv::crypto::KeyClearGuard wrapped_cek_guard{std::span<uint8_t>(wrapped_cek)};
    if (const auto status = load_uds_wrapped_cek(
            std::span<uint8_t, UdsWrappedCekSize>(wrapped_cek.data(), UdsWrappedCekSize));
        status != ApOpErrCode::Success) {
        return status;
    }

    uint8_t uds_slot = 0U;
    if (const auto status = get_uds_slot_num(uds_slot); status != ApOpErrCode::Success) {
        return status;
    }
    if (!is_valid_uds_slot(uds_slot)) {
        return ApOpErrCode::InvalidParam;
    }

    return from_lpu_layout(lpu::patch_encrypted_fw_header(
        data,
        block,
        uds_slot,
        std::span<const uint8_t, lpu::WrappedKeyPayloadSize>(wrapped_cek)));
}

// Read back the PUF-wrapped key at `offset`, unwrap it, and check it matches
// `expected`. UdsKey and CekKey are both 32-byte arrays, so one helper covers
// both.
ApOpErrCode verify_programmed_puf_wrapped_key(uint32_t                 offset,
                                              std::span<const uint8_t> expected)
{
    static_assert(UdsKeySize == CekKeySize);

    std::array<uint8_t, UdsKeySize> verify{};
    const nv::crypto::KeyClearGuard verify_guard{std::span<uint8_t>(verify)};
    if (const auto status = load_key(offset, std::span<uint8_t>(verify));
        status != ApOpErrCode::Success) {
        return status;
    }
    return std::equal(verify.begin(), verify.end(), expected.begin(), expected.end())
             ? ApOpErrCode::Success
             : ApOpErrCode::ProvisionFailure;
}

ApOpErrCode verify_programmed_ecid(std::span<const uint8_t> expected)
{
    if (expected.size() != EcIdKeySize) {
        return ApOpErrCode::InvalidParam;
    }

    cfpa_customer::EcId             ecid{};
    const nv::crypto::KeyClearGuard ecid_guard(to_span(ecid));
    if (nv::flash::Flash::read_cfpa_customer(to_span(ecid), cfpa_customer::EcIdOffset)
        != nv::flash::Status::Ok) {
        return ApOpErrCode::FlashReadFailure;
    }
    if (ecid.valid != cfpa_customer::ValidMarker) {
        return ApOpErrCode::ProvisionFailure;
    }
    return std::equal(ecid.ecid.begin(), ecid.ecid.end(), expected.begin(), expected.end())
             ? ApOpErrCode::Success
             : ApOpErrCode::ProvisionFailure;
}

ApOpErrCode program_uds_to_lpu([[maybe_unused]] std::span<const uint8_t> uds_key,
                               [[maybe_unused]] uint8_t                  slot_num)
{
    // TODO: Platform team to implement this.
    return ApOpErrCode::Success;
}

ApOpErrCode lock_lpu_debug_port()
{
    // TODO: Platform team to implement this.
    return ApOpErrCode::Success;
}

ApOpErrCode read_lpu_debug_port_status(uint8_t& debug_port_status)
{
    debug_port_status = 0U;
    // TODO: Platform team to implement this.
    return ApOpErrCode::Success;
}

[[maybe_unused]] ApOpErrCode provision_new_uds()
{
    uint8_t    current_slot_num = 0U;
    const auto uds_slot_status  = get_uds_slot_num(current_slot_num);
    if (uds_slot_status == ApOpErrCode::FlashDataInvalid) {
        current_slot_num = cfpa_customer::LastReservedUdsSlotNum;
    }
    else if (uds_slot_status != ApOpErrCode::Success) {
        return uds_slot_status;
    }
    if (current_slot_num >= cfpa_customer::MaxUdsSlotNum) {
        return ApOpErrCode::NoCapacity;
    }
    const uint8_t next_slot_num = std::max<uint8_t>(static_cast<uint8_t>(current_slot_num + 1U),
                                                    cfpa_customer::FirstUdsSlotNum);

    // Generate a random UDS key with TRNG
    cfpa_customer::WrappedUds       puf_wrapped_uds{};
    const nv::crypto::KeyClearGuard puf_wrapped_uds_guard(to_span(puf_wrapped_uds));
    UdsKey                          uds_key{};
    const nv::crypto::KeyClearGuard uds_key_guard{std::span<uint8_t>(uds_key)};

    if (const auto status = generate_valid_uds_key(uds_key); status != ApOpErrCode::Success) {
        return status;
    }

    // Wrap the UDS key with PUF
    if (nv::spdm::crypto::puf_wrap(std::span<uint8_t>(uds_key.data(), uds_key.size()),
                                   std::span<uint8_t>(puf_wrapped_uds.wrapped_key.data(),
                                                      puf_wrapped_uds.wrapped_key.size()))
        != CryptoStatus::Success) {
        return ApOpErrCode::HardwareSdkFailure;
    }

    // Program the UDS key to the LPU first
    if (const auto status = program_uds_to_lpu(std::span<const uint8_t>(uds_key),
                                               next_slot_num);
        status != ApOpErrCode::Success) {
        return status;
    }

    // Program the UDS key to the MCU if programmed to the LPU successfully
    puf_wrapped_uds.valid           = cfpa_customer::ValidMarker;
    puf_wrapped_uds.active_slot_num = next_slot_num;

    // RMW the full CFPA customer record and overlay only the UDS slot so
    // ECID, CEK, and future customer fields remain on the active CFPA page.
    if (const auto status = program_cfpa_customer_data_to_mcu(to_span(puf_wrapped_uds),
                                                              cfpa_customer::WrappedUdsOffset);
        status != ApOpErrCode::Success) {
        return status;
    }

    // Verify the programmed UDS key matches the generated UDS key.
    if (const auto status = verify_programmed_puf_wrapped_key(
            cfpa_customer::WrappedUdsOffset,
            std::span<const uint8_t>(uds_key.data(), uds_key.size()));
        status != ApOpErrCode::Success) {
        return status;
    }

    uint8_t programmed_slot_num = 0U;
    if (const auto status = get_uds_slot_num(programmed_slot_num);
        status != ApOpErrCode::Success) {
        return status;
    }
    if (programmed_slot_num != next_slot_num) {
        return ApOpErrCode::ProvisionFailure;
    }

    return set_lpu_provision_status_bit(LpuProvisionState::UdsProvisioned);
}

ApOpErrCode read_lpu_provision_status(nv::flash::Data& provisioned)
{
    const auto status = nv::flash::Flash::get_data(nv::flash::Key::NpdsAp0ProvisionStatus,
                                                   provisioned);
    if (status == nv::flash::Status::Ok) {
        return ApOpErrCode::Success;
    }
    if (status == nv::flash::Status::Error) {
        provisioned = 0U;
        return ApOpErrCode::Success;
    }
    return ApOpErrCode::FlashReadFailure;
}

ApOpErrCode set_lpu_provision_status_bit(LpuProvisionState bit)
{
    nv::flash::Data provisioned = 0U;
    if (const auto status = read_lpu_provision_status(provisioned);
        status != ApOpErrCode::Success) {
        return status;
    }

    const auto bit_value = static_cast<nv::flash::Data>(bit);
    if ((provisioned & bit_value) == bit_value) {
        return ApOpErrCode::Success;
    }

    provisioned |= bit_value;
    if (nv::flash::Flash::set_data(nv::flash::Key::NpdsAp0ProvisionStatus, provisioned)
        != nv::flash::Status::Ok) {
        return ApOpErrCode::FlashWriteFailure;
    }
    return ApOpErrCode::Success;
}

// All-or-nothing provisioning: generates UDS, reads CEK and ECID, PUF-wraps
// UDS/CEK, burns UDS into the LPU, then commits all three slots to CFPA
// customer in a SINGLE Flash::write_cfpa_customer call. The single-write
// constraint matters: cfpa_driver's session-offset state only resets across
// boots, so two CFPA writes in one boot would skip the second's setup block
// and fail verify_erased. Verifies all slots round-trip and commits each NPDS
// status bit independently.
[[maybe_unused]] ApOpErrCode provision_all()
{
    constexpr auto AllBits = static_cast<nv::flash::Data>(LpuProvisionState::UdsProvisioned)
                           | static_cast<nv::flash::Data>(LpuProvisionState::CekProvisioned)
                           | static_cast<nv::flash::Data>(LpuProvisionState::EcIdProvisioned);

    // AlreadyProvisioned only when all bits are set (a benign replay of a
    // successful provision). A subset of bits means the device is in an
    // unexpected partial state (failed mid-op under prior firmware, NPDS
    // tamper, or CFPA corruption) — surface that distinctly via
    // ProvisionFailure → InconsistentState in the translator below.
    nv::flash::Data provisioned = 0U;
    if (const auto status = read_lpu_provision_status(provisioned);
        status != ApOpErrCode::Success) {
        return status;
    }
    if ((provisioned & AllBits) == AllBits) {
        return ApOpErrCode::AlreadyProvisioned;
    }
    if ((provisioned & AllBits) != 0U) {
        // Some but not all bits are set — device is in an inconsistent state.
        return ApOpErrCode::ProvisionFailure;
    }

    // 1. TRNG-generate the 32-byte UDS plaintext.
    UdsKey                          uds_key{};
    const nv::crypto::KeyClearGuard uds_key_guard{std::span<uint8_t>(uds_key)};
    if (const auto status = generate_valid_uds_key(uds_key); status != ApOpErrCode::Success) {
        return status;
    }

    // 2. Read the plaintext CEK from the reserved flash sector at 0xEA000
    //    (programmed at manufacture time via the SB `load` command).
    CekKey                          cek_plaintext{};
    const nv::crypto::KeyClearGuard cek_plaintext_guard{std::span<uint8_t>(cek_plaintext)};
    if (const auto status = read_cek_from_flash(cek_plaintext);
        status != ApOpErrCode::Success) {
        return status;
    }

    // 3. Read ECID from the LPU.
    EcIdKey                         ecid_bytes{};
    const nv::crypto::KeyClearGuard ecid_bytes_guard{std::span<uint8_t>(ecid_bytes)};
    if (const auto status = read_ecid_from_lpu(ecid_bytes); status != ApOpErrCode::Success) {
        return status;
    }

    // 4. PUF-wrap UDS.
    cfpa_customer::WrappedUds       puf_wrapped_uds{};
    const nv::crypto::KeyClearGuard puf_wrapped_uds_guard(to_span(puf_wrapped_uds));
    if (nv::spdm::crypto::puf_wrap(std::span<uint8_t>(uds_key.data(), uds_key.size()),
                                   std::span<uint8_t>(puf_wrapped_uds.wrapped_key.data(),
                                                      puf_wrapped_uds.wrapped_key.size()))
        != CryptoStatus::Success) {
        return ApOpErrCode::HardwareSdkFailure;
    }

    // 5. PUF-wrap CEK. Route through SPDM task so PUF hardware is touched
    //    from privileged SPDM context.
    cfpa_customer::WrappedCek       puf_wrapped_cek{};
    const nv::crypto::KeyClearGuard puf_wrapped_cek_guard(to_span(puf_wrapped_cek));
    if (nv::spdm::crypto::puf_wrap(
            std::span<uint8_t>(cek_plaintext.data(), cek_plaintext.size()),
            std::span<uint8_t>(puf_wrapped_cek.wrapped_key.data(),
                               puf_wrapped_cek.wrapped_key.size()))
        != CryptoStatus::Success) {
        return ApOpErrCode::HardwareSdkFailure;
    }

    // 6. Program the UDS key to the LPU vROT first. On failure no MCU-side
    //    state has been mutated yet — CFPA and NPDS are still pristine.
    if (const auto status = program_uds_to_lpu(std::span<const uint8_t>(uds_key),
                                               cfpa_customer::FirstUdsSlotNum);
        status != ApOpErrCode::Success) {
        return status;
    }

    puf_wrapped_uds.valid           = cfpa_customer::ValidMarker;
    puf_wrapped_uds.active_slot_num = cfpa_customer::FirstUdsSlotNum;
    puf_wrapped_cek.valid           = cfpa_customer::ValidMarker;

    cfpa_customer::EcId             ecid{};
    const nv::crypto::KeyClearGuard ecid_guard(to_span(ecid));
    std::copy(ecid_bytes.begin(), ecid_bytes.end(), ecid.ecid.begin());
    ecid.valid = cfpa_customer::ValidMarker;

    // 7. Single CFPA-customer RMW: read the full 208-byte customer record,
    //    overlay UDS, CEK, and ECID, write back at offset 0 in ONE
    //    Flash::write_cfpa_customer call.
    {
        // NOLINTNEXTLINE(misc-const-correctness)
        cfpa_customer::CfpaCustomerData record{};
        auto record_span = std::span<uint8_t>(std::bit_cast<uint8_t*>(&record), sizeof(record));
        const nv::crypto::KeyClearGuard record_span_guard(record_span);

        if (nv::flash::Flash::read_cfpa_customer(record_span, 0) != nv::flash::Status::Ok) {
            return ApOpErrCode::FlashReadFailure;
        }

        std::memcpy(record_span.data() + cfpa_customer::WrappedUdsOffset,
                    to_span(puf_wrapped_uds).data(),
                    sizeof(puf_wrapped_uds));
        std::memcpy(record_span.data() + cfpa_customer::WrappedCekOffset,
                    to_span(puf_wrapped_cek).data(),
                    sizeof(puf_wrapped_cek));
        std::memcpy(
            record_span.data() + cfpa_customer::EcIdOffset, to_span(ecid).data(), sizeof(ecid));

        if (nv::flash::Flash::write_cfpa_customer(record_span, 0) != nv::flash::Status::Ok) {
            return ApOpErrCode::FlashWriteFailure;
        }
    }

    // 8. Verify all slots round-trip cleanly. Each
    //    verify reads CFPA but doesn't write, so the single-write
    //    invariant is preserved.
    if (const auto status = verify_programmed_puf_wrapped_key(
            cfpa_customer::WrappedUdsOffset,
            std::span<const uint8_t>(uds_key.data(), uds_key.size()));
        status != ApOpErrCode::Success) {
        return status;
    }
    if (const auto status = verify_programmed_puf_wrapped_key(
            cfpa_customer::WrappedCekOffset,
            std::span<const uint8_t>(cek_plaintext.data(), cek_plaintext.size()));
        status != ApOpErrCode::Success) {
        return status;
    }
    if (const auto status = verify_programmed_ecid(
            std::span<const uint8_t>(ecid_bytes.data(), ecid_bytes.size()));
        status != ApOpErrCode::Success) {
        return status;
    }

    // 9. Erase the plaintext CEK source sector so the key can't be recovered
    //    from flash after provisioning.
    if (const auto status = erase_cek_in_flash(); status != ApOpErrCode::Success) {
        return status;
    }

    // 10. Set NPDS bits independently so each asset's status update preserves
    // any existing provision bits.
    if (const auto status = set_lpu_provision_status_bit(LpuProvisionState::UdsProvisioned);
        status != ApOpErrCode::Success) {
        return status;
    }
    if (const auto status = set_lpu_provision_status_bit(LpuProvisionState::CekProvisioned);
        status != ApOpErrCode::Success) {
        return status;
    }
    if (const auto status = set_lpu_provision_status_bit(LpuProvisionState::EcIdProvisioned);
        status != ApOpErrCode::Success) {
        return status;
    }
    return ApOpErrCode::Success;
}

[[maybe_unused]] uint8_t to_lpu_provision_completion_code(ApOpErrCode status)
{
    using CC = lpu::ProvisionCompletionCode;
    switch (status) {
        case ApOpErrCode::Success: return static_cast<uint8_t>(CC::Success);
        case ApOpErrCode::AlreadyProvisioned:
            return static_cast<uint8_t>(CC::AlreadyProvisioned);
        case ApOpErrCode::HardwareSdkFailure: return static_cast<uint8_t>(CC::CryptoFailed);
        case ApOpErrCode::FlashReadFailure  :
        case ApOpErrCode::FlashWriteFailure : return static_cast<uint8_t>(CC::CfpaWriteFailed);
        case ApOpErrCode::ProvisionFailure  : return static_cast<uint8_t>(CC::InconsistentState);
        case ApOpErrCode::NoCapacity        : return static_cast<uint8_t>(CC::NoCapacity);
        // LPU-side errors from program_uds_to_lpu (LpuUds SMBus transport).
        case ApOpErrCode::Timeout:
        case ApOpErrCode::Busy:
        case ApOpErrCode::Fail   : return static_cast<uint8_t>(CC::LpuUdsProvisionedFailed);
        default                  : return static_cast<uint8_t>(CC::GeneralError);
    }
}

}  // namespace

}  // namespace nv::vrot
