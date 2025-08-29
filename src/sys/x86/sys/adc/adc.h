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

namespace sys::adc {

using AdcPeripheral = uint32_t;
using AdcChannel    = uint32_t;

class ADC
{
public:
    // init adc peripheral hardware, trigger, and irq
    static bool init_adc(AdcPeripheral peripheral);

    // triggers a read from the ADC
    // poll read_recent_value to get data
    static void trigger_read(AdcPeripheral peripheral, uint32_t triggerCommand);

    // returns current data count in fifo
    static uint32_t get_fifo_count(AdcPeripheral peripheral, uint8_t fifo_num);

    // returns the top value off the fifo
    // identifer is the command used for that result
    static bool
    pop_fifo(AdcPeripheral peripheral, uint8_t fifo_num, uint16_t& result, uint32_t& command);

    // returns if the ADC finished the latest trigger and is ready for a new one
    static bool adc_ready(AdcPeripheral peripheral);

    // returns status flag for ADC
    // see _lpadc_status_flags in fsl_lpadc.h
    static uint32_t get_status_flags(AdcPeripheral peripheral);
};

}  // namespace sys::adc
