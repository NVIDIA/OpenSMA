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

namespace sys::otp {

// OTP Index 0: [Reserved][LOCKS0][LOCKS1][LOCKS2]
struct Fuse0
{
    struct Reserved
    {
        uint8_t reserved : 8;
    };

    struct LOCKS0
    {
        uint8_t Reserved        : 1;  // [0] Reserved
        uint8_t BOOT_CFG_LOCK   : 3;  // [3:1] Boot Config Lock
        uint8_t PRINCE_CFG_LOCK : 3;  // [6:4] Prince Config Lock
        uint8_t OSCAA_KEY_LOCK  : 1;  // [7] OSCAA Key Lock [0]
    };

    struct LOCKS1
    {
        uint8_t OSCAA_KEY_LOCK : 2;  // [1:0] OSCAA Key Lock [2:1]
        uint8_t CUST_LOCK0     : 3;  // [4:2] Customer Lock 0
        uint8_t CUST_LOCK1     : 3;  // [7:5] Customer Lock 1
    };

    struct LOCKS2
    {
        uint8_t CUST_LOCK2 : 3;  // [2:0] Customer Lock 2
        uint8_t CUST_LOCK3 : 2;  // [5:4] Customer Lock 3
        uint8_t Reserved   : 2;  // [7:6] Reserved
    };

    Reserved reserved;
    LOCKS0   locks0;
    LOCKS1   locks1;
    LOCKS2   locks2;
};

static_assert(sizeof(Fuse0) == 4, "Fuse0 size is not correct");

// OTP Index 1: [LIFE_CYCLE][DCFG_CC_SOCU_NS][ROT_KEY_REVOKE][LIFE_CYCLE_DP]
struct Fuse1
{
    struct LIFE_CYCLE
    {
        uint8_t life_cycle : 8;  // [7:0] Life Cycle State
                                 // 0x00 = Blank
                                 // 0x01 = Fab
                                 // 0x03 = Develop
                                 // 0x07 = Develop2
                                 // 0x0F = InField
                                 // 0xCF = InFieldLock
                                 // 0x1F = FieldReturnOEM
                                 // 0x7F = FailureAnalysis
                                 // 0xFF = Bricked
    };

    struct DCFG_CC_SOCU_NS
    {
        uint8_t NIDEN_dis   : 1;  // [0] NID Enable Disable
        uint8_t DBGEN_dis   : 1;  // [1] DBG Enable Disable
        uint8_t SPIDEN_dis  : 1;  // [2] SPN ID Enable Disable
        uint8_t SPNIDEN_dis : 1;  // [3] SPN ID Enable Disable
        uint8_t Reserved    : 3;  // [6:4] Reserved
        uint8_t ISP_CMD_dis : 1;  // [7] ISP Command Disable
    };

    struct ROT_KEY_REVOKE
    {
        uint8_t FA_CMD_dis     : 1;  // [0] FA Command Disable
        uint8_t ME_CMD_dis     : 1;  // [1] ME Command Disable
        uint8_t UUID_CHECK_ENF : 1;  // [2] UUID Check Enable
        uint8_t Reserved       : 1;  // [3] Reserved
        uint8_t RoTK0_dis      : 1;  // [4] RoTK0 Disable
        uint8_t RoTK1_dis      : 1;  // [5] RoTK1 Disable
        uint8_t RoTK2_dis      : 1;  // [6] RoTK2 Disable
        uint8_t RoTK3_dis      : 1;  // [7] RoTK3 Disable
    };
    struct LIFE_CYCLE_DP
    {
        uint8_t life_cycle_dp : 8;  // [7:0] Life Cycle Inverse
    };

    LIFE_CYCLE      life_cycle;
    DCFG_CC_SOCU_NS dcfg_cc_socu_ns;
    ROT_KEY_REVOKE  rot_key_revoke;
    LIFE_CYCLE_DP   life_cycle_dp;
};

// OTP Index 2: [DBG_KEY_REVOKE][IMAGE_KEY_REVOKE][SEC_CFG][Reserved]
struct Fuse2
{
    struct DBG_KEY_REVOKE
    {
        uint8_t DBG_KEY_REVOKE : 8;  // [7:0] Debug Key Revocation
    };

    struct IMAGE_KEY_REVOKE
    {
        uint8_t IMAGE_KEY_REVOKE : 8;  // [7:0] Image Key Revocation
    };

    struct SEC_CFG
    {
        uint8_t Reserved1      : 4;
        uint8_t PUF_ENROLL_DIS : 1;
        uint8_t Reserved       : 2;
        uint8_t DIS_NXP_FW     : 1;
    };

    struct Reserved
    {
        uint8_t reserved : 8;
    };

