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
#include "MCXN547_cm33_core1.h"

// Declare the linker symbols as global extern "C"
extern "C" {
extern uint32_t __stream_buffer_start__;
extern uint32_t __stream_buffer_end__;
}

namespace sys::common {

struct MemRuleRegion
{
    uint32_t           start;
    uint32_t           end;
    volatile uint32_t* reg_addr;  // Store register address
};

static const MemRuleRegion rule_table[] = {
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

bool AHBConfig()
{
    // Get stream buffer info from linker symbols
    uint32_t stream_buffer_start = reinterpret_cast<uint32_t>(&__stream_buffer_start__);
    uint32_t stream_buffer_end   = reinterpret_cast<uint32_t>(&__stream_buffer_end__);

    AHBSC->MISC_CTRL_REG    = 0x000086A6U;
    AHBSC->MISC_CTRL_DP_REG = 0x000086A6U;

    // Configure stream buffer access only
    return ConfigureStreamBufferAccess(stream_buffer_start, stream_buffer_end)
        && ConfigureCore1TextAccess() && ConfigureCore1RAMAccess();
}

bool ConfigureStreamBufferAccess(uint32_t stream_buffer_start, uint32_t stream_buffer_end)
{
    constexpr uint32_t AHB_RULE_BLOCK_SIZE    = 0x1000U;
    constexpr uint32_t AHB_RULE_BLOCK_SHIFT   = 12U;   // log2(4096) = 12
    constexpr uint32_t AHB_RULES_PER_REGISTER = 8U;    // Each register controls 8 rules
    constexpr uint32_t AHB_RULE_MASK          = 0x7U;  // Mask for 8 rules (0-7)
    constexpr uint32_t AHB_BITS_PER_RULE      = 4U;    // Each rule uses 4 bits
    constexpr uint32_t AHB_RULE_VALUE_MASK    = 0xFU;  // Mask for 4-bit rule value

    // Check alignment requirements (start and end must be 0x1000 aligned)
    // If both start and end are 4K aligned, size is automatically 4K aligned
    if ((stream_buffer_start & (AHB_RULE_BLOCK_SIZE - 1U)) != 0) {
        // Start address not aligned to 4KB boundary
        return false;
    }

    if ((stream_buffer_end & (AHB_RULE_BLOCK_SIZE - 1U)) != 0) {
        // End address not aligned to 4KB boundary
        return false;
    }

    // Configure each 4KB region that the stream buffer spans
    for (uint32_t addr  = stream_buffer_start; addr < stream_buffer_end;
         addr          += AHB_RULE_BLOCK_SIZE) {
        for (const auto& region : rule_table) {
            if (addr >= region.start && addr < region.end) {
                uint32_t rule_index = (addr - region.start)
                                   >> AHB_RULE_BLOCK_SHIFT;             // Calculate
                                                                        // 4KB
                                                                        // rule
                                                                        // index
                uint32_t rule_offset = rule_index & AHB_RULE_MASK;      // Bit position within
                                                                        // register (0-7)
                uint32_t rule_shift = rule_offset * AHB_BITS_PER_RULE;  // Each rule uses 4 bits

                // Access register directly
                uint32_t current_rule = *region.reg_addr;
                current_rule &= ~(AHB_RULE_VALUE_MASK << rule_shift);  // Clear the 4-bit rule
                                                                       // (sets to 0x0 =
                                                                       // Non-secure, User
                                                                       // access)
                *region.reg_addr = current_rule;
                break;  // Region matched, no need to check further
            }
        }
    }

    return true;  // Success
}

bool ConfigureCore1TextAccess()
{
    // will need to implement this
    return true;
}

bool ConfigureCore1RAMAccess()
{
    // will need to implement this
    return true;
}

}  // namespace sys::common