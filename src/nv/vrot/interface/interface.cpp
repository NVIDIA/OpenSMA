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

#include "nv/vrot/platform/cpld.h"

namespace nv::vrot {
namespace {

constexpr bool HasCpld = has_ap_type(ApType::Cpld, ApList);

}  // namespace

ApOpErrCode hold_reset(const ApInfo& ap)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::hold_reset(ap);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode pre_authenticate(const ApInfo& ap)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::pre_authenticate(ap);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode post_authenticate(const ApInfo& ap, nv::spdm::crypto::CryptoStatus result)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::post_authenticate(ap, result);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode release_reset(const ApInfo& ap)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::release_reset(ap);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode check_booted(const ApInfo& ap)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::check_booted(ap);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode fw_update_prepare(const ApInfo& ap)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::fw_update_prepare(ap);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode fw_update_callback(const ApInfo& ap, nv::spdm::crypto::CryptoStatus status)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::fw_update_callback(ap, status);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode read_metadata(const ApInfo& ap, uint32_t metadata_offset, std::span<uint8_t> data)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::read_metadata(ap, metadata_offset, data);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode read_fw_data(const ApInfo& ap, uint32_t fw_data_offset, std::span<uint8_t> data)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::read_fw_data(ap, fw_data_offset, data);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode
write_metadata(const ApInfo& ap, uint32_t metadata_offset, std::span<const uint8_t> data)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::write_metadata(ap, metadata_offset, data);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode
write_fw_data(const ApInfo& ap, uint32_t fw_data_offset, std::span<const uint8_t> data)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::write_fw_data(ap, fw_data_offset, data);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode set_debug_token_feature(const ApInfo& ap, DebugTokenFeature feature, bool enable)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::set_debug_token_feature(ap, feature, enable);
        }
    }
    return ApOpErrCode::NotSupported;
}

void notify_ap_reset([[maybe_unused]] const ApInfo& ap) {}

// ---------------------------------------------------------------------------
// AP-by-component-id dispatch. Single source of truth for "which AP is this
// component_id, and what platform Ops handle it?". Each function looks up
// `ap` once against ApList and routes via switch on ap->type to the matching
// platform Ops, following the same `if constexpr (HasCpld) + switch` pattern
// as the AP-by-handle surface above.
//
// Adding a new AP type means:
//   1) extend `ApType` in types.h,
//   2) add a `HasXxx` constexpr in the anonymous namespace at the top of
//      this file,
//   3) extend the outer `if constexpr` guard with `|| HasXxx`,
//   4) add a `case ApType::Xxx: return XxxOps::method(*ap, ...);` arm in
//      each function below.
// ---------------------------------------------------------------------------
namespace ap {

ApOpErrCode fw_update_callback(uint16_t component_id, nv::spdm::crypto::CryptoStatus result)
{
    auto ap = find_ap_by_component_id(component_id, ApList);
    if (!ap) {
        return ApOpErrCode::UnknownComponent;
    }
    if constexpr (HasCpld) {
        switch (ap->type) {
            case ApType::Cpld: return CpldOps::fw_update_callback(*ap, result);
        }
    }
    return ApOpErrCode::NotSupported;
}

ApOpErrCode request_authentication(uint16_t component_id)
{
    auto ap = find_ap_by_component_id(component_id, ApList);
    if (!ap) {
        return ApOpErrCode::UnknownComponent;
    }
    if constexpr (HasCpld) {
        switch (ap->type) {
            case ApType::Cpld: return CpldOps::request_authentication(*ap);
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
    if constexpr (HasCpld) {
        switch (ap->type) {
            case ApType::Cpld: return CpldOps::get_write_fail_retry(*ap);
        }
    }
    return 0;
}

}  // namespace ap

}  // namespace nv::vrot
