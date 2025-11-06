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
#include "nv/nv.h"
#include "sys/gpio/constant.h"
#include "sys/gpio/driver.h"
extern "C" {
// NOLINTBEGIN

void GPIO40_IRQHandler()
{
    nv::info("GPIO40_IRQHandler\n");
    GPIO_Type* inst = GPIO4;
    uint32_t   flag = GPIO_GpioGetInterruptChannelFlags(inst, 0);
    nv::info("flag:0x%x\n", flag);

    if (flag & nv::common::bit(SYS_GPIO_GA_GPIO9_IROT_ERROR_N_PIN)) {
        nv::info("GA_GPIO9_IROT_ERROR_N_PIN\n");
    }

    if (flag & nv::common::bit(SYS_GPIO_GA_GPIO10_IROT_AP_BOOT_COMPLETE_PIN)) {
        nv::info("GA_GPIO10_IROT_AP_BOOT_COMPLETE_PIN\n");
    }

    if (flag & nv::common::bit(SYS_GPIO_THERM_OVERT_PIN)) {
        nv::info("THERM_OVERT_PIN\n");
    }

    if (flag & nv::common::bit(SYS_GPIO_PS_BOARD_PGOOD_PIN)) {
        nv::info("PS_BOARD_PGOOD_PIN\n");
    }

    GPIO_GpioClearInterruptChannelFlags(inst, flag, 0);
}

void GPIO41_IRQHandler()
{
    nv::info("GPIO41_IRQHandler\n");
    GPIO_Type* inst = GPIO4;
    uint32_t   flag = GPIO_GpioGetInterruptChannelFlags(inst, 1);
    nv::info("flag:0x%x\n", flag);

    if (flag & nv::common::bit(SYS_GPIO_GA_GPIO9_IROT_ERROR_N_PIN)) {
        nv::info("GA_GPIO9_IROT_ERROR_N_PIN\n");
    }

    if (flag & nv::common::bit(SYS_GPIO_GA_GPIO10_IROT_AP_BOOT_COMPLETE_PIN)) {
        nv::info("GA_GPIO10_IROT_AP_BOOT_COMPLETE_PIN\n");
    }

    if (flag & nv::common::bit(SYS_GPIO_THERM_OVERT_PIN)) {
        nv::info("THERM_OVERT_PIN\n");
    }

    if (flag & nv::common::bit(SYS_GPIO_PS_BOARD_PGOOD_PIN)) {
        nv::info("PS_BOARD_PGOOD_PIN\n");
    }

    GPIO_GpioClearInterruptChannelFlags(inst, flag, 1);
}
}
// NOLINTEND