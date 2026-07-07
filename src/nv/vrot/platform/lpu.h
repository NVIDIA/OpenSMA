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
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/spdm/crypto_status.h"
#include "nv/vrot/interface/types.h"
#include NV_IPC_CONFIG_H

#ifndef NV_VROT_LPU_FLASH_SIZE_BYTES
#define NV_VROT_LPU_FLASH_SIZE_BYTES (0U)
#endif

namespace nv::vrot {

// Persisted in NpdsAp0ProvisionStatus. The numeric values are part of the
// on-flash format -- do not renumber. ProvisionAll still requires the combined
// flow to verify all assets, but commits each completion bit independently.
enum class LpuProvisionState : uint8_t
{
    NotProvisioned  = 0x00,
    UdsProvisioned  = 0x01,  // (bit 1)
    CekProvisioned  = 0x02,  // (bit 2)
    EcIdProvisioned = 0x04,  // (bit 3)
};

namespace lpu {

// VDM-payload sizes — these commands carry no AP-specific data
// (the CEK plaintext and ECID are sourced on-device, not from the wire).
constexpr std::size_t ProvisionAllDataSize     = 0;
constexpr std::size_t ProvisionNewUdsDataSize  = 0;
constexpr std::size_t LockDebugPortDataSize    = 0;
constexpr uint8_t     UdsSlotNumNotProvisioned = 0xFF;

enum class ProvisionSubCommand : uint8_t
{
    // Single combined provisioning: TRNG UDS + read CEK/ECID + PUF-wrap
    // UDS/CEK + LPU UDS burn + one CFPA customer write + verify all slots.
    // Designed so that exactly one Flash::write_cfpa_customer happens per boot, which
    // sidesteps the cfpa_driver session-offset reuse bug that bit the older
    // per-slot sub-commands.
    ProvisionAll    = 0x00,
    ProvisionNewUds = 0x01,
    LockDebugPort   = 0x02,
};

enum class QueryApProvisionStatusSubCommand : uint8_t
{
    ProvisionState  = 0x00,
    UdsSlotNum      = 0x01,
    DebugPortStatus = 0x02,
};

// Per the AP Provision VDM spec.
enum class ProvisionCompletionCode : uint8_t
{
    Success                 = 0x00,
    GeneralError            = 0x01,
    AlreadyProvisioned      = 0x02,  // All provision bits set in NPDS — benign replay
    LpuUdsProvisionedFailed = 0x03,  // LPU vROT SMBus UDS burn failed
    CryptoFailed            = 0x04,  // TRNG or PUF wrap failed
    CfpaWriteFailed         = 0x05,  // CFPA customer write or read-back failed
    // Device state doesn't match expectations. Covers two cases:
    //   (a) Some but not all provision bits are set in NPDS at entry — partial
    //       provisioning from a prior firmware, NPDS tamper, or CFPA
    //       corruption.
    //   (b) Verify round-trip after the CFPA write didn't match the
    //       plaintext we just generated/read (CFPA flash anomaly or PUF
    //       context drift).
    // Both have the same recovery: investigate / reflash. Distinct from
    // AlreadyProvisioned so the host can flag a broken device.
    InconsistentState = 0x06,
    NoCapacity        = 0x07,  // All available LPU UDS slots are already used.
};

}  // namespace lpu

// LPU external SPI flash is split into two equal slots. Each slot stores the
// AP firmware image first and the AP metadata at the end of the slot.
namespace LpuFlashLayout {
constexpr uint32_t FlashSize          = NV_VROT_LPU_FLASH_SIZE_BYTES;
constexpr uint32_t SlotCount          = 2U;
constexpr auto     MetadataRegionSize = static_cast<uint32_t>(
    sizeof(nv::fw_parser::ap::ApFwMetadata));
constexpr uint32_t SlotSize  = FlashSize / SlotCount;
constexpr uint32_t ImageSize = SlotSize > MetadataRegionSize ? SlotSize - MetadataRegionSize
                                                             : 0U;
}  // namespace LpuFlashLayout

namespace LpuFlashPartitionIndex {
constexpr uint8_t Slot0Image    = 0;
constexpr uint8_t Slot0Metadata = 1;
constexpr uint8_t Slot1Image    = 2;
constexpr uint8_t Slot1Metadata = 3;
constexpr uint8_t Count         = 4;
}  // namespace LpuFlashPartitionIndex

struct LpuOps
{
    static ApOpErrCode hold_reset(const ApInfo& ap);
    static ApOpErrCode pre_authenticate(const ApInfo& ap);
    static ApOpErrCode post_authenticate(const ApInfo&                  ap,
                                         nv::spdm::crypto::CryptoStatus result);
    static ApOpErrCode release_reset(const ApInfo& ap);
    static ApOpErrCode check_booted(const ApInfo& ap);

    static ApOpErrCode read_metadata(const ApInfo&      ap,
                                     uint8_t            slot,
                                     uint32_t           metadata_offset,
                                     std::span<uint8_t> data);
    static ApOpErrCode read_fw_data(const ApInfo&      ap,
                                    uint8_t            slot,
                                    uint32_t           fw_data_offset,
                                    std::span<uint8_t> data);
    static ApOpErrCode write_metadata(const ApInfo&            ap,
                                      uint8_t                  slot,
                                      uint32_t                 metadata_offset,
                                      std::span<const uint8_t> data);
    static ApOpErrCode write_fw_data(const ApInfo&      ap,
                                     uint8_t            slot,
                                     uint32_t           fw_data_offset,
                                     std::span<uint8_t> data,
                                     bool               background_copy);

    static ApOpErrCode fw_update_prepare(const ApInfo& ap);
    static ApOpErrCode fw_update_callback(const ApInfo&                  ap,
                                          nv::spdm::crypto::CryptoStatus result);

    static ApOpErrCode
    set_debug_token_feature(const ApInfo& ap, DebugTokenFeature feature, bool enable);

    static ApOpErrCode request_authentication(const ApInfo& ap, uint8_t& auth_request_id);
    static uint8_t     get_write_fail_retry(const ApInfo& ap);

    static ApOpErrCode ap_provision(const ApInfo&            ap,
                                    uint8_t                  sub_command,
                                    std::span<const uint8_t> data,
                                    uint8_t&                 ap_completion_code);
    static ApOpErrCode
    query_ap_provision_status(const ApInfo& ap, uint8_t sub_command, uint8_t& provision_info);

    static bool is_uds_provisioned();
    static bool is_cek_provisioned();
    static bool is_ecid_provisioned();

    // Compose the LpuProvisionState bitflag from CFPA-resident wrapped keys
    // and write it to NpdsAp0ProvisionStatus. Called once per boot.
    static ApOpErrCode set_ap_provision_status(const ApInfo& ap);

    static bool        supports_background_copy(const ApInfo& ap);
    static ApOpErrCode background_copy_regions(const ApInfo&                     ap,
                                               uint8_t                           source_slot,
                                               std::span<ApBackgroundCopyRegion> regions,
                                               std::size_t&                      region_count,
                                               uint32_t&                         total_size);
    static ApOpErrCode background_copy_begin(const ApInfo& ap);
    static ApOpErrCode background_copy_end(const ApInfo& ap);
};

}  // namespace nv::vrot
