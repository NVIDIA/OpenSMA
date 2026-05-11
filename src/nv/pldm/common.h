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
#include <array>
#include <cstddef>
#include <cstdint>

namespace nv::pldm {

template<typename ApInfo, size_t N>
constexpr std::array<uint16_t, N>
make_component_id_list(const std::array<ApInfo, N>& ap_list) noexcept
{
    std::array<uint16_t, N> result{};

    for (size_t i = 0; i < N; ++i) {
        result[i] = ap_list[i].component_id;
    }
    return result;
}

}  // namespace nv::pldm
