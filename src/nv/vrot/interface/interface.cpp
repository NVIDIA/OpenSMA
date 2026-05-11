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

ApOpErrCode fw_update_write(const ApInfo& ap, uint32_t addr, std::span<const uint8_t> data)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::fw_update_write(ap, addr, data);
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

ApOpErrCode read_flash(const ApInfo& ap, uint32_t start_address, std::span<uint8_t> data)
{
    if constexpr (HasCpld) {
        switch (ap.type) {
            case ApType::Cpld: return CpldOps::read_flash(ap, start_address, data);
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

}  // namespace nv::vrot
