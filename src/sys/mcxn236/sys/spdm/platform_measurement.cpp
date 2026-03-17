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
std::array<uint8_t, 16> get_boot_rom_cmac_in_kernel()
{
    return {};
}

std::array<std::pair<uint32_t, uint32_t>, FUSE_ARRAY_SIZE> get_fuse_index_mask_pair()
{
    using namespace sys::otp;
    // Constants for fuse configuration
    constexpr uint32_t ALL_BITS_SET_MASK = 0xFFFFFFFF;
    constexpr uint8_t  FIELD_DISABLED    = 0;

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
    auto               f1_mask         = std::bit_cast<Fuse1>(DefaultFuseMask);
    auto               f2_mask         = std::bit_cast<Fuse2>(DefaultFuseMask);
    auto               f3_mask         = std::bit_cast<Fuse3>(DefaultFuseMask);
    auto               f7_mask         = std::bit_cast<Fuse7>(DefaultFuseMask);

    // Disable RoTK (Root of Trust Key) fields for Fuse1
    f1_mask.rot_key_revoke.RoTK0_dis = FIELD_DISABLED;
    f1_mask.rot_key_revoke.RoTK1_dis = FIELD_DISABLED;
    f1_mask.rot_key_revoke.RoTK2_dis = FIELD_DISABLED;
    f1_mask.rot_key_revoke.RoTK3_dis = FIELD_DISABLED;

    // Disable key revocation fields for Fuse2
    f2_mask.dbg_key_revoke.DBG_KEY_REVOKE     = FIELD_DISABLED;
    f2_mask.image_key_revoke.IMAGE_KEY_REVOKE = FIELD_DISABLED;

    // Disable ISP I2C for Fuse3
    f3_mask.boot_cfg.ISP_I2C_Dis = FIELD_DISABLED;

    // Disable DICE and NPX lock context fields for Fuse7
    f7_mask.rotk_usage.SKIP_DICE     = FIELD_DISABLED;
    f7_mask.rotk_usage.NPX_LOCK_CTX0 = FIELD_DISABLED;
    f7_mask.rotk_usage.NPX_LOCK_CTX1 = FIELD_DISABLED;
    f7_mask.rotk_usage.NPX_LOCK_CTX2 = FIELD_DISABLED;
    f7_mask.rotk_usage.NPX_LOCK_CTX3 = FIELD_DISABLED;

    std::array<std::pair<uint32_t, uint32_t>, FUSE_ARRAY_SIZE> FuseIndexsMask{
        {{FUSE_INDEX_0, DefaultFuseMask},
         {FUSE_INDEX_1, std::bit_cast<uint32_t>(f1_mask)},
         {FUSE_INDEX_2, std::bit_cast<uint32_t>(f2_mask)},
         {FUSE_INDEX_3, std::bit_cast<uint32_t>(f3_mask)},
         {FUSE_INDEX_6, DefaultFuseMask},
         {FUSE_INDEX_7, std::bit_cast<uint32_t>(f7_mask)},
         {FUSE_INDEX_11, DefaultFuseMask},
         {FUSE_INDEX_12, DefaultFuseMask},
         {FUSE_INDEX_13, DefaultFuseMask},
         {FUSE_INDEX_14, DefaultFuseMask},
         {FUSE_INDEX_15, DefaultFuseMask},
         {FUSE_INDEX_16, DefaultFuseMask},
         {FUSE_INDEX_17, DefaultFuseMask},
         {FUSE_INDEX_18, DefaultFuseMask},
         {FUSE_INDEX_19, DefaultFuseMask},
         {FUSE_INDEX_20, DefaultFuseMask},
         {FUSE_INDEX_21, DefaultFuseMask},
         {FUSE_INDEX_22, DefaultFuseMask},
         {FUSE_INDEX_23, DefaultFuseMask},
         {FUSE_INDEX_24, DefaultFuseMask},
         {FUSE_INDEX_25, DefaultFuseMask},
         {FUSE_INDEX_26, DefaultFuseMask},
         {FUSE_INDEX_27, DefaultFuseMask},
         {FUSE_INDEX_28, DefaultFuseMask},
         {FUSE_INDEX_29, DefaultFuseMask},
         {FUSE_INDEX_30, DefaultFuseMask}}
    };
    return FuseIndexsMask;
}

}  // namespace sys::spdm
