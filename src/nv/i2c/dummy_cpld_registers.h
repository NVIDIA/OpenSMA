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

#ifndef PRODUCT_CPLD_REGISTERS_H
#define PRODUCT_CPLD_REGISTERS_H

#include <array>
#include <cstdint>

// ============================================================================
// CPLD Register Table Definitions
// This file contains register address definitions, bit masks, and default
// values for the CPLD interface.
// ============================================================================

namespace Cpld_User_Reg {

constexpr uint8_t USR_REG_ADDR_START = 0;
constexpr uint8_t CPLD_USER_REG_SIZE = 0x9C;

// CL - Indicates the CL of the current release build
constexpr uint8_t  CL_ADDR_START = 0x00;
constexpr uint8_t  CL_ADDR_END   = 0x03;
constexpr uint32_t CL_MASK       = 0xFFFFFFFF;
constexpr uint32_t CL_DEFAULT    = 0;  // DEF_CL

// Firmware Version Registers
constexpr uint8_t FW_MAJOR_VERSION_ADDR    = 0x04;
constexpr uint8_t FW_MAJOR_VERSION_MASK    = 0xFF;
constexpr uint8_t FW_MAJOR_VERSION_DEFAULT = 0;  // DEF_FW_MAJOR_VERSION

constexpr uint8_t FW_MINOR_VERSION_ADDR    = 0x05;
constexpr uint8_t FW_MINOR_VERSION_MASK    = 0xFF;
constexpr uint8_t FW_MINOR_VERSION_DEFAULT = 0;  // DEF_FW_MINOR_VERSION

constexpr uint8_t FW_PATCH_VERSION_ADDR    = 0x06;
constexpr uint8_t FW_PATCH_VERSION_MASK    = 0xFF;
constexpr uint8_t FW_PATCH_VERSION_DEFAULT = 0;  // DEF_FW_PATCH_VERSION

// REGTBL_VERSION - Indicates the version of the REGTBL
constexpr uint8_t  REGTBL_VERSION_ADDR_START = 0x07;
constexpr uint8_t  REGTBL_VERSION_ADDR_END   = 0x08;
constexpr uint16_t REGTBL_VERSION_MASK       = 0xFFFF;
constexpr uint16_t REGTBL_VERSION_DEFAULT    = 0;  // DEF_REGTBL_VERSION

// USER_STRAP
constexpr uint8_t USER_STRAP_ADDR      = 0x09;
constexpr uint8_t USER_STRAP_MASK      = 0xFF;
constexpr uint8_t USER_STRAP_DEFAULT   = 0x06;
constexpr uint8_t FPGA_USER_STRAP_MASK = 0xFF;

// BMC_GPI_SIGNALS
constexpr uint8_t BMC_GPI_SIGNALS_ADDR   = 0x0A;
constexpr uint8_t IOX_PRE_IST_RST_N_BIT  = 2;
constexpr uint8_t IOX_PRE_IST_RST_N_MASK = (1 << IOX_PRE_IST_RST_N_BIT);
constexpr uint8_t SHDN_FORCE_L_BIT       = 1;
constexpr uint8_t SHDN_FORCE_L_MASK      = (1 << SHDN_FORCE_L_BIT);
constexpr uint8_t RUN_POWER_EN_BIT       = 0;
constexpr uint8_t RUN_POWER_EN_MASK      = (1 << RUN_POWER_EN_BIT);

// BMC_GPO_SIGNALS
constexpr uint8_t BMC_GPO_SIGNALS_ADDR = 0x0B;
constexpr uint8_t RUN_POWER_PG_BIT     = 0;
constexpr uint8_t RUN_POWER_PG_MASK    = (1 << RUN_POWER_PG_BIT);

// BOARD_INFO_1
constexpr uint8_t BOARD_INFO_1_ADDR            = 0x0C;
constexpr uint8_t CLK_BUF_27MHZ_EN_BIT         = 6;
constexpr uint8_t CLK_BUF_27MHZ_EN_MASK        = (1 << CLK_BUF_27MHZ_EN_BIT);
constexpr uint8_t MOD1_BIT                     = 5;
constexpr uint8_t MOD1_MASK                    = (1 << MOD1_BIT);
constexpr uint8_t TRAY_FAST_SHDN_L_BIT         = 4;
constexpr uint8_t TRAY_FAST_SHDN_L_MASK        = (1 << TRAY_FAST_SHDN_L_BIT);
constexpr uint8_t CPLD_PWR_BRAKE_L_OUT_BIT     = 3;
constexpr uint8_t CPLD_PWR_BRAKE_L_OUT_MASK    = (1 << CPLD_PWR_BRAKE_L_OUT_BIT);
constexpr uint8_t CPLD_PWR_BRAKE_L_OUT_DEFAULT = (1 << CPLD_PWR_BRAKE_L_OUT_BIT);
constexpr uint8_t NVHS_AND_PCIE_CLK_EN_BIT     = 2;
constexpr uint8_t NVHS_AND_PCIE_CLK_EN_MASK    = (1 << NVHS_AND_PCIE_CLK_EN_BIT);
constexpr uint8_t I2C1_FPGA_ALERT_L_BIT        = 1;
constexpr uint8_t I2C1_FPGA_ALERT_L_MASK       = (1 << I2C1_FPGA_ALERT_L_BIT);
constexpr uint8_t I2C2_FPGA_ALERT_L_BIT        = 0;
constexpr uint8_t I2C2_FPGA_ALERT_L_MASK       = (1 << I2C2_FPGA_ALERT_L_BIT);

// SOCAMM
constexpr uint8_t SOCAMM_ADDR              = 0x0D;
constexpr uint8_t C0_ALL_SOCAMM_PRSNT_BIT  = 7;
constexpr uint8_t C0_ALL_SOCAMM_PRSNT_MASK = (1 << C0_ALL_SOCAMM_PRSNT_BIT);
constexpr uint8_t ALL_C1_SOCAMM_PRSNT_BIT  = 6;
constexpr uint8_t ALL_C1_SOCAMM_PRSNT_MASK = (1 << ALL_C1_SOCAMM_PRSNT_BIT);
constexpr uint8_t C1_5V_SOCAMM_PGOOD_BIT   = 3;
constexpr uint8_t C1_5V_SOCAMM_PGOOD_MASK  = (1 << C1_5V_SOCAMM_PGOOD_BIT);
constexpr uint8_t C1_ALL_SOCAMM_PGOOD_BIT  = 2;
constexpr uint8_t C1_ALL_SOCAMM_PGOOD_MASK = (1 << C1_ALL_SOCAMM_PGOOD_BIT);
constexpr uint8_t C0_5V_SOCAMM_PGOOD_BIT   = 1;
constexpr uint8_t C0_5V_SOCAMM_PGOOD_MASK  = (1 << C0_5V_SOCAMM_PGOOD_BIT);
constexpr uint8_t C0_ALL_SOCAMM_PGOOD_BIT  = 0;
constexpr uint8_t C0_ALL_SOCAMM_PGOOD_MASK = (1 << C0_ALL_SOCAMM_PGOOD_BIT);

// PWR_SEQ_INFO
constexpr uint8_t PWR_SEQ_INFO_ADDR     = 0x0E;
constexpr uint8_t RUN_PGOOD_SYSRST_BIT  = 2;
constexpr uint8_t RUN_PGOOD_SYSRST_MASK = (1 << RUN_PGOOD_SYSRST_BIT);
constexpr uint8_t RUN_PWR_EN_ONCE_BIT   = 1;
constexpr uint8_t RUN_PWR_EN_ONCE_MASK  = (1 << RUN_PWR_EN_ONCE_BIT);
constexpr uint8_t RUN_PWR_PG_ONCE_BIT   = 0;
constexpr uint8_t RUN_PWR_PG_ONCE_MASK  = (1 << RUN_PWR_PG_ONCE_BIT);

// INT_MASK
constexpr uint8_t INT_MASK_ADDR = 0x0F;

// GPU_PGOOD
constexpr uint8_t GPU_PGOOD_ADDR    = 0x10;
constexpr uint8_t GPU_PWR_GOOD_BIT  = 0;
constexpr uint8_t GPU_PWR_GOOD_MASK = (1 << GPU_PWR_GOOD_BIT);

// BMC_CLK_BUF_27MHZ_DIS
constexpr uint8_t BMC_CLK_ADDR               = 0x11;
constexpr uint8_t BMC_CLK_BUF_27MHZ_DIS_BIT  = 1;
constexpr uint8_t BMC_CLK_BUF_27MHZ_DIS_MASK = (1 << BMC_CLK_BUF_27MHZ_DIS_BIT);

// MCU Unlock
constexpr uint8_t MCU_UNLOCK_EN_ADDR_UPPER = 0x00;
constexpr uint8_t MCU_UNLOCK_EN_ADDR_LOWER = 0x11;
constexpr uint8_t MCU_UNLOCK_EN_BIT        = 0;
constexpr uint8_t MCU_UNLOCK_EN_MASK       = (1 << MCU_UNLOCK_EN_BIT);
constexpr uint8_t MCU_UNLOCK_EN_LOCK       = 0;
constexpr uint8_t MCU_UNLOCK_EN_UNLOCK     = 1;

constexpr uint8_t INT_MASK_START_ADDR = 0x12;
constexpr uint8_t INT_MASK_END_ADDR   = 0x15;

// GPU_THERM_OVERT_MASK
constexpr uint8_t GPU_THERM_OVERT_MASK_ADDR    = 0x16;
constexpr uint8_t GPU_THERM_OVERT_MASK_BITS    = 0x03;
constexpr uint8_t GPU_THERM_OVERT_MASK_DEFAULT = 0x00;

// CPU_THERM_OVERT_MASK
constexpr uint8_t CPU_THERM_OVERT_MASK_ADDR    = 0x17;
constexpr uint8_t CPU_THERM_OVERT_MASK_BITS    = 0x03;
constexpr uint8_t CPU_THERM_OVERT_MASK_DEFAULT = 0x00;

// REGTBL_FSM_ERR_MASK
constexpr uint8_t REGTBL_FSM_ERR_MASK_ADDR    = 0x1A;
constexpr uint8_t REGTBL_FSM_ERR_MASK_DEFAULT = 0x00;

// I2C0_PERI_ERROR_MASK
constexpr uint8_t I2C0_PERI_ERROR_MASK_ADDR    = 0x1D;
constexpr uint8_t I2C0_PERI_ERROR_MASK_DEFAULT = 0x00;

// I2C2_PERI_ERROR_MASK
constexpr uint8_t I2C2_PERI_ERROR_MASK_ADDR    = 0x1E;
constexpr uint8_t I2C2_PERI_ERROR_MASK_DEFAULT = 0x00;

// GPU_THERM_OVERT_INT
constexpr uint8_t GPU_THERM_OVERT_INT_ADDR    = 0x21;
constexpr uint8_t GPU_THERM_OVERT_INT_BITS    = 0x03;
constexpr uint8_t GPU_THERM_OVERT_INT_DEFAULT = 0x00;

// CPU_THERM_OVERT_INT
constexpr uint8_t CPU_THERM_OVERT_INT_ADDR    = 0x22;
constexpr uint8_t CPU_THERM_OVERT_INT_BITS    = 0x03;
constexpr uint8_t CPU_THERM_OVERT_INT_DEFAULT = 0x00;

// REGTBL_FSM_ERR_INT
constexpr uint8_t REGTBL_FSM_ERR_INT_ADDR    = 0x25;
constexpr uint8_t REGTBL_FSM_ERR_INT_DEFAULT = 0x00;

// I2C0_PERI_ERROR_INT
constexpr uint8_t I2C0_PERI_ERROR_INT_ADDR    = 0x28;
constexpr uint8_t I2C0_PERI_ERROR_INT_DEFAULT = 0x00;

// I2C2_PERI_ERROR_INT
constexpr uint8_t I2C2_PERI_ERROR_INT_ADDR    = 0x29;
constexpr uint8_t I2C2_PERI_ERROR_INT_DEFAULT = 0x00;

// CPU_POWER_BRAKE
constexpr uint8_t CPU_POWER_BRAKE_ADDR    = 0x2C;
constexpr uint8_t CPU_POWER_BRAKE_MASK    = 0x03;
constexpr uint8_t CPU_POWER_BRAKE_DEFAULT = 0x00;

// PWRSEQ_SUMMARY
constexpr uint8_t PWRSEQ_SUMMARY_ADDR          = 0x40;
constexpr uint8_t PWR_FAIL_SYS_BIT             = 5;
constexpr uint8_t PWR_FAIL_SYS_MASK            = (1 << PWR_FAIL_SYS_BIT);
constexpr uint8_t PWR_FAIL_RUN_TIME_BIT        = 4;
constexpr uint8_t PWR_FAIL_RUN_TIME_MASK       = (1 << PWR_FAIL_RUN_TIME_BIT);
constexpr uint8_t PWR_FAIL_POWER_ON_BIT        = 3;
constexpr uint8_t PWR_FAIL_POWER_ON_MASK       = (1 << PWR_FAIL_POWER_ON_BIT);
constexpr uint8_t PWR_FAIL_TRAY_FAST_SHDN_BIT  = 2;
constexpr uint8_t PWR_FAIL_TRAY_FAST_SHDN_MASK = (1 << PWR_FAIL_TRAY_FAST_SHDN_BIT);
constexpr uint8_t PWR_FAIL_GPU_BIT             = 1;
constexpr uint8_t PWR_FAIL_GPU_MASK            = (1 << PWR_FAIL_GPU_BIT);
constexpr uint8_t PWR_FAIL_CPU_BIT             = 0;
constexpr uint8_t PWR_FAIL_CPU_MASK            = (1 << PWR_FAIL_CPU_BIT);

// PWRSEQ_STATE
constexpr uint8_t PWRSEQ_STATE_ADDR    = 0x41;
constexpr uint8_t PWRSEQ_STATE_MASK    = 0x1F;
constexpr uint8_t PWRSEQ_STATE_DEFAULT = 0x00;

// PWRSEQ_FAIL_STATE
constexpr uint8_t PWRSEQ_FAIL_STATE_ADDR    = 0x42;
constexpr uint8_t PWRSEQ_FAIL_STATE_MASK    = 0x1F;
constexpr uint8_t PWRSEQ_FAIL_STATE_DEFAULT = 0x00;

// PWRSEQ_GPU_STATE
constexpr uint8_t PWRSEQ_GPU_STATE_ADDR       = 0x43;
constexpr uint8_t PWRSEQ_GPU_FAIL_STATE_SHIFT = 4;
constexpr uint8_t PWRSEQ_GPU_FAIL_STATE_MASK  = 0xF0;
constexpr uint8_t PWRSEQ_GPU_STATE_MASK       = 0x0F;
constexpr uint8_t PWRSEQ_GPU_STATE_DEFAULT    = 0x00;

// PWRSEQ_SYS_RST_STATE
constexpr uint8_t PWRSEQ_SYS_RST_STATE_ADDR    = 0x44;
constexpr uint8_t PWRSEQ_SYS_RST_STATE_MASK    = 0x07;
constexpr uint8_t PWRSEQ_SYS_RST_STATE_DEFAULT = 0x00;

// PWRSEQ_FAIL_0
constexpr uint8_t PWRSEQ_FAIL_0_ADDR                       = 0x45;
constexpr uint8_t PWR_FAIL_STBY_12V_INPUT_VALID_IN_3V3_BIT = 7;
constexpr uint8_t
    PWR_FAIL_STBY_12V_INPUT_VALID_IN_3V3_MASK = (1 << PWR_FAIL_STBY_12V_INPUT_VALID_IN_3V3_BIT);
constexpr uint8_t PWR_FAIL_IOX_STBY_PWR_BIT   = 6;
constexpr uint8_t PWR_FAIL_IOX_STBY_PWR_MASK  = (1 << PWR_FAIL_IOX_STBY_PWR_BIT);
constexpr uint8_t PWR_FAIL_HSC_PG_3V3_BIT     = 5;
constexpr uint8_t PWR_FAIL_HSC_PG_3V3_MASK    = (1 << PWR_FAIL_HSC_PG_3V3_BIT);
constexpr uint8_t PWR_FAIL_12V_HSCC_BIT       = 4;
constexpr uint8_t PWR_FAIL_12V_HSCC_MASK      = (1 << PWR_FAIL_12V_HSCC_BIT);
constexpr uint8_t PWR_FAIL_INPUT_12V_VALID_BIT  = 3;
constexpr uint8_t PWR_FAIL_INPUT_12V_VALID_MASK = (1 << PWR_FAIL_INPUT_12V_VALID_BIT);
constexpr uint8_t PWR_FAIL_C0_5V_SOCAMM_BIT     = 2;
constexpr uint8_t PWR_FAIL_C0_5V_SOCAMM_MASK    = (1 << PWR_FAIL_C0_5V_SOCAMM_BIT);
constexpr uint8_t PWR_FAIL_VR_VCC_BIT           = 1;
constexpr uint8_t PWR_FAIL_VR_VCC_MASK          = (1 << PWR_FAIL_VR_VCC_BIT);
constexpr uint8_t PWR_FAIL_VDD_3V3_BIT          = 0;
constexpr uint8_t PWR_FAIL_VDD_3V3_MASK         = (1 << PWR_FAIL_VDD_3V3_BIT);

// PWRSEQ_FAIL_1
constexpr uint8_t PWRSEQ_FAIL_1_ADDR              = 0x46;
constexpr uint8_t PWR_FAIL_C0_1V2_BIT             = 7;
constexpr uint8_t PWR_FAIL_C0_1V2_MASK            = (1 << PWR_FAIL_C0_1V2_BIT);
constexpr uint8_t PWR_FAIL_GPU1_PS_HVDD_BIT       = 6;
constexpr uint8_t PWR_FAIL_GPU1_PS_HVDD_MASK      = (1 << PWR_FAIL_GPU1_PS_HVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_HVDD_BIT       = 5;
constexpr uint8_t PWR_FAIL_GPU2_PS_HVDD_MASK      = (1 << PWR_FAIL_GPU2_PS_HVDD_BIT);
constexpr uint8_t PWR_FAIL_VDD_1V8_3V3_LEVEL_BIT  = 4;
constexpr uint8_t PWR_FAIL_VDD_1V8_3V3_LEVEL_MASK = (1 << PWR_FAIL_VDD_1V8_3V3_LEVEL_BIT);
constexpr uint8_t PWR_FAIL_VDDQPS_BIT             = 3;
constexpr uint8_t PWR_FAIL_VDDQPS_MASK            = (1 << PWR_FAIL_VDDQPS_BIT);
constexpr uint8_t PWR_FAIL_C0_SOCVDD_BIT          = 2;
constexpr uint8_t PWR_FAIL_C0_SOCVDD_MASK         = (1 << PWR_FAIL_C0_SOCVDD_BIT);
constexpr uint8_t PWR_FAIL_C0_CPUVDD_BIT          = 1;
constexpr uint8_t PWR_FAIL_C0_CPUVDD_MASK         = (1 << PWR_FAIL_C0_CPUVDD_BIT);
constexpr uint8_t PWR_FAIL_C2CVDD_BIT             = 0;
constexpr uint8_t PWR_FAIL_C2CVDD_MASK            = (1 << PWR_FAIL_C2CVDD_BIT);

// PWRSEQ_FAIL_2
constexpr uint8_t PWRSEQ_FAIL_2_ADDR              = 0x47;
constexpr uint8_t PWR_FAIL_C0_C2CLPI_BIT          = 7;
constexpr uint8_t PWR_FAIL_C0_C2CLPI_MASK         = (1 << PWR_FAIL_C0_C2CLPI_BIT);
constexpr uint8_t PWR_FAIL_C0_C2CLLI_BIT          = 6;
constexpr uint8_t PWR_FAIL_C0_C2CLLI_MASK         = (1 << PWR_FAIL_C0_C2CLLI_BIT);
constexpr uint8_t PWR_FAIL_C0_MSVDD_BIT           = 5;
constexpr uint8_t PWR_FAIL_C0_MSVDD_MASK          = (1 << PWR_FAIL_C0_MSVDD_BIT);
constexpr uint8_t PWR_FAIL_PS_NVHS_CVDD_BIT       = 4;
constexpr uint8_t PWR_FAIL_PS_NVHS_CVDD_MASK      = (1 << PWR_FAIL_PS_NVHS_CVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_NVHS_CVDD_BIT  = 3;
constexpr uint8_t PWR_FAIL_GPU2_PS_NVHS_CVDD_MASK = (1 << PWR_FAIL_GPU2_PS_NVHS_CVDD_BIT);
constexpr uint8_t PWR_FAIL_PS_NVHS_AVDD_BIT       = 2;
constexpr uint8_t PWR_FAIL_PS_NVHS_AVDD_MASK      = (1 << PWR_FAIL_PS_NVHS_AVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_NVHS_AVDD_BIT  = 1;
constexpr uint8_t PWR_FAIL_GPU2_PS_NVHS_AVDD_MASK = (1 << PWR_FAIL_GPU2_PS_NVHS_AVDD_BIT);
constexpr uint8_t PWR_FAIL_PS_SYSVDD_BIT          = 0;
constexpr uint8_t PWR_FAIL_PS_SYSVDD_MASK         = (1 << PWR_FAIL_PS_SYSVDD_BIT);

// PWRSEQ_FAIL_3
constexpr uint8_t PWRSEQ_FAIL_3_ADDR                 = 0x48;
constexpr uint8_t PWR_FAIL_GPU2_PS_SYSVDD_BIT        = 7;
constexpr uint8_t PWR_FAIL_GPU2_PS_SYSVDD_MASK       = (1 << PWR_FAIL_GPU2_PS_SYSVDD_BIT);
constexpr uint8_t PWR_FAIL_PS_GA_MSVDD_BIT           = 6;
constexpr uint8_t PWR_FAIL_PS_GA_MSVDD_MASK          = (1 << PWR_FAIL_PS_GA_MSVDD_BIT);
constexpr uint8_t PWR_FAIL_PS_GB_MSVDD_BIT           = 5;
constexpr uint8_t PWR_FAIL_PS_GB_MSVDD_MASK          = (1 << PWR_FAIL_PS_GB_MSVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_GA_MSVDD_BIT      = 4;
constexpr uint8_t PWR_FAIL_GPU2_PS_GA_MSVDD_MASK     = (1 << PWR_FAIL_GPU2_PS_GA_MSVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_GB_MSVDD_BIT      = 3;
constexpr uint8_t PWR_FAIL_GPU2_PS_GB_MSVDD_MASK     = (1 << PWR_FAIL_GPU2_PS_GB_MSVDD_BIT);
constexpr uint8_t PWR_FAIL_PS_HBIN_PEX_VDD_BIT       = 2;
constexpr uint8_t PWR_FAIL_PS_HBIN_PEX_VDD_MASK      = (1 << PWR_FAIL_PS_HBIN_PEX_VDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_HBIN_PEX_VDD_BIT  = 1;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBIN_PEX_VDD_MASK = (1 << PWR_FAIL_GPU2_PS_HBIN_PEX_VDD_BIT);
constexpr uint8_t PWR_FAIL_PS_GA_NVVDD_BIT           = 0;
constexpr uint8_t PWR_FAIL_PS_GA_NVVDD_MASK          = (1 << PWR_FAIL_PS_GA_NVVDD_BIT);

// PWRSEQ_FAIL_4
constexpr uint8_t PWRSEQ_FAIL_4_ADDR               = 0x49;
constexpr uint8_t PWR_FAIL_PS_GB_NVVDD_BIT         = 7;
constexpr uint8_t PWR_FAIL_PS_GB_NVVDD_MASK        = (1 << PWR_FAIL_PS_GB_NVVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_GA_NVVDD_BIT    = 6;
constexpr uint8_t PWR_FAIL_GPU2_PS_GA_NVVDD_MASK   = (1 << PWR_FAIL_GPU2_PS_GA_NVVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_GB_NVVDD_BIT    = 5;
constexpr uint8_t PWR_FAIL_GPU2_PS_GB_NVVDD_MASK   = (1 << PWR_FAIL_GPU2_PS_GB_NVVDD_BIT);
constexpr uint8_t PWR_FAIL_PS_HBI_GS_VDD_BIT       = 4;
constexpr uint8_t PWR_FAIL_PS_HBI_GS_VDD_MASK      = (1 << PWR_FAIL_PS_HBI_GS_VDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_HBI_GS_VDD_BIT  = 3;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBI_GS_VDD_MASK = (1 << PWR_FAIL_GPU2_PS_HBI_GS_VDD_BIT);
constexpr uint8_t PWR_FAIL_GPU1_PS_C2CVDD_BIT      = 2;
constexpr uint8_t PWR_FAIL_GPU1_PS_C2CVDD_MASK     = (1 << PWR_FAIL_GPU1_PS_C2CVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_C2CVDD_BIT      = 1;
constexpr uint8_t PWR_FAIL_GPU2_PS_C2CVDD_MASK     = (1 << PWR_FAIL_GPU2_PS_C2CVDD_BIT);
constexpr uint8_t PWR_FAIL_PS_HBMVPP_BIT           = 0;
constexpr uint8_t PWR_FAIL_PS_HBMVPP_MASK          = (1 << PWR_FAIL_PS_HBMVPP_BIT);

// PWRSEQ_FAIL_5
constexpr uint8_t PWRSEQ_FAIL_5_ADDR              = 0x4A;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVPP_BIT     = 7;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVPP_MASK    = (1 << PWR_FAIL_GPU2_PS_HBMVPP_BIT);
constexpr uint8_t PWR_FAIL_C0_CVDD_BIT            = 6;
constexpr uint8_t PWR_FAIL_C0_CVDD_MASK           = (1 << PWR_FAIL_C0_CVDD_BIT);
constexpr uint8_t PWR_FAIL_C0_VDDP_BIT            = 5;
constexpr uint8_t PWR_FAIL_C0_VDDP_MASK           = (1 << PWR_FAIL_C0_VDDP_BIT);
constexpr uint8_t PWR_FAIL_C0_ALL_SOCAMM_BIT      = 4;
constexpr uint8_t PWR_FAIL_C0_ALL_SOCAMM_MASK     = (1 << PWR_FAIL_C0_ALL_SOCAMM_BIT);
constexpr uint8_t PWR_FAIL_C0_DVDD_BIT            = 3;
constexpr uint8_t PWR_FAIL_C0_DVDD_MASK           = (1 << PWR_FAIL_C0_DVDD_BIT);
constexpr uint8_t PWR_FAIL_DDR_VDDQ_BIT           = 2;
constexpr uint8_t PWR_FAIL_DDR_VDDQ_MASK          = (1 << PWR_FAIL_DDR_VDDQ_BIT);
constexpr uint8_t PWR_FAIL_PS_NVHS_DVDD_BIT       = 1;
constexpr uint8_t PWR_FAIL_PS_NVHS_DVDD_MASK      = (1 << PWR_FAIL_PS_NVHS_DVDD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_NVHS_DVDD_BIT  = 0;
constexpr uint8_t PWR_FAIL_GPU2_PS_NVHS_DVDD_MASK = (1 << PWR_FAIL_GPU2_PS_NVHS_DVDD_BIT);

// PWRSEQ_FAIL_6
constexpr uint8_t PWRSEQ_FAIL_6_ADDR                  = 0x4B;
constexpr uint8_t PWR_FAIL_PS_HBMVDDQC_ABCD_BIT       = 7;
constexpr uint8_t PWR_FAIL_PS_HBMVDDQC_ABCD_MASK      = (1 << PWR_FAIL_PS_HBMVDDQC_ABCD_BIT);
constexpr uint8_t PWR_FAIL_PS_HBMVDDQC_CDAB_BIT       = 6;
constexpr uint8_t PWR_FAIL_PS_HBMVDDQC_CDAB_MASK      = (1 << PWR_FAIL_PS_HBMVDDQC_CDAB_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQC_ABCD_BIT  = 5;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQC_ABCD_MASK = (1
                                                         << PWR_FAIL_GPU2_PS_HBMVDDQC_ABCD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQC_CDAB_BIT  = 4;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQC_CDAB_MASK = (1
                                                         << PWR_FAIL_GPU2_PS_HBMVDDQC_CDAB_BIT);
constexpr uint8_t PWR_FAIL_PS_HBMVDDQR_ABCD_BIT       = 3;
constexpr uint8_t PWR_FAIL_PS_HBMVDDQR_ABCD_MASK      = (1 << PWR_FAIL_PS_HBMVDDQR_ABCD_BIT);
constexpr uint8_t PWR_FAIL_PS_HBMVDDQR_CDAB_BIT       = 2;
constexpr uint8_t PWR_FAIL_PS_HBMVDDQR_CDAB_MASK      = (1 << PWR_FAIL_PS_HBMVDDQR_CDAB_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQR_ABCD_BIT  = 1;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQR_ABCD_MASK = (1
                                                         << PWR_FAIL_GPU2_PS_HBMVDDQR_ABCD_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQR_CDAB_BIT  = 0;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQR_CDAB_MASK = (1
                                                         << PWR_FAIL_GPU2_PS_HBMVDDQR_CDAB_BIT);

// PWRSEQ_FAIL_7
constexpr uint8_t PWRSEQ_FAIL_7_ADDR              = 0x4C;
constexpr uint8_t PWR_FAIL_PS_NVHS_AVCC_BIT       = 7;
constexpr uint8_t PWR_FAIL_PS_NVHS_AVCC_MASK      = (1 << PWR_FAIL_PS_NVHS_AVCC_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_NVHS_AVCC_BIT  = 6;
constexpr uint8_t PWR_FAIL_GPU2_PS_NVHS_AVCC_MASK = (1 << PWR_FAIL_GPU2_PS_NVHS_AVCC_BIT);
constexpr uint8_t PWR_FAIL_PS_HBMVDDQL_BIT        = 5;
constexpr uint8_t PWR_FAIL_PS_HBMVDDQL_MASK       = (1 << PWR_FAIL_PS_HBMVDDQL_BIT);
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQL_BIT   = 4;
constexpr uint8_t PWR_FAIL_GPU2_PS_HBMVDDQL_MASK  = (1 << PWR_FAIL_GPU2_PS_HBMVDDQL_BIT);

// THERM_FAIL
constexpr uint8_t THERM_FAIL_ADDR                 = 0x4D;
constexpr uint8_t PWR_FAIL_CPU_THERM_OVERT_SHIFT  = 4;
constexpr uint8_t PWR_FAIL_CPU_THERM_OVERT_MASK   = 0x70;
constexpr uint8_t PWR_FAIL_GPU2_THERM_OVERT_SHIFT = 2;
constexpr uint8_t PWR_FAIL_GPU2_THERM_OVERT_MASK  = 0x0C;
constexpr uint8_t PWR_FAIL_GPU1_THERM_OVERT_SHIFT = 0;
constexpr uint8_t PWR_FAIL_GPU1_THERM_OVERT_MASK  = 0x03;

// PWRSEQ_FAIL_8
constexpr uint8_t PWRSEQ_FAIL_8_ADDR          = 0x4E;
constexpr uint8_t PWR_FAIL_C1_1V2_BIT         = 7;
constexpr uint8_t PWR_FAIL_C1_1V2_MASK        = (1 << PWR_FAIL_C1_1V2_BIT);
constexpr uint8_t PWR_FAIL_C1_SOCVDD_BIT      = 6;
constexpr uint8_t PWR_FAIL_C1_SOCVDD_MASK     = (1 << PWR_FAIL_C1_SOCVDD_BIT);
constexpr uint8_t PWR_FAIL_C1_CPUVDD_BIT      = 5;
constexpr uint8_t PWR_FAIL_C1_CPUVDD_MASK     = (1 << PWR_FAIL_C1_CPUVDD_BIT);
constexpr uint8_t PWR_FAIL_C1_C2CVDD_BIT      = 4;
constexpr uint8_t PWR_FAIL_C1_C2CVDD_MASK     = (1 << PWR_FAIL_C1_C2CVDD_BIT);
constexpr uint8_t PWR_FAIL_C1_C2CLPI_BIT      = 3;
constexpr uint8_t PWR_FAIL_C1_C2CLPI_MASK     = (1 << PWR_FAIL_C1_C2CLPI_BIT);
constexpr uint8_t PWR_FAIL_C1_C2CLLI_BIT      = 2;
constexpr uint8_t PWR_FAIL_C1_C2CLLI_MASK     = (1 << PWR_FAIL_C1_C2CLLI_BIT);
constexpr uint8_t PWR_FAIL_C1_MSVDD_BIT       = 1;
constexpr uint8_t PWR_FAIL_C1_MSVDD_MASK      = (1 << PWR_FAIL_C1_MSVDD_BIT);
constexpr uint8_t PWR_FAIL_C1_ALL_SOCAMM_BIT  = 0;
constexpr uint8_t PWR_FAIL_C1_ALL_SOCAMM_MASK = (1 << PWR_FAIL_C1_ALL_SOCAMM_BIT);

// PWRSEQ_FAIL_9
constexpr uint8_t PWRSEQ_FAIL_9_ADDR         = 0x4F;
constexpr uint8_t PWR_FAIL_C1_5V_SOCAMM_BIT  = 4;
constexpr uint8_t PWR_FAIL_C1_5V_SOCAMM_MASK = (1 << PWR_FAIL_C1_5V_SOCAMM_BIT);
constexpr uint8_t PWR_FAIL_C1_CVDD_BIT       = 3;
constexpr uint8_t PWR_FAIL_C1_CVDD_MASK      = (1 << PWR_FAIL_C1_CVDD_BIT);
constexpr uint8_t PWR_FAIL_C1_VDDP_BIT       = 2;
constexpr uint8_t PWR_FAIL_C1_VDDP_MASK      = (1 << PWR_FAIL_C1_VDDP_BIT);
constexpr uint8_t PWR_FAIL_C1_DVDD_BIT       = 1;
constexpr uint8_t PWR_FAIL_C1_DVDD_MASK      = (1 << PWR_FAIL_C1_DVDD_BIT);
constexpr uint8_t PWR_FAIL_C1_DDR_VDDQ_BIT   = 0;
constexpr uint8_t PWR_FAIL_C1_DDR_VDDQ_MASK  = (1 << PWR_FAIL_C1_DDR_VDDQ_BIT);

// ============================================================================
// INJECTION CONTROL AND DEBUG Category
// ============================================================================
// INJ_ADDR
constexpr uint8_t  INJ_ADDR_START   = 0x80;
constexpr uint8_t  INJ_ADDR_END     = 0x81;
constexpr uint16_t INJ_ADDR_MASK    = 0xFFFF;
constexpr uint16_t INJ_ADDR_DEFAULT = 0x0000;

// INJ_DATA
constexpr uint8_t INJ_DATA_ADDR    = 0x82;
constexpr uint8_t INJ_DATA_MASK    = 0xFF;
constexpr uint8_t INJ_DATA_DEFAULT = 0x00;

// INJ_CTRL
constexpr uint8_t INJ_CTRL_ADDR       = 0x83;
constexpr uint8_t INJ_RESET_BIT       = 2;
constexpr uint8_t INJ_RESET_MASK      = (1 << INJ_RESET_BIT);
constexpr uint8_t INJ_EN_BIT          = 1;
constexpr uint8_t INJ_EN_MASK         = (1 << INJ_EN_BIT);
constexpr uint8_t INJ_DATA_WR_EN_BIT  = 0;
constexpr uint8_t INJ_DATA_WR_EN_MASK = (1 << INJ_DATA_WR_EN_BIT);

// TIMER1_START
constexpr uint8_t  TIMER1_START_ADDR_START = 0x84;
constexpr uint8_t  TIMER1_START_ADDR_END   = 0x87;
constexpr uint32_t TIMER1_START_DEFAULT    = 0x00000000;

// TIMER1_PULSE
constexpr uint8_t  TIMER1_PULSE_ADDR_START = 0x88;
constexpr uint8_t  TIMER1_PULSE_ADDR_END   = 0x8B;
constexpr uint32_t TIMER1_PULSE_DEFAULT    = 0x00000000;

// TIMER1_END
constexpr uint8_t  TIMER1_END_ADDR_START = 0x8C;
constexpr uint8_t  TIMER1_END_ADDR_END   = 0x8F;
constexpr uint32_t TIMER1_END_DEFAULT    = 0x00000000;

// TIMER1_PULSE_CNT
constexpr uint8_t  TIMER1_PULSE_CNT_ADDR_START = 0x90;
constexpr uint8_t  TIMER1_PULSE_CNT_ADDR_END   = 0x91;
constexpr uint16_t TIMER1_PULSE_CNT_DEFAULT    = 0x0000;

// TIMER1_CTRL
constexpr uint8_t TIMER1_CTRL_ADDR    = 0x92;
constexpr uint8_t TIMER1_CTRL_MASK    = 0xFF;
constexpr uint8_t TIMER1_CTRL_DEFAULT = 0x00;

// TIMER_GBL_CONTROL
constexpr uint8_t TIMER_GBL_CONTROL_ADDR  = 0x95;
constexpr uint8_t TIMER1_PULSE_START_BIT  = 0;
constexpr uint8_t TIMER1_PULSE_START_MASK = (1 << TIMER1_PULSE_START_BIT);

// REGTBL_FSM_ERROR_CODE
constexpr uint8_t REGTBL_FSM_ERROR_CODE_ADDR    = 0x96;
constexpr uint8_t REGTBL_FSM_ERROR_CODE_MASK    = 0xFF;
constexpr uint8_t REGTBL_FSM_ERROR_CODE_DEFAULT = 0x00;

// REGTBL_FSM_ERROR_ADDR
constexpr uint8_t REGTBL_FSM_ERROR_ADDR_ADDR    = 0x97;
constexpr uint8_t REGTBL_FSM_ERROR_ADDR_MASK    = 0xFF;
constexpr uint8_t REGTBL_FSM_ERROR_ADDR_DEFAULT = 0x00;

// MCU_FWD_I2C0_PERI_ERROR_ADDR
constexpr uint8_t MCU_FWD_I2C0_PERI_ERROR_ADDR_ADDR    = 0x98;
constexpr uint8_t MCU_FWD_I2C0_PERI_ERROR_ADDR_MASK    = 0xFF;
constexpr uint8_t MCU_FWD_I2C0_PERI_ERROR_ADDR_DEFAULT = 0x00;

// MCU_FWD_I2C0_PERI_ERROR_CODE
constexpr uint8_t MCU_FWD_I2C0_PERI_ERROR_CODE_ADDR    = 0x99;
constexpr uint8_t MCU_FWD_I2C0_PERI_ERROR_CODE_MASK    = 0xFF;
constexpr uint8_t MCU_FWD_I2C0_PERI_ERROR_CODE_DEFAULT = 0x00;

// HMC_FWD_I2C2_PERI_ERROR_ADDR
constexpr uint8_t HMC_FWD_I2C2_PERI_ERROR_ADDR_ADDR    = 0x9A;
constexpr uint8_t HMC_FWD_I2C2_PERI_ERROR_ADDR_MASK    = 0xFF;
constexpr uint8_t HMC_FWD_I2C2_PERI_ERROR_ADDR_DEFAULT = 0x00;

// HMC_FWD_I2C2_PERI_ERROR_CODE
constexpr uint8_t HMC_FWD_I2C2_PERI_ERROR_CODE_ADDR    = 0x9B;
constexpr uint8_t HMC_FWD_I2C2_PERI_ERROR_CODE_MASK    = 0xFF;
constexpr uint8_t HMC_FWD_I2C2_PERI_ERROR_CODE_DEFAULT = 0x00;

}  // namespace Cpld_User_Reg

namespace Cpld_Feature_Row {
constexpr std::array<uint8_t, 8> EXPECTED_FEATURE_ROW = {
    0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00};
constexpr std::array<uint8_t, 2> EXPECTED_FEABITS = {0x02, 0x00};
}  // namespace Cpld_Feature_Row

#endif  // PRODUCT_CPLD_REGISTERS_H
