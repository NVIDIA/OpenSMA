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
#include NV_IPC_CONFIG_H
#include <assert.h>
#include <fsl_device_registers.h>
#include <fsl_i3c.h>
#include <fsl_i3c_edma.h>
#include <fsl_smartdma.h>
#ifdef CPU_MCXN556SCDF
#include <smartdma_i3c.h>
#endif
namespace sys::smartdma {

typedef struct __attribute__((aligned(4))) [[gnu::packed]]
{
    volatile uint8_t type;
    volatile uint8_t address;
    volatile uint8_t payload_size;
    volatile uint8_t buf[8];
} i3c_ibi_data_t;

#ifdef CPU_MCXN556SCDF
constexpr uint32_t SmartDmaStackSize       = 32;
constexpr uint32_t SmartDmaRegistersSize   = 20;
constexpr uint32_t SmartDmaDebugBufferSize = 8;
constexpr size_t   SmartDmaI3cNumInstances = 2;

enum SmartDmaRegisters
{
    I3C0_MSTATUS   = I3C0_BASE + 0x88,
    I3C0_MCTRL     = I3C0_BASE + 0x84,
    I3C0_MDATACTRL = I3C0_BASE + 0xAC,
    I3C0_MRDATAB   = I3C0_BASE + 0xC0,

    I3C1_MSTATUS   = I3C1_BASE + 0x88,
    I3C1_MCTRL     = I3C1_BASE + 0x84,
    I3C1_MDATACTRL = I3C1_BASE + 0xAC,
    I3C1_MRDATAB   = I3C1_BASE + 0xC0,
};

struct SmartDmaContext
{
    smartdma_i3c_param_t param;
    volatile uint32_t    stack[SmartDmaStackSize];
    volatile uint32_t    debug_buf[SmartDmaDebugBufferSize];
    i3c_ibi_data_t       ibi_data[SmartDmaI3cNumInstances];
    volatile uint32_t    registers[SmartDmaRegistersSize];
};
#endif

class Driver
{
protected:
public:
    static void            init();
    static i3c_ibi_data_t* get_ibi_data(uint8_t instance);
    static void            log_status(i3c_master_edma_handle_t& handle);
};

}  // namespace sys::smartdma
