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
#include "nv/vrot/interface/interface.h"

#include NV_IPC_CONFIG_H

#include "nv/secure_boot/secure_boot.h"
#include "nv/vrot/platform/cpld.h"
#include "nv/vrot/platform/lpu.h"

namespace nv::vrot {
namespace {

constexpr bool HasAp   = !ApList.empty();
constexpr bool HasCpld = has_ap_type(ApType::Cpld, ApList);
constexpr bool HasLpu  = has_ap_type(ApType::Lpu, ApList);

}  // namespace

ApOpErrCode hold_reset(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::hold_reset(ap);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::hold_reset(ap);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode pre_authenticate(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::pre_authenticate(ap);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::pre_authenticate(ap);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode post_authenticate(const ApInfo& ap, nv::spdm::crypto::CryptoStatus result)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::post_authenticate(ap, result);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::post_authenticate(ap, result);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode release_reset(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::release_reset(ap);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::release_reset(ap);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode check_booted(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::check_booted(ap);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::check_booted(ap);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode fw_update_prepare(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::fw_update_prepare(ap);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::fw_update_prepare(ap);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode fw_update_callback(const ApInfo& ap, nv::spdm::crypto::CryptoStatus status)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::fw_update_callback(ap, status);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::fw_update_callback(ap, status);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode
read_metadata(const ApInfo& ap, uint8_t slot, uint32_t metadata_offset, std::span<uint8_t> data)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::read_metadata(ap, slot, metadata_offset, data);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::read_metadata(ap, slot, metadata_offset, data);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode
read_fw_data(const ApInfo& ap, uint8_t slot, uint32_t fw_data_offset, std::span<uint8_t> data)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::read_fw_data(ap, slot, fw_data_offset, data);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::read_fw_data(ap, slot, fw_data_offset, data);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode write_metadata(const ApInfo&            ap,
                           uint8_t                  slot,
                           uint32_t                 metadata_offset,
                           std::span<const uint8_t> data)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::write_metadata(ap, slot, metadata_offset, data);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::write_metadata(ap, slot, metadata_offset, data);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode write_fw_data(const ApInfo&      ap,
                          uint8_t            slot,
                          uint32_t           fw_data_offset,
                          std::span<uint8_t> data,
                          bool               background_copy)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::write_fw_data(
                        ap, slot, fw_data_offset, data, background_copy);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::write_fw_data(
                        ap, slot, fw_data_offset, data, background_copy);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode set_debug_token_feature(const ApInfo& ap, DebugTokenFeature feature, bool enable)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::set_debug_token_feature(ap, feature, enable);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::set_debug_token_feature(ap, feature, enable);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode ap_provision(const ApInfo&            ap,
                         uint8_t                  sub_command,
                         std::span<const uint8_t> data,
                         uint8_t&                 ap_completion_code)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld: break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::ap_provision(ap, sub_command, data, ap_completion_code);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode set_ap_provision_status(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld: break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::set_ap_provision_status(ap);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode background_copy_begin(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld: break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::background_copy_begin(ap);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

bool supports_ap_background_copy(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld: break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::supports_background_copy(ap);
                }
                break;
        }
    }
    return false;
}

ApOpErrCode background_copy_regions(const ApInfo&                     ap,
                                    uint8_t                           source_slot,
                                    std::span<ApBackgroundCopyRegion> regions,
                                    std::size_t&                      region_count,
                                    uint32_t&                         total_size)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld: break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::background_copy_regions(
                        ap, source_slot, regions, region_count, total_size);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode background_copy_end(const ApInfo& ap)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld: break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::background_copy_end(ap);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode
query_ap_provision_status(const ApInfo& ap, uint8_t sub_command, uint8_t& provision_info)
{
    if constexpr (HasAp) {
        switch (ap.type) {
            case ApType::Cpld: break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::query_ap_provision_status(ap, sub_command, provision_info);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

void notify_ap_reset([[maybe_unused]] const ApInfo& ap)
{
    if constexpr (HasAp) {
        nv::secure_boot::SecureBoot::notify_ap_reset(ap);
    }
}

// ---------------------------------------------------------------------------
// AP-by-component-id dispatch. Single source of truth for "which AP is this
// component_id, and what platform Ops handle it?". Each function looks up
// `ap` once against ApList and routes via switch on ap->type to the matching
// platform Ops, following the same `if constexpr (HasAp) + switch +
// per-case if constexpr (HasXxx)` pattern as the AP-by-handle surface above.
//
// Adding a new AP type means:
//   1) extend `ApType` in types.h,
//   2) add a `HasXxx` constexpr in the anonymous namespace at the top of
//      this file,
//   3) add a `case ApType::Xxx: if constexpr (HasXxx) { return
//      XxxOps::method(*ap, ...); } break;` arm in each function below.
// ---------------------------------------------------------------------------
namespace ap {

ApOpErrCode fw_update_callback(uint16_t component_id, nv::spdm::crypto::CryptoStatus result)
{
    auto ap = find_ap_by_component_id(component_id, ApList);
    if (!ap) {
        return ApOpErrCode::UnknownComponent;
    }
    if constexpr (HasAp) {
        switch (ap->type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::fw_update_callback(*ap, result);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::fw_update_callback(*ap, result);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode request_authentication(uint16_t component_id, uint8_t& auth_request_id)
{
    auth_request_id = 0;
    auto ap         = find_ap_by_component_id(component_id, ApList);
    if (!ap) {
        return ApOpErrCode::UnknownComponent;
    }
    if constexpr (HasAp) {
        switch (ap->type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::request_authentication(*ap, auth_request_id);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::request_authentication(*ap, auth_request_id);
                }
                break;
        }
    }
    return ApOpErrCode::NotSupported;
}

uint8_t get_write_fail_retry(uint16_t component_id)
{
    auto ap = find_ap_by_component_id(component_id, ApList);
    if (!ap) {
        return 0;
    }
    if constexpr (HasAp) {
        switch (ap->type) {
            case ApType::Cpld:
                if constexpr (HasCpld) {
                    return CpldOps::get_write_fail_retry(*ap);
                }
                break;
            case ApType::Lpu:
                if constexpr (HasLpu) {
                    return LpuOps::get_write_fail_retry(*ap);
                }
                break;
        }
    }
    return 0;
}

}  // namespace ap

}  // namespace nv::vrot
