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

// ADC constants for x86 stub (matching hardware platform enum values, not literal values)
constexpr uint8_t ADC_SAMPLE_TIME_ADCK3   = 0;  // kLPADC_SampleTimeADCK3 = 0U (3 ADCK cycles)
constexpr uint8_t ADC_SAMPLE_TIME_ADCK5   = 1;  // kLPADC_SampleTimeADCK5 = 1U (5 ADCK cycles)
constexpr uint8_t ADC_SAMPLE_TIME_ADCK7   = 2;  // kLPADC_SampleTimeADCK7 = 2U (7 ADCK cycles)
constexpr uint8_t ADC_SAMPLE_TIME_ADCK11  = 3;  // kLPADC_SampleTimeADCK11 = 3U (11 ADCK cycles)
constexpr uint8_t ADC_SAMPLE_TIME_ADCK19  = 4;  // kLPADC_SampleTimeADCK19 = 4U (19 ADCK cycles)
constexpr uint8_t ADC_SAMPLE_TIME_ADCK35  = 5;  // kLPADC_SampleTimeADCK35 = 5U (35 ADCK cycles)
constexpr uint8_t ADC_SAMPLE_TIME_ADCK67  = 6;  // kLPADC_SampleTimeADCK67 = 6U (67 ADCK cycles)
constexpr uint8_t ADC_SAMPLE_TIME_ADCK131 = 7;  // kLPADC_SampleTimeADCK131 = 7U (131 ADCK
                                                // cycles)

constexpr uint8_t ADC_HW_AVERAGE_COUNT1    = 0;   // kLPADC_HardwareAverageCount1 = 0U
constexpr uint8_t ADC_HW_AVERAGE_COUNT2    = 1;   // kLPADC_HardwareAverageCount2 = 1U
constexpr uint8_t ADC_HW_AVERAGE_COUNT4    = 2;   // kLPADC_HardwareAverageCount4 = 2U
constexpr uint8_t ADC_HW_AVERAGE_COUNT8    = 3;   // kLPADC_HardwareAverageCount8 = 3U
constexpr uint8_t ADC_HW_AVERAGE_COUNT16   = 4;   // kLPADC_HardwareAverageCount16 = 4U
constexpr uint8_t ADC_HW_AVERAGE_COUNT32   = 5;   // kLPADC_HardwareAverageCount32 = 5U
constexpr uint8_t ADC_HW_AVERAGE_COUNT64   = 6;   // kLPADC_HardwareAverageCount64 = 6U
constexpr uint8_t ADC_HW_AVERAGE_COUNT128  = 7;   // kLPADC_HardwareAverageCount128 = 7U
constexpr uint8_t ADC_HW_AVERAGE_COUNT256  = 8;   // kLPADC_HardwareAverageCount256 = 8U
constexpr uint8_t ADC_HW_AVERAGE_COUNT512  = 9;   // kLPADC_HardwareAverageCount512 = 9U
constexpr uint8_t ADC_HW_AVERAGE_COUNT1024 = 10;  // kLPADC_HardwareAverageCount1024 = 10U (1024
                                                  // conversions)
constexpr uint8_t ADC_RESOLUTION_HIGH    = 0;     // kLPADC_ResolutionHigh
constexpr uint8_t ADC_COMPARE_DISABLED   = 0;     // kLPADC_HardwareCompareDisabled
constexpr uint8_t ADC_COMPARE_STORE_TRUE = 1;     // kLPADC_HardwareCompareStoreOnTrue

class ADC
{
public:
    static constexpr uint32_t resolution_bits(bool hi_resolution, bool single_ended)
    {
        return hi_resolution ? 16 : single_ended ? 12 : 13;
    }

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

    // returns the conversion result from the FIFO
    // use instead of `pop_fifo` in performance critical code
    template<AdcPeripheral peripheral, uint32_t fifo_num>
    static uint16_t pop_fifo_conv_value()
    {
        return 0;
    }

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
    static void set_adc_trigger(AdcPeripheral peripheral,
                                uint32_t      triggerSrc,
                                uint32_t      targetCommandId,
                                uint8_t       fifoSelect = 0);

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
        uint8_t  loopCount;
        uint8_t  sampleTimeMode;
        uint8_t  hardwareAverageMode;
        uint8_t  conversionResolutionMode;
        uint8_t  hardwareCompareMode;
    };
    static void set_adc_command(AdcPeripheral           peripheral,
                                uint32_t                commandId,
                                const AdcCommandConfig& config);

    // Temperature sensor parameter methods (stub for x86)
    static constexpr float get_temp_parameter_a() { return 1.0f; }
    static constexpr float get_temp_parameter_b() { return 0.0f; }
    static constexpr float get_temp_parameter_alpha() { return 1.0f; }
};

}  // namespace sys::adc
