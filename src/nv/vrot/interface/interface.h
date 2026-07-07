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

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "nv/flash/common.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/spdm/crypto_status.h"
#include "nv/vrot/interface/types.h"

namespace nv::vrot {

constexpr uint8_t ApSlotUseActive = 0xFF;
constexpr uint8_t ApSlotUseUpdate = 0xFE;

inline uint8_t slot_from_parsing(nv::fw_parser::ap::ParsingApFwType slot)
{
    switch (slot) {
        case nv::fw_parser::ap::ParsingApFwType::ActiveSlot: return ApSlotUseActive;
        case nv::fw_parser::ap::ParsingApFwType::UpdateSlot: return ApSlotUseUpdate;
        default                                            : return ApSlotUseActive;
    }
}

// ---------------------------------------------------------------------------
// Public API. Each function takes an ApInfo that identifies which AP to operate
// on. The dispatch layer routes supported APs to the matching platform Ops.
// ---------------------------------------------------------------------------

// SecureBoot -> platform operations.
ApOpErrCode hold_reset(const ApInfo& ap);
ApOpErrCode pre_authenticate(const ApInfo& ap);
ApOpErrCode post_authenticate(const ApInfo& ap, nv::spdm::crypto::CryptoStatus result);
ApOpErrCode release_reset(const ApInfo& ap);
ApOpErrCode check_booted(const ApInfo& ap);

ApOpErrCode fw_update_prepare(const ApInfo& ap);
ApOpErrCode fw_update_callback(const ApInfo& ap, nv::spdm::crypto::CryptoStatus result);

// slot: physical index (0/1) or ApSlotUseActive / ApSlotUseUpdate.
ApOpErrCode read_metadata(const ApInfo&      ap,
                          uint8_t            slot,
                          uint32_t           metadata_offset,
                          std::span<uint8_t> data);
ApOpErrCode
read_fw_data(const ApInfo& ap, uint8_t slot, uint32_t fw_data_offset, std::span<uint8_t> data);
ApOpErrCode write_metadata(const ApInfo&            ap,
                           uint8_t                  slot,
                           uint32_t                 metadata_offset,
                           std::span<const uint8_t> data);
ApOpErrCode write_fw_data(const ApInfo&      ap,
                          uint8_t            slot,
                          uint32_t           fw_data_offset,
                          std::span<uint8_t> data,
                          bool               background_copy = false);

ApOpErrCode set_debug_token_feature(const ApInfo& ap, DebugTokenFeature feature, bool enable);

constexpr bool supports_ap_provision(ApType type)
{
    switch (type) {
        case ApType::Cpld: return false;
        case ApType::Lpu : return true;
    }
    return false;
}

template<std::size_t N>
constexpr bool has_ap_provision(const std::array<ApInfo, N>& list)
{
    for (const auto& ap : list) {
        if (supports_ap_provision(ap.type)) return true;
    }
    return false;
}

ApOpErrCode ap_provision(const ApInfo&            ap,
                         uint8_t                  sub_command,
                         std::span<const uint8_t> data,
                         uint8_t&                 ap_completion_code);
ApOpErrCode
query_ap_provision_status(const ApInfo& ap, uint8_t sub_command, uint8_t& provision_info);

// Compute the AP's provision state bitflag from underlying storage
// (e.g. CFPA for LPU) and write it to the NPDS provision-status cache.
// Called once per boot before any provision/query VDM runs.
ApOpErrCode set_ap_provision_status(const ApInfo& ap);

bool        supports_ap_background_copy(const ApInfo& ap);
ApOpErrCode background_copy_regions(const ApInfo&                     ap,
                                    uint8_t                           source_slot,
                                    std::span<ApBackgroundCopyRegion> regions,
                                    std::size_t&                      region_count,
                                    uint32_t&                         total_size);
ApOpErrCode background_copy_begin(const ApInfo& ap);
ApOpErrCode background_copy_end(const ApInfo& ap);

// Platform -> SecureBoot notifications.
// Future use: platform reset/update flows can call this to wake SecureBoot
// from terminal states and restart AP authentication after an AP reset.
void notify_ap_reset(const ApInfo& ap);

// ---------------------------------------------------------------------------
// AP-by-component-id surface. Each function takes a PLDM component_id, looks
// up the matching AP in nv::vrot::ApList, and dispatches via switch on
// ap.type. Returns UnknownComponent if component_id is not a known AP. Used by
// PLDM-FD wrappers so the wrappers don't have to plumb ApInfo themselves.
//
// Dispatch follows the same `if constexpr (HasAp) + switch + per-case
// if constexpr (HasXxx)` pattern as the AP-by-handle surface above.
// ---------------------------------------------------------------------------
namespace ap {

ApOpErrCode fw_update_callback(uint16_t component_id, nv::spdm::crypto::CryptoStatus result);

ApOpErrCode request_authentication(uint16_t component_id, uint8_t& auth_request_id);

uint8_t get_write_fail_retry(uint16_t component_id);

}  // namespace ap

}  // namespace nv::vrot
