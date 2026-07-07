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
#include <optional>

namespace nv::vrot {

enum class ApId : uint8_t
{
    AP0     = 0,
    AP1     = 1,
    Invalid = 0xFF,  // PLDM-only components without a physical AP index.
};

enum class ApType : uint8_t
{
    Cpld = 0,
    Lpu  = 1,
};

enum class ApOpErrCode : uint8_t
{
    Success = 0,
    Fail,
    InvalidParam,
    Timeout,
    Busy,
    NotSupported,
    // Returned by AP-by-component-id surface (`nv::vrot::ap::*`) when the
    // caller-supplied PLDM `component_id` does not match any AP in the
    // project's `ApList`. Distinct from `InvalidParam` so callers can
    // distinguish "this AP is not configured for this project" (a legitimate
    // outcome on AP-less projects) from real argument-validation failures.
    UnknownComponent,
    AlreadyProvisioned,
    FlashReadFailure,
    FlashWriteFailure,
    HardwareSdkFailure,
    ProvisionFailure,
    CryptoFailure,
    FlashDataInvalid,
    NoCapacity,
    PlaintextImageNotAllowed,
    // AP firmware image failed layout or header validation (e.g. pointer block,
    // region containment, encrypted header structure). Distinct from
    // InvalidParam so callers can distinguish malformed images from bad args.
    ImageLayoutInvalid,
};

enum class DebugTokenFeature : uint8_t
{
    CpldUnlock = 0,
};

struct ApInfo
{
    ApId     id;
    ApType   type;
    uint16_t component_id;
    uint32_t fw_size;
    uint32_t fw_offset;
    uint32_t ap_sku_id;
    uint8_t  build_mode;

    // Optional AP metadata placement in the same address space as fw_offset.
    // Projects that store metadata in a separate AP-specific region can leave
    // these as zero and let the platform Ops use its native layout.
    uint32_t metadata_offset = 0;
    uint32_t metadata_size   = 0;
};

struct ApBackgroundCopyRegion
{
    uint32_t offset = 0;
    uint32_t size   = 0;
};

constexpr std::size_t ApBackgroundCopyMaxRegions = 2;

template<std::size_t N>
constexpr std::optional<ApInfo> find_ap(ApId id, const std::array<ApInfo, N>& list)
{
    for (const auto& ap : list) {
        if (ap.id == id) return ap;
    }
    return std::nullopt;
}

template<std::size_t N>
constexpr std::optional<ApInfo> find_ap_by_component_id(uint16_t component_id,
                                                        const std::array<ApInfo, N>& list)
{
    for (const auto& ap : list) {
        if (ap.component_id == component_id) return ap;
    }
    return std::nullopt;
}

template<std::size_t N>
constexpr bool has_ap_type(ApType type, const std::array<ApInfo, N>& list)
{
    for (const auto& ap : list) {
        if (ap.type == type) return true;
    }
    return false;
}

}  // namespace nv::vrot
