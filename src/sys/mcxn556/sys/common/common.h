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

namespace nv::ipc {

constexpr uint8_t UsbDeviceInterruptPriority = 3;

// Bug-5947030: During workloads that involve a large number of USB and I3C interrupts (eg. PLDM
// updates) if the I3C interrupt service time is high, long-T bit issue (Bug-5726655) might
// resurface. Allow I3C interrupts to pre-empt USB interrupts to avoid this issue.
constexpr uint8_t I3CInterruptPriority = UsbDeviceInterruptPriority - 1;

enum class CoreId : uint8_t
{
    Begin,
    Core0 = Begin,  // Core0 owns the instance
    Core1,          // Core1 owns the instance
    Both,           // Both core has its own instance, TaskId should not use this
    Abstract,       // Only used for TaskId, meaning the task doesn't really exist.
    Invalid,
    End = Invalid
};

constexpr nv::ipc::CoreId get_current_core()
{
#if defined(CPU_MCXN547VDF_cm33_core1) || defined(CPU_MCXN556SCDF_cm33_core1)
    return nv::ipc::CoreId::Core1;
    // default return Core0
#else
    return nv::ipc::CoreId::Core0;
#endif
}

constexpr nv::ipc::CoreId get_peer_core()
{
    if constexpr (get_current_core() == nv::ipc::CoreId::Core0) {
        return nv::ipc::CoreId::Core1;
    }
    else {
        return nv::ipc::CoreId::Core0;
    }
}

enum class HardwareId
{
    Begin,
    USB = Begin,  // USB Full Speed + High Speed + PHY + DCD + Clock/Power
    I3C_0,
    I3C_1,
    FLEXCOMM0,
    FLEXCOMM1,
    FLEXCOMM2,
    FLEXCOMM3,
    FLEXCOMM4,
    FLEXCOMM5,
    FLEXCOMM6,
    FLEXCOMM7,
    FLEXCOMM8,
    FLEXCOMM9,
    eDMA_0_CH0,
    eDMA_0_CH1,
    eDMA_0_CH2,
    eDMA_0_CH3,
    eDMA_0_CH4,
    eDMA_0_CH5,
    eDMA_0_CH6,
    eDMA_0_CH7,
    eDMA_0_CH8,
    eDMA_0_CH9,
    eDMA_0_CH10,
    eDMA_0_CH11,
    eDMA_0_CH12,
    eDMA_0_CH13,
    eDMA_0_CH14,
    eDMA_0_CH15,
    eDMA_1_CH0,
    eDMA_1_CH1,
    eDMA_1_CH2,
    eDMA_1_CH3,
    eDMA_1_CH4,
    eDMA_1_CH5,
    eDMA_1_CH6,
    eDMA_1_CH7,
    eDMA_1_CH8,
    eDMA_1_CH9,
    eDMA_1_CH10,
    eDMA_1_CH11,
    eDMA_1_CH12,
    eDMA_1_CH13,
    eDMA_1_CH14,
    eDMA_1_CH15,
    ADC_0,
    ADC_1,
    PKC_RAM,
    ITRC,
    End
};

}  // namespace nv::ipc
