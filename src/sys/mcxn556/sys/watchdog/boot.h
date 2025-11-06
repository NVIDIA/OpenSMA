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
#include "fsl_cdog.h"

namespace sys::watchdog {

enum BootStatus
{
    Init    = 0,
    Success = 1,
    Failed  = 2,
    TryBoot = 3,
};

class Boot
{
protected:
    // WAR: Not all platform are boot with FMC in this phase
    // Authenticate inactive instead of using FMC authenticate result
    static bool auth_inactive_from_kernel();

    static constexpr uint32_t CdogSecureConst  = 0x95959595;
    static constexpr uint32_t StatusCurstMask  = (0xF0000000);
    static constexpr uint32_t StatusCurstShift = 28;
    static constexpr uint32_t OneSecMs         = 1'000;
    enum class State : uint8_t
    {
        Idle   = 0x5,
        Active = 0xA,
    };

    struct [[gnu::packed]] SlotFailedRecord
    {
        uint32_t try_times    : 2;
        uint32_t switch_times : 2;
        uint8_t  status       : 4;
    };

    static_assert(sizeof(SlotFailedRecord) == sizeof(uint8_t),
                  "BootFailedRecord shall be the same size as uint8_t");

    struct BootFailedRecord
    {
        SlotFailedRecord slot0_failed;
        SlotFailedRecord slot1_failed;
        uint8_t          prev_try_boot_slot : 2;
        uint8_t          prev_booted_slot   : 2;
        uint8_t          slot0_runtime_flag : 2;
        uint8_t          slot1_runtime_flag : 2;
        uint8_t          target_boot_slot   : 2;
        uint16_t         rvsd               : 6;
    };

    static_assert(sizeof(BootFailedRecord) == sizeof(uint32_t),
                  "BootFailedRecord shall be the same size as uint32_t");

    static constexpr uint32_t MaxRetry = 1;
    static constexpr uint32_t MaxNum   = MaxRetry + 1;
};

}  // namespace sys::watchdog