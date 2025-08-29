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

#include "sys/adc/adc.h"

#include "nv/common/debug.h"
#include "nv/common/utils.h"
#include "nv/logger/log.h"

namespace sys::adc {

bool ADC::init_adc([[maybe_unused]] AdcPeripheral peripheral)
{
    return false;
}

void ADC::trigger_read([[maybe_unused]] AdcPeripheral peripheral,
                       [[maybe_unused]] uint32_t      triggerCommand)
{
    return;
}

uint32_t ADC::get_fifo_count([[maybe_unused]] AdcPeripheral peripheral,
                             [[maybe_unused]] uint8_t       fifo_num)
{
    return 0U;
}

bool ADC::pop_fifo([[maybe_unused]] AdcPeripheral peripheral,
                   [[maybe_unused]] uint8_t       fifo_num,
                   [[maybe_unused]] uint16_t&     result,
                   [[maybe_unused]] uint32_t&     command)
{
    return false;
}

bool ADC::adc_ready([[maybe_unused]] AdcPeripheral peripheral)
{
    return false;
}

uint32_t ADC::get_status_flags([[maybe_unused]] AdcPeripheral peripheral)
{
    return 0U;
}

}  // namespace sys::adc
