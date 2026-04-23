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

#pragma once
#include <cstdint>
#include <array>
#include <cstddef>

namespace nv::pldm {
#define NV_PLDM_MAX_COMPONENT_SIZE 3

inline constexpr uint16_t COMP_CPLD = 0xff03;
inline constexpr uint16_t COMP_HPDO = 0xff04;

struct FwInfo
{
    uint16_t component_id;
    uint32_t fw_size;
    uint32_t fw_offset;
    uint32_t ap_sku_id;
    uint8_t  build_mode;
};

template<size_t N>
constexpr bool component_id_in_fw_info_list(uint16_t                     component_id,
                                            const std::array<FwInfo, N>& fw_info_list) noexcept
{
    if constexpr (N == 0) {
        return false;
    }
    for (size_t i = 0; i < N; ++i) {
        if (fw_info_list[i].component_id == component_id) {
            return true;
        }
    }
    return false;
}

template<size_t N>
constexpr FwInfo get_fw_info_from_list(uint16_t                     component_id,
                                       uint8_t&                     is_valid,
                                       const std::array<FwInfo, N>& fw_info_list)
{
    FwInfo result{};
    is_valid = 0;

    if constexpr (N > 0) {
        for (uint8_t i = 0; i < N; ++i) {
            if (fw_info_list[i].component_id == component_id) {
                result   = fw_info_list[i];
                is_valid = 1;
                break;
            }
        }
    }
    return result;
}

}  // namespace nv::pldm