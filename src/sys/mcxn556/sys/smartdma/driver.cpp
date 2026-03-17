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
#ifdef CPU_MCXN556SCDF
#include <algorithm>
#include <cstring>
#include "nv/nv.h"
#include "sys/smartdma/driver.h"
#include "nv/logger/log.h"

using namespace sys::smartdma;

// shared global flags between smartdma and i3c
NV_SRAMX_CODE volatile i3c_func_t i3c_function_config[SmartDmaI3cNumInstances]
    __attribute__((aligned(4))) = {0};  // NOLINT(*-non-const-global-variables)
NV_SRAMX_CODE volatile i3c_smartdma_flag_t i3c_smartdma_flag[SmartDmaI3cNumInstances]
    __attribute__((aligned(4))) = {0};  // NOLINT(*-non-const-global-variables)

namespace {
// shared config between smartdma, interrupt, and task contexts
NV_SRAMX_CODE static SmartDmaContext smartdma_ctx;

static void Smartdma_callback(void* param) {}
}  // namespace

void Driver::init()
{
    // init smartdma registers
    smartdma_ctx.registers[0]  = SmartDmaRegisters::I3C0_MSTATUS;
    smartdma_ctx.registers[1]  = SmartDmaRegisters::I3C0_MCTRL;
    smartdma_ctx.registers[2]  = SmartDmaRegisters::I3C0_MDATACTRL;
    smartdma_ctx.registers[3]  = SmartDmaRegisters::I3C0_MRDATAB;
    smartdma_ctx.registers[10] = SmartDmaRegisters::I3C1_MSTATUS;
    smartdma_ctx.registers[11] = SmartDmaRegisters::I3C1_MCTRL;
    smartdma_ctx.registers[12] = SmartDmaRegisters::I3C1_MDATACTRL;
    smartdma_ctx.registers[13] = SmartDmaRegisters::I3C1_MRDATAB;

    // init smartdma
    SMARTDMA_InitWithoutFirmware();
    SMARTDMA_InstallCallback(Smartdma_callback, NULL);
    smartdma_ctx.param.smartdma_stack   = (uint32_t*)smartdma_ctx.stack;
    smartdma_ctx.param.p_i3c_reg_addr   = (uint32_t*)smartdma_ctx.registers;
    smartdma_ctx.param.p_smartdma_debug = (uint32_t*)smartdma_ctx.debug_buf;

    // I3C0
    i3c_function_config[0].i3c_read_write_start = 0;
    i3c_function_config[0].i3c_ibi_won          = 0;
    i3c_smartdma_flag[0].ibi_stop               = 0;
    i3c_smartdma_flag[0].read_write_stop        = 0;
    smartdma_ctx.param.p_i3c0_func_config       = (uint32_t*)&i3c_function_config[0];
    smartdma_ctx.param.p_i3c0_smartdma_flags    = (uint32_t*)&i3c_smartdma_flag[0];
    memset(&smartdma_ctx.ibi_data[0], 0, sizeof(i3c_ibi_data_t));
    smartdma_ctx.param.p_i3c0_ibi_data = (uint32_t*)&smartdma_ctx.ibi_data[0];

    // I3C1
    i3c_function_config[1].i3c_read_write_start = 0;
    i3c_function_config[1].i3c_ibi_won          = 0;
    i3c_smartdma_flag[1].ibi_stop               = 0;
    i3c_smartdma_flag[1].read_write_stop        = 0;
    smartdma_ctx.param.p_i3c1_func_config       = (uint32_t*)&i3c_function_config[1];
    smartdma_ctx.param.p_i3c1_smartdma_flags    = (uint32_t*)&i3c_smartdma_flag[1];
    memset(&smartdma_ctx.ibi_data[1], 0, sizeof(i3c_ibi_data_t));
    smartdma_ctx.param.p_i3c1_ibi_data = (uint32_t*)&smartdma_ctx.ibi_data[1];

    SMARTDMA_Boot_API((uint32_t)Smartdma_Dual_I3C_DMA, &smartdma_ctx.param, 0x2);
    NVIC_SetPriority(SMARTDMA_IRQn, 3);
    EnableIRQ(SMARTDMA_IRQn);
}

i3c_ibi_data_t* Driver::get_ibi_data(uint8_t instance)
{
    return &smartdma_ctx.ibi_data[instance];
}

void Driver::log_status(i3c_master_edma_handle_t& handle)
{
    auto instance = I3C_GetInstance(handle.base);
    auto state    = static_cast<uint8_t>(handle.state);
    auto flags    = static_cast<uint8_t>(i3c_smartdma_flag[instance].read_write_stop << 5
                                      | i3c_smartdma_flag[instance].ibi_stop << 4
                                      | i3c_function_config[instance].i3c_read_write_start << 1
                                      | i3c_function_config[instance].i3c_ibi_won << 0);
    auto mstatus  = static_cast<uint16_t>(handle.base->MSTATUS & 0xFFFF);
    auto pc       = SMARTDMA0->PC;

    nv::logger::info(nv::logger::Event::I3CSmartDmaDebug,
                     {
                         static_cast<uint8_t>(state),
                         static_cast<uint8_t>(flags),
                         static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                         static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                         static_cast<uint8_t>(pc >> 0 & 0xFF),
                         static_cast<uint8_t>(pc >> 8 & 0xFF),
                         static_cast<uint8_t>(pc >> 16 & 0xFF),
                         static_cast<uint8_t>(pc >> 24 & 0xFF),
                     });
}
#endif