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
void GPIO00_IRQHandler()
{
    nv::info("GPIO00_IRQHandler\n");
    GPIO_Type* inst = GPIO0;
    uint32_t   flag = GPIO_GpioGetInterruptChannelFlags(inst, 0);
    nv::info("flag:0x%x\n", flag);
    GPIO_GpioClearInterruptChannelFlags(inst, flag, 0);
}

void GPIO01_IRQHandler()
{
    nv::info("GPIO01_IRQHandler\n");
    GPIO_Type* inst = GPIO0;
    uint32_t   flag = GPIO_GpioGetInterruptChannelFlags(inst, 1);
    nv::info("flag:0x%x\n", flag);
    GPIO_GpioClearInterruptChannelFlags(inst, flag, 1);
}
}
// NOLINTEND