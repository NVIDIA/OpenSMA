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

#include <array>
#include <cstdint>
#include <assert.h>

#include <fsl_lpadc.h>
#include <fsl_spc.h>

#include "board/peripherals.h"

namespace sys::adc {

using AdcPeripheral = uint32_t;
using AdcChannel    = uint32_t;

enum class AdcScanMode : uint8_t
{
    SingleEndedSideA       = 0,
    SingleEndedSideB       = 1,
    DifferentialBothSideAB = 2,
    Invalid
};

// ADC command configuration constants (from fsl_lpadc.h driver)
// These are exposed to nv layer through sys layer abstraction

// Sample Time Mode - lpadc_sample_time_mode_t
constexpr uint8_t ADC_SAMPLE_TIME_ADCK3   = kLPADC_SampleTimeADCK3;    // 3 ADCK cycles
constexpr uint8_t ADC_SAMPLE_TIME_ADCK5   = kLPADC_SampleTimeADCK5;    // 5 ADCK cycles
constexpr uint8_t ADC_SAMPLE_TIME_ADCK7   = kLPADC_SampleTimeADCK7;    // 7 ADCK cycles
constexpr uint8_t ADC_SAMPLE_TIME_ADCK11  = kLPADC_SampleTimeADCK11;   // 11 ADCK cycles
constexpr uint8_t ADC_SAMPLE_TIME_ADCK19  = kLPADC_SampleTimeADCK19;   // 19 ADCK cycles
constexpr uint8_t ADC_SAMPLE_TIME_ADCK35  = kLPADC_SampleTimeADCK35;   // 35 ADCK cycles
constexpr uint8_t ADC_SAMPLE_TIME_ADCK67  = kLPADC_SampleTimeADCK67;   // 67 ADCK cycles
constexpr uint8_t ADC_SAMPLE_TIME_ADCK131 = kLPADC_SampleTimeADCK131;  // 131 ADCK cycles

// Hardware Average Mode - lpadc_hardware_average_mode_t
constexpr uint8_t ADC_HW_AVERAGE_COUNT1 = kLPADC_HardwareAverageCount1;    // Single conversion
constexpr uint8_t ADC_HW_AVERAGE_COUNT2 = kLPADC_HardwareAverageCount2;    // 2 conversions
                                                                           // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT4 = kLPADC_HardwareAverageCount4;    // 4 conversions
                                                                           // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT8 = kLPADC_HardwareAverageCount8;    // 8 conversions
                                                                           // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT16 = kLPADC_HardwareAverageCount16;  // 16 conversions
                                                                           // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT32 = kLPADC_HardwareAverageCount32;  // 32 conversions
                                                                           // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT64 = kLPADC_HardwareAverageCount64;  // 64 conversions
                                                                           // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT128 = kLPADC_HardwareAverageCount128;  // 128 conversions
                                                                             // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT256 = kLPADC_HardwareAverageCount256;  // 256 conversions
                                                                             // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT512 = kLPADC_HardwareAverageCount512;  // 512 conversions
                                                                             // averaged
constexpr uint8_t ADC_HW_AVERAGE_COUNT1024 = kLPADC_HardwareAverageCount1024;  // 1024
                                                                               // conversions
                                                                               // averaged

// Conversion Resolution Mode - lpadc_conversion_resolution_mode_t
constexpr uint8_t
    ADC_RESOLUTION_STANDARD = kLPADC_ConversionResolutionStandard;  // 12-bit single-ended,
                                                                    // 13-bit differential
constexpr uint8_t ADC_RESOLUTION_HIGH = kLPADC_ConversionResolutionHigh;  // 16-bit
                                                                          // single-ended,
                                                                          // 16-bit differential

// Hardware Compare Mode - lpadc_hardware_compare_mode_t
constexpr uint8_t ADC_COMPARE_DISABLED   = kLPADC_HardwareCompareDisabled;  // Compare disabled
constexpr uint8_t ADC_COMPARE_STORE_TRUE = kLPADC_HardwareCompareStoreOnTrue;  // Store on true
constexpr uint8_t ADC_COMPARE_REPEAT_UNTIL = kLPADC_HardwareCompareRepeatUntilTrue;  // Repeat
                                                                                     // until
                                                                                     // true

// Sample Channel Mode - lpadc_sample_channel_mode_t (MCXN556 uses CTYPE mode)
constexpr uint8_t ADC_CHANNEL_SINGLE_A = kLPADC_SampleChannelSingleEndSideA;  // Single-ended
                                                                              // side A
constexpr uint8_t ADC_CHANNEL_SINGLE_B = kLPADC_SampleChannelSingleEndSideB;  // Single-ended
                                                                              // side B
constexpr uint8_t ADC_CHANNEL_DIFF_AB = kLPADC_SampleChannelDiffBothSide;  // Differential (A-B)
constexpr uint8_t
    ADC_CHANNEL_DUAL_SINGLE = kLPADC_SampleChannelDualSingleEndBothSide;  // Dual single-ended

// Sample Scale Mode - lpadc_sample_scale_mode_t
constexpr uint8_t ADC_SCALE_PARTIAL = kLPADC_SamplePartScale;  // Divided input voltage
constexpr uint8_t ADC_SCALE_FULL    = kLPADC_SampleFullScale;  // Full scale (Factor of 1)

// ADC Status Flags - lpadc_status_flags_t
constexpr uint32_t ADC_FIFO0_READY_FLAG = kLPADC_ResultFIFO0ReadyFlag;  // FIFO 0 has valid data

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
        static_assert(peripheral < std::size(ADC_BASE_ADDRS), "Peripheral out of range");
        static_assert(fifo_num < FSL_FEATURE_LPADC_FIFO_COUNT, "FIFO number out of range");
        const uint32_t raw = std::to_array(ADC_BASE_PTRS)[peripheral]->RESFIFO[fifo_num];
        return raw & ADC_RESFIFO_D_MASK;
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
     * ADC command configuration structure
     * Maps to lpadc_conv_command_config_t with commonly used fields
     * All fields must be explicitly specified by the caller
     */
    struct AdcCommandConfig
    {
        uint8_t  sampleChannelMode;
        uint8_t  channelNumber;
        uint8_t  chainedNextCommandNumber;
        uint16_t hwCompareValueHigh;
        uint16_t hwCompareValueLow;
        uint32_t loopCount;
        uint8_t  sampleTimeMode;
        uint8_t  hardwareAverageMode;
        uint8_t  conversionResolutionMode;
        uint8_t  hardwareCompareMode;
    };
    static void set_adc_command(AdcPeripheral           peripheral,
                                uint32_t                commandId,
                                const AdcCommandConfig& config);

    /**
     * Get temperature sensor calibration parameters
     * These are chip-specific constants defined by NXP SDK
     */
    static constexpr float get_temp_parameter_a() { return FSL_FEATURE_LPADC_TEMP_PARAMETER_A; }

    static constexpr float get_temp_parameter_b() { return FSL_FEATURE_LPADC_TEMP_PARAMETER_B; }

    static constexpr float get_temp_parameter_alpha()
    {
        return FSL_FEATURE_LPADC_TEMP_PARAMETER_ALPHA;
    }

private:
    // returns the hw base address of the ADC
    static ADC_Type* get_base(AdcPeripheral peripheral);
};

}  // namespace sys::adc
