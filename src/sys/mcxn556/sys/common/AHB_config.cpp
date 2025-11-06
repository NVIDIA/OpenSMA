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

#include "AHB_config.h"
#include "fsl_common.h"
#ifdef CPU_MCXN547VDF
#include "MCXN547_cm33_core1.h"
#elif defined(CPU_MCXN556SCDF)
#include "MCXN556S_cm33_core1.h"
#endif
#include <limits>
#include "fsl_edma.h"

// Declare the linker symbols as global extern "C"
extern "C" {
extern uint32_t __shared_memory_start__;
extern uint32_t __shared_memory_end__;
extern uint32_t __core1_image_start__;
extern uint32_t __core1_image_end__;
extern uint32_t __core1_ram_start__;
extern uint32_t __core1_ram_end__;

// Declare DMA config structures from peripherals.c
extern edma_config_t DMA0_config;
extern edma_config_t DMA1_config;
}

namespace sys::common {

// AHB configuration constants
constexpr uint32_t AHB_FLASH_BLOCK_SIZE   = 0x8000U;  // 32KB per flash rule
constexpr uint32_t AHB_RAM_BLOCK_SIZE     = 0x1000U;  // 4KB per RAM rule
constexpr uint32_t AHB_BITS_PER_RULE      = 4U;       // 4 bits per rule
constexpr uint32_t AHB_REGISTER_SIZE_BITS = std::numeric_limits<uint32_t>::digits;
constexpr uint32_t AHB_RULE_MASK          = (AHB_REGISTER_SIZE_BITS / AHB_BITS_PER_RULE)
                                 - 1U;                                    // 8
                                                                          // rules
                                                                          // per
                                                                          // register
                                                                          // (0-7)
constexpr uint32_t AHB_RULE_VALUE_MASK = (1U << AHB_BITS_PER_RULE) - 1U;  // 4-bit mask

constexpr uint32_t align_down(uint32_t value, uint32_t alignment)
{
    // coverity[cert_int30_c_violation] dont care about wrap
    return value & ~(alignment - 1U);
}

constexpr uint32_t align_up(uint32_t value, uint32_t alignment)
{
    // coverity[cert_int30_c_violation] dont care about wrap
    return (value + alignment - 1U) & ~(alignment - 1U);
}

struct MemRuleRegion
{
    uint32_t           start;
    uint32_t           end;
    volatile uint32_t* reg_addr;  // Store register address
};

// Flash rule table for Core1 text
static const MemRuleRegion flash_rule_table[] = {
    {0x10000000U, 0x10040000U, &AHBSC->FLASH00_MEM_RULE[0]},
    {0x10040000U, 0x10080000U, &AHBSC->FLASH00_MEM_RULE[1]},
};

// RAM rule table for shared memory and Core1 RAM
static const MemRuleRegion ram_rule_table[] = {
    {0x30000000U, 0x30008000U,    &AHBSC->RAMA_MEM_RULE}, // RAMA: 0x30000000-0x30007FFF
    {0x30008000U, 0x30010000U,    &AHBSC->RAMB_MEM_RULE}, // RAMB: 0x30008000-0x3000FFFF
    {0x30010000U, 0x30018000U, &AHBSC->RAMC_MEM_RULE[0]}, // RAMC[0]: 0x30010000-0x30017FFF
    {0x30018000U, 0x30020000U, &AHBSC->RAMC_MEM_RULE[1]}, // RAMC[1]: 0x30018000-0x3001FFFF
    {0x30020000U, 0x30028000U, &AHBSC->RAMD_MEM_RULE[0]}, // RAMD[0]: 0x30020000-0x30027FFF
    {0x30028000U, 0x30030000U, &AHBSC->RAMD_MEM_RULE[1]}, // RAMD[1]: 0x30028000-0x3002FFFF
    {0x30030000U, 0x30038000U, &AHBSC->RAME_MEM_RULE[0]}, // RAME[0]: 0x30030000-0x30037FFF
    {0x30038000U, 0x30040000U, &AHBSC->RAME_MEM_RULE[1]}, // RAME[1]: 0x30038000-0x3003FFFF
    {0x30040000U, 0x30048000U, &AHBSC->RAMF_MEM_RULE[0]}, // RAMF[0]: 0x30040000-0x30047FFF
    {0x30048000U, 0x30050000U, &AHBSC->RAMF_MEM_RULE[1]}, // RAMF[1]: 0x30048000-0x3004FFFF
    {0x30050000U, 0x30058000U, &AHBSC->RAMG_MEM_RULE[0]}, // RAMG[0]: 0x30050000-0x30057FFF
    {0x30058000U, 0x30060000U, &AHBSC->RAMG_MEM_RULE[1]}, // RAMG[1]: 0x30058000-0x3005FFFF
    {0x30060000U, 0x30068000U,    &AHBSC->RAMH_MEM_RULE}, // RAMH: 0x30060000-0x30067FFF
};

struct HardwareRegisterConfig
{
    volatile uint32_t* reg_addr;    // Register address pointer
    uint32_t           bit_offset;  // Bit offset within register
    uint32_t           bit_mask;    // Bit mask (0xF for 4-bit, 0x3 for 2-bit)
    const char*        peripheral;  // Peripheral name for debugging
};

// Unified RAM configuration function for both shared memory and Core1 RAM
// coverity[declared_but_not_referenced] - reference in dual core
static void ConfigureRulesAccess(uint32_t start_addr, uint32_t end_addr, uint32_t block_size)
{
    const uint32_t AHB_RULE_BLOCK_SIZE  = block_size;
    const uint32_t AHB_RULE_BLOCK_SHIFT = __builtin_ctz(block_size);  // Calculate shift from
                                                                      // block size

    const MemRuleRegion* rule_table = nullptr;
    size_t               table_size = 0;

    if (block_size == AHB_RAM_BLOCK_SIZE) {
        rule_table = ram_rule_table;
        table_size = sizeof(ram_rule_table) / sizeof(ram_rule_table[0]);
    }
    else if (block_size == AHB_FLASH_BLOCK_SIZE) {
        rule_table = flash_rule_table;
        table_size = sizeof(flash_rule_table) / sizeof(flash_rule_table[0]);
    }

    // coverity[cert_int30_c_violation] - will not wrap
    for (uint32_t addr = start_addr; addr < end_addr; addr += AHB_RULE_BLOCK_SIZE) {
        for (size_t i = 0; i < table_size; ++i) {
            const auto& region = rule_table[i];
            if (addr >= region.start && addr < region.end) {
                uint32_t rule_index  = (addr - region.start) >> AHB_RULE_BLOCK_SHIFT;
                uint32_t rule_offset = rule_index & AHB_RULE_MASK;
                uint32_t rule_shift  = rule_offset * AHB_BITS_PER_RULE;

                // Access register directly
                *region.reg_addr &= ~(AHB_RULE_VALUE_MASK << rule_shift);  // Clear the 4-bit
                                                                           // rule
                *region.reg_addr |= static_cast<uint32_t>(AHBAccessLevel::NonSecureUser)
                                 << rule_shift;  // Set new rule value
                break;                           // Region matched, no need to check further
            }
        }
    }
}

static void USBConfig(nv::ipc::CoreId core_id)
{
    if (core_id == nv::ipc::CoreId::Core1) {
        return;
    }

    constexpr uint32_t spc0_offset          = 20;
    constexpr uint32_t scg0_offset          = 16;
    constexpr uint32_t usbdcd_offset        = 16;
    constexpr uint32_t usbfs_offset         = 20;
    constexpr uint32_t usbhsphy_offset      = 8;
    constexpr uint32_t usbhs_offset         = 12;
    constexpr uint32_t usb_fs_master_offset = 22;
    constexpr uint32_t usb_hs_master_offset = 26;
    constexpr uint32_t rule_mask            = 0xF;
    constexpr uint32_t master_sec_mask      = 0x3;

    // Set to secure user
    AHBSC->AIPS_BRIDGE_GROUP0_MEM_RULE0 &= ~(rule_mask << spc0_offset);
    AHBSC->AIPS_BRIDGE_GROUP0_MEM_RULE0 |= static_cast<uint32_t>(
                                               AHBAccessLevel::SecurePrivileged)
                                        << spc0_offset;
    AHBSC->AIPS_BRIDGE_GROUP0_MEM_RULE0 &= ~(rule_mask << scg0_offset);
    AHBSC->AIPS_BRIDGE_GROUP0_MEM_RULE0 |= static_cast<uint32_t>(
                                               AHBAccessLevel::SecurePrivileged)
                                        << scg0_offset;

    AHBSC->AIPS_BRIDGE_GROUP3_MEM_RULE3 &= ~(rule_mask << usbdcd_offset);
    AHBSC->AIPS_BRIDGE_GROUP3_MEM_RULE3 |= static_cast<uint32_t>(
                                               AHBAccessLevel::SecurePrivileged)
                                        << usbdcd_offset;
    AHBSC->AIPS_BRIDGE_GROUP3_MEM_RULE3 &= ~(rule_mask << usbfs_offset);
    AHBSC->AIPS_BRIDGE_GROUP3_MEM_RULE3 |= static_cast<uint32_t>(
                                               AHBAccessLevel::SecurePrivileged)
                                        << usbfs_offset;

    AHBSC->AIPS_BRIDGE_GROUP4_MEM_RULE1 &= ~(rule_mask << usbhsphy_offset);
    AHBSC->AIPS_BRIDGE_GROUP4_MEM_RULE1 |= static_cast<uint32_t>(
                                               AHBAccessLevel::SecurePrivileged)
                                        << usbhsphy_offset;
    AHBSC->AIPS_BRIDGE_GROUP4_MEM_RULE1 &= ~(rule_mask << usbhs_offset);
    AHBSC->AIPS_BRIDGE_GROUP4_MEM_RULE1 |= static_cast<uint32_t>(
                                               AHBAccessLevel::SecurePrivileged)
                                        << usbhs_offset;

    AHBSC->MASTER_SEC_LEVEL &= ~(master_sec_mask << usb_fs_master_offset);
    AHBSC->MASTER_SEC_LEVEL |= static_cast<uint32_t>(AHBAccessLevel::SecurePrivileged)
                            << usb_fs_master_offset;
    AHBSC->MASTER_SEC_LEVEL &= ~(master_sec_mask << usb_hs_master_offset);
    AHBSC->MASTER_SEC_LEVEL |= static_cast<uint32_t>(AHBAccessLevel::SecurePrivileged)
                            << usb_hs_master_offset;

    AHBSC->MASTER_SEC_ANTI_POL_REG &= ~(master_sec_mask << usb_fs_master_offset);
    AHBSC->MASTER_SEC_ANTI_POL_REG |= static_cast<uint32_t>(AHBAccessLevel::NonSecureUser)
                                   << usb_fs_master_offset;
    AHBSC->MASTER_SEC_ANTI_POL_REG &= ~(master_sec_mask << usb_hs_master_offset);
    AHBSC->MASTER_SEC_ANTI_POL_REG |= static_cast<uint32_t>(AHBAccessLevel::NonSecureUser)
                                   << usb_hs_master_offset;
}

static void DMAConfig(uint32_t dma_index, uint32_t channel, nv::ipc::CoreId core_id)
{
    if (core_id == nv::ipc::CoreId::Invalid) {
        return;
    }

    // Re-initialize DMA controllers with security settings
    // This is required because DMA was initialized before security configuration

    // Enable security for DMA on both cores and configure the channels
    edma_channel_config_t channelConfig = {
        .enableMasterIDReplication = true,
        .securityLevel             = kEDMA_ChannelSecurityLevelSecure,
        .protectionLevel           = kEDMA_ChannelProtectionLevelPrivileged,
    };

    if (core_id == nv::ipc::CoreId::Core0) {
        if (dma_index == 0) {
            DMA0_config.enableMasterIdReplication = true;
            // coverity[cert_ctr50_cpp_violation] - will not exceed array bounds
            DMA0_config.channelConfig[channel] = &channelConfig;
            EDMA_Deinit(DMA0);
            EDMA_Init(DMA0, &DMA0_config);
        }
        else if (dma_index == 1) {
            DMA1_config.enableMasterIdReplication = true;
            // coverity[cert_ctr50_cpp_violation] - will not exceed array bounds
            DMA1_config.channelConfig[channel] = &channelConfig;
            EDMA_Deinit(DMA1);
            EDMA_Init(DMA1, &DMA1_config);
        }
    }
}

static void I3CConfig(nv::ipc::HardwareId hardware_id, nv::ipc::CoreId core_id)
{
    if (core_id == nv::ipc::CoreId::Invalid) {
        return;
    }

    constexpr uint32_t rule_mask = 0xF;

    // Determine access level based on core assignment
    AHBAccessLevel access_level = (core_id == nv::ipc::CoreId::Core0)
                                    ? AHBAccessLevel::SecurePrivileged
                                    : AHBAccessLevel::NonSecureUser;

    // Map I3C to correct bit offset in APB_PERIPHERAL_GROUP1_MEM_RULE0
    uint32_t bit_offset = hardware_id == nv::ipc::HardwareId::I3C_0 ? 4 : 8;

    // Configure the I3C peripheral access
    AHBSC->APB_PERIPHERAL_GROUP1_MEM_RULE0 &= ~(rule_mask << bit_offset);
    AHBSC->APB_PERIPHERAL_GROUP1_MEM_RULE0 |= static_cast<uint32_t>(access_level) << bit_offset;
}

static void FlexcommConfig(nv::ipc::HardwareId hardware_id, nv::ipc::CoreId core_id)
{
    constexpr uint32_t rule_mask = 0xF;

    // Determine access level based on core assignment
    AHBAccessLevel access_level = (core_id == nv::ipc::CoreId::Core0)
                                    ? AHBAccessLevel::SecurePrivileged
                                    : AHBAccessLevel::NonSecureUser;

    // Map FLEXCOMM to correct register and bit offset based on documentation
    volatile uint32_t* target_reg = nullptr;
    uint32_t           bit_offset = 0;

    switch (hardware_id) {
        // AHB_PERIPHERAL0_SLAVE_PORT_P12_SLAVE_RULE0
        case nv::ipc::HardwareId::FLEXCOMM0:
            target_reg = &AHBSC->AHB_PERIPHERAL0_SLAVE_PORT_P12_SLAVE_RULE0;
            bit_offset = 12;  // LP_FLEXCOMM0: bits [13:12]
            break;
        case nv::ipc::HardwareId::FLEXCOMM1:
            target_reg = &AHBSC->AHB_PERIPHERAL0_SLAVE_PORT_P12_SLAVE_RULE0;
            bit_offset = 16;  // LP_FLEXCOMM1: bits [17:16] = 0 nibble (the one to change)
            break;
        case nv::ipc::HardwareId::FLEXCOMM2:
            target_reg = &AHBSC->AHB_PERIPHERAL0_SLAVE_PORT_P12_SLAVE_RULE0;
            bit_offset = 20;  // LP_FLEXCOMM2: bits [21:20]
            break;
        case nv::ipc::HardwareId::FLEXCOMM3:
            target_reg = &AHBSC->AHB_PERIPHERAL0_SLAVE_PORT_P12_SLAVE_RULE0;
            bit_offset = 24;  // LP_FLEXCOMM3: bits [25:24]
            break;

        // AHB_PERIPHERAL1_SLAVE_PORT_P13_SLAVE_RULE0
        case nv::ipc::HardwareId::FLEXCOMM4:
            target_reg = &AHBSC->AHB_PERIPHERAL1_SLAVE_PORT_P13_SLAVE_RULE0;
            bit_offset = 20;  // FLEXCOMM4: bits [21:20]
            break;
        case nv::ipc::HardwareId::FLEXCOMM5:
            target_reg = &AHBSC->AHB_PERIPHERAL1_SLAVE_PORT_P13_SLAVE_RULE0;
            bit_offset = 24;  // FLEXCOMM5: bits [25:24]
            break;
        case nv::ipc::HardwareId::FLEXCOMM6:
            target_reg = &AHBSC->AHB_PERIPHERAL1_SLAVE_PORT_P13_SLAVE_RULE0;
            bit_offset = 28;  // FLEXCOMM6: bits [29:28]
            break;

        // AHB_PERIPHERAL1_SLAVE_PORT_P13_SLAVE_RULE1
        case nv::ipc::HardwareId::FLEXCOMM7:
            target_reg = &AHBSC->AHB_PERIPHERAL1_SLAVE_PORT_P13_SLAVE_RULE1;
            bit_offset = 0;  // FLEXCOMM7: bits [1:0]
            break;
        case nv::ipc::HardwareId::FLEXCOMM8:
            target_reg = &AHBSC->AHB_PERIPHERAL1_SLAVE_PORT_P13_SLAVE_RULE1;
            bit_offset = 4;  // FLEXCOMM8: bits [5:4]
            break;
        case nv::ipc::HardwareId::FLEXCOMM9:
            target_reg = &AHBSC->AHB_PERIPHERAL1_SLAVE_PORT_P13_SLAVE_RULE1;
            bit_offset = 8;  // FLEXCOMM9: bits [9:8]
            break;

        default: return;  // Invalid FLEXCOMM ID
    }

    *target_reg &= ~(rule_mask << bit_offset);
    *target_reg |= static_cast<uint32_t>(access_level) << bit_offset;
}

// coverity[declared_but_not_referenced] - reference in dual core
static void HardwareConfig(nv::ipc::HardwareId hardware_id)
{
    // Get core assignment for this hardware
    nv::ipc::CoreId core_id = nv::ipc::get_core_from_hardware(hardware_id);

    if (core_id == nv::ipc::CoreId::Invalid) {
        return;  // Invalid hardware ID
    }
    switch (hardware_id) {
        case nv::ipc::HardwareId::USB        : USBConfig(core_id); return;
        case nv::ipc::HardwareId::I3C_0      : I3CConfig(nv::ipc::HardwareId::I3C_0, core_id); return;
        case nv::ipc::HardwareId::I3C_1      : I3CConfig(nv::ipc::HardwareId::I3C_1, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH0 : DMAConfig(0, 0, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH1 : DMAConfig(0, 1, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH2 : DMAConfig(0, 2, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH3 : DMAConfig(0, 3, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH4 : DMAConfig(0, 4, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH5 : DMAConfig(0, 5, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH6 : DMAConfig(0, 6, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH7 : DMAConfig(0, 7, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH8 : DMAConfig(0, 8, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH9 : DMAConfig(0, 9, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH10: DMAConfig(0, 10, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH11: DMAConfig(0, 11, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH12: DMAConfig(0, 12, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH13: DMAConfig(0, 13, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH14: DMAConfig(0, 14, core_id); return;
        case nv::ipc::HardwareId::eDMA_0_CH15: DMAConfig(0, 15, core_id); return;
        case nv::ipc::HardwareId::eDMA_1_CH0 : DMAConfig(1, 0, core_id); return;
        case nv::ipc::HardwareId::eDMA_1_CH1 : DMAConfig(1, 1, core_id); return;
        case nv::ipc::HardwareId::FLEXCOMM0  :
        case nv::ipc::HardwareId::FLEXCOMM1  :
        case nv::ipc::HardwareId::FLEXCOMM2  :
        case nv::ipc::HardwareId::FLEXCOMM3  :
        case nv::ipc::HardwareId::FLEXCOMM4  :
        case nv::ipc::HardwareId::FLEXCOMM5  :
        case nv::ipc::HardwareId::FLEXCOMM6  :
        case nv::ipc::HardwareId::FLEXCOMM7  :
        case nv::ipc::HardwareId::FLEXCOMM8  :
        case nv::ipc::HardwareId::FLEXCOMM9  : FlexcommConfig(hardware_id, core_id); return;

        default: return;
    }
}

void AHBConfig()
{
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
    // Prevent Chip reset by ITRC
    ITRC0->OUT_SEL[4][0] = (ITRC0->OUT_SEL[4][0] & ~ITRC_OUT_SEL_IN7_SELn_MASK)
                         | (ITRC_OUT_SEL_IN7_SELn(0x2));
    ITRC0->OUT_SEL[4][1] = (ITRC0->OUT_SEL[4][1] & ~ITRC_OUT_SEL_IN7_SELn_MASK)
                         | (ITRC_OUT_SEL_IN7_SELn(0x2));
    SCB->SHCSR |= (1UL << 19U);  // Enable secure fault
    // NOLINTBEGIN(*-reinterpret-cast)
    // Get shared memory info from linker symbols
    uint32_t shared_memory_start = reinterpret_cast<uint32_t>(&__shared_memory_start__);
    uint32_t shared_memory_end   = reinterpret_cast<uint32_t>(&__shared_memory_end__);

    // Get Core1 image boundaries from linker symbols
    uint32_t core1_text_start = reinterpret_cast<uint32_t>(&__core1_image_start__);
    uint32_t core1_text_end   = reinterpret_cast<uint32_t>(&__core1_image_end__);

    // Get Core1 RAM boundaries from linker symbols
    uint32_t core1_ram_start = reinterpret_cast<uint32_t>(&__core1_ram_start__);
    uint32_t core1_ram_end   = reinterpret_cast<uint32_t>(&__core1_ram_end__);

    // NOLINTEND(*-reinterpret-cast)

    // Configure shared memory access
    ConfigureRulesAccess(shared_memory_start, shared_memory_end, AHB_RAM_BLOCK_SIZE);

    // Configure Core1 text access
    ConfigureRulesAccess(align_down(core1_text_start, AHB_FLASH_BLOCK_SIZE),
                         align_up(core1_text_end, AHB_FLASH_BLOCK_SIZE),
                         AHB_FLASH_BLOCK_SIZE);

    // Configure Core1 RAM access
    ConfigureRulesAccess(core1_ram_start, core1_ram_end, AHB_RAM_BLOCK_SIZE);

    // Loop through all hardware IDs and configure them
    for (int v = static_cast<int>(nv::ipc::HardwareId::Begin);
         v < static_cast<int>(nv::ipc::HardwareId::End);
         ++v) {
        HardwareConfig(static_cast<nv::ipc::HardwareId>(v));
    }

    NVIC_EnableIRQ(SEC_VIO_IRQn);
    // NVIC_SetPriority(SEC_VIO_IRQn, 0);

    // Enable Security Check
    // In 556, security violation reset is shown in strata board
    // Disable it currently
    AHBSC->MISC_CTRL_REG    = 0x000086AAU;
    AHBSC->MISC_CTRL_DP_REG = 0x000086AAU;
#ifdef CPU_MCXN547VDF
    AHBSC->MISC_CTRL_REG    = 0x000086A6U;
    AHBSC->MISC_CTRL_DP_REG = 0x000086A6U;
#endif

#else
    // CMSE not available, configuration not performed
#endif
}

}  // namespace sys::common
