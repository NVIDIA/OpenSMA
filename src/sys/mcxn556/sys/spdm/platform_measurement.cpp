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
#include "sys/spdm/platform_measurement.h"

namespace sys::spdm {

std::array<std::pair<uint32_t, uint32_t>, FUSE_ARRAY_SIZE> get_fuse_index_mask_pair()
{
    // Constants for fuse configuration
    constexpr uint32_t ALL_BITS_SET_MASK = 0xFFFFFFFF;

    // Fuse indices for platform measurement
    constexpr uint32_t FUSE_INDEX_0    = 0;
    constexpr uint32_t FUSE_INDEX_1    = 1;
    constexpr uint32_t FUSE_INDEX_2    = 2;
    constexpr uint32_t FUSE_INDEX_3    = 3;
    constexpr uint32_t FUSE_INDEX_6    = 6;
    constexpr uint32_t FUSE_INDEX_7    = 7;
    constexpr uint32_t FUSE_INDEX_11   = 11;
    constexpr uint32_t FUSE_INDEX_12   = 12;
    constexpr uint32_t FUSE_INDEX_13   = 13;
    constexpr uint32_t FUSE_INDEX_14   = 14;
    constexpr uint32_t FUSE_INDEX_15   = 15;
    constexpr uint32_t FUSE_INDEX_16   = 16;
    constexpr uint32_t FUSE_INDEX_17   = 17;
    constexpr uint32_t FUSE_INDEX_18   = 18;
    constexpr uint32_t FUSE_INDEX_19   = 19;
    constexpr uint32_t FUSE_INDEX_20   = 20;
    constexpr uint32_t FUSE_INDEX_21   = 21;
    constexpr uint32_t FUSE_INDEX_22   = 22;
    constexpr uint32_t FUSE_INDEX_23   = 23;
    constexpr uint32_t FUSE_INDEX_24   = 24;
    constexpr uint32_t FUSE_INDEX_25   = 25;
    constexpr uint32_t FUSE_INDEX_26   = 26;
    constexpr uint32_t FUSE_INDEX_27   = 27;
    constexpr uint32_t FUSE_INDEX_28   = 28;
    constexpr uint32_t FUSE_INDEX_29   = 29;
    constexpr uint32_t FUSE_INDEX_30   = 30;
    constexpr uint32_t DefaultFuseMask = ALL_BITS_SET_MASK;

    std::array<std::pair<uint32_t, uint32_t>, FUSE_ARRAY_SIZE> FuseIndexsMask{
        {{FUSE_INDEX_0, DefaultFuseMask},  {FUSE_INDEX_1, DefaultFuseMask},
         {FUSE_INDEX_2, DefaultFuseMask},  {FUSE_INDEX_3, DefaultFuseMask},
         {FUSE_INDEX_6, DefaultFuseMask},  {FUSE_INDEX_7, DefaultFuseMask},
         {FUSE_INDEX_11, DefaultFuseMask}, {FUSE_INDEX_12, DefaultFuseMask},
         {FUSE_INDEX_13, DefaultFuseMask}, {FUSE_INDEX_14, DefaultFuseMask},
         {FUSE_INDEX_15, DefaultFuseMask}, {FUSE_INDEX_16, DefaultFuseMask},
         {FUSE_INDEX_17, DefaultFuseMask}, {FUSE_INDEX_18, DefaultFuseMask},
         {FUSE_INDEX_19, DefaultFuseMask}, {FUSE_INDEX_20, DefaultFuseMask},
         {FUSE_INDEX_21, DefaultFuseMask}, {FUSE_INDEX_22, DefaultFuseMask},
         {FUSE_INDEX_23, DefaultFuseMask}, {FUSE_INDEX_24, DefaultFuseMask},
         {FUSE_INDEX_25, DefaultFuseMask}, {FUSE_INDEX_26, DefaultFuseMask},
         {FUSE_INDEX_27, DefaultFuseMask}, {FUSE_INDEX_28, DefaultFuseMask},
         {FUSE_INDEX_29, DefaultFuseMask}, {FUSE_INDEX_30, DefaultFuseMask}}
    };
    return FuseIndexsMask;
}

}  // namespace sys::spdm