    DBG_KEY_REVOKE   dbg_key_revoke;
    IMAGE_KEY_REVOKE image_key_revoke;
    SEC_CFG          sec_cfg;
    Reserved         reserved;
};

// OTP Index 3: BOOT_CFG
struct Fuse3
{
    struct BOOT_CFG
    {
        // byte 0
        uint8_t PinSel_SPI_RBoot_Dis : 1;
        uint8_t RBoot_IFR0_Dis       : 1;
        uint8_t RBoot_SPI_Dis        : 1;
        uint8_t RESERVED0            : 1;
        uint8_t RESERVED1            : 1;
        uint8_t Boot_IFR0_Dis        : 1;
        uint8_t Boot_flash_Dis       : 1;
        uint8_t RESERVED2            : 1;
        // byte 1
        uint8_t ISP_UART_Dis         : 1;
        uint8_t ISP_I2C_Dis          : 1;
        uint8_t ISP_SPI_Dis          : 1;
        uint8_t RESERVED5            : 1;
        uint8_t ISP_USB1_Dis         : 1;
        uint8_t ISP_CAN_Dis          : 1;
        uint8_t RESERVED4            : 1;
        uint8_t RESERVED3            : 1;
        // byte 2
        uint8_t BOOT_SPEED           : 2;
        uint8_t RESERVED6            : 1;
        uint8_t RESERVED7            : 1;
        uint8_t RESERVED8            : 1;
        uint8_t RESERVED9            : 1;
        uint8_t ISP_DISABLE          : 2;
        // byte 3
        uint8_t FLASH_REMAP_SZ       : 5;
        uint8_t OEM_BANK1_IFR0_USAGE : 3;
    };
    BOOT_CFG boot_cfg;
};

// OTP Index 6: SEC_BOOT_CFG
struct Fuse6
{
    struct SEC_BOOT_CFG
    {
        // byte 0
        uint8_t RESERVED0       : 1;
        uint8_t RESERVED1       : 1;
        uint8_t RESERVED2       : 1;
        uint8_t LP_SEC_BOOT     : 2;
        uint8_t RESERVED3       : 1;
        uint8_t SEC_BOOT_EN     : 2;
        // byte 1
        uint8_t ACTIVE_IMG_PROT : 2;
        uint8_t ITRC_ZEROIZE    : 2;
        uint8_t ENF_TZM_PRESET  : 2;
        uint8_t ENF_CNSA        : 2;
        // byte 2
        uint8_t FIPS_DRBG_STEN  : 2;
        uint8_t FIPS_ECDSA_STEN : 2;
        uint8_t FIPS_AES_STEN   : 2;
        uint8_t FIPS_SHA_STEN   : 2;
        // byte 3
        uint8_t RESERVED4       : 1;
        uint8_t RESERVED5       : 1;
        uint8_t FIPS_PUF_STEN   : 2;
        uint8_t FIPS_KDF_STEN   : 2;
        uint8_t FIPS_CMAC_STEN  : 2;
    };

    SEC_BOOT_CFG sec_boot_cfg;
};

// OTP Index 7: RotK_USAGE
struct Fuse7
{
    struct ROTK_USAGE
    {
        // byte 0
        uint8_t RoTK0_Usage       : 3;
        uint8_t RoTK1_Usage       : 3;
        uint8_t RoTK2_Usage0      : 2;
        // byte 1
        uint8_t RoTK2_Usage1      : 1;
        uint8_t RoTK3_Usage       : 3;
        uint8_t SKIP_DICE         : 1;
        uint8_t DICE_INC_NXP_CFG  : 1;
        uint8_t DICE_INC_CUST_CFG : 1;
        uint8_t RESERVED0         : 1;
        // byte 2
        uint8_t RESERVED1         : 8;
        // byte 3
        uint8_t NPX_LOCK_CTX0     : 2;
        uint8_t NPX_LOCK_CTX1     : 2;
        uint8_t NPX_LOCK_CTX2     : 2;
        uint8_t NPX_LOCK_CTX3     : 2;
    };

    ROTK_USAGE rotk_usage;
};

static_assert(sizeof(Fuse0) == 4, "Fuse0 size is not correct");
static_assert(sizeof(Fuse1) == 4, "Fuse1 size is not correct");
static_assert(sizeof(Fuse2) == 4, "Fuse2 size is not correct");
static_assert(sizeof(Fuse3) == 4, "Fuse3 size is not correct");
static_assert(sizeof(Fuse6) == 4, "Fuse6 size is not correct");
static_assert(sizeof(Fuse7) == 4, "Fuse7 size is not correct");

}  // namespace sys::otp