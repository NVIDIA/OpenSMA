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

    // resets fifo for given ADC
    static void reset_fifo(AdcPeripheral peripheral, uint8_t fifo_num);

    // enable VREF
    static void enable_vref();

    // enable ADC
    static void enable_adc(AdcPeripheral peripheral, bool enable);

    // enable ADC interrupt
    static void enable_adc_nvic_interrupt(AdcPeripheral peripheral);
    static void enable_adc_lpadc_interrupt(AdcPeripheral peripheral, uint32_t mask);

    // disable ADC interrupt
    static void disable_adc_nvic_interrupt(AdcPeripheral peripheral);
    static void disable_adc_lpadc_interrupt(AdcPeripheral peripheral, uint32_t mask);

    // clear status flags for ADC
    static void clear_status_flags(AdcPeripheral peripheral, uint32_t flags);

    // set adc trigger
    static void
    set_adc_trigger(AdcPeripheral peripheral, uint32_t triggerSrc, uint32_t targetCommandId);

    // get adc reading
    struct AdcConvResult
    {
        uint32_t commandIdSource; /*!< Indicate the command buffer being executed that generated
                                     this result. */
        uint32_t loopCountIndex;  /*!< Indicate the loop count value during command execution
                                     that generated this result. */
        uint32_t triggerIdSource; /*!< Indicate the trigger source that initiated a conversion
                                     and generated this result. */
        uint16_t convValue;       /*!< Data result. */
    };
    static bool get_adc_reading(AdcPeripheral peripheral, AdcConvResult& result, uint8_t index);

    /**
     * subset of the lpadc_conv_command_config_t struct
     * @note fill in the rest of the struct if needed
     */
    struct AdcCommandConfig
    {
        uint8_t  sampleChannelMode;
        uint8_t  channelNumber;
        uint8_t  chainedNextCommandNumber;
        uint16_t hwCompareValueHigh;
        uint16_t hwCompareValueLow;
    };
    static void set_adc_command(AdcPeripheral           peripheral,
                                uint32_t                commandId,
                                const AdcCommandConfig& config);
};

}  // namespace sys::adc
