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
};

enum class ApOpErrCode : uint8_t
{
    Success = 0,
    Fail,
    InvalidParam,
    Timeout,
    Busy,
    NotSupported,
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
};

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
