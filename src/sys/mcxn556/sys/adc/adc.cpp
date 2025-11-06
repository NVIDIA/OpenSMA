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

bool ADC::init_adc(AdcPeripheral peripheral)
{
    // TODO move all intiailization code from peripherals.c in here

    // Enable VREF
    SPC0->ACTIVE_CFG1 |= 0x1;

    const uint32_t flags               = get_status_flags(peripheral);
    const bool     calibrationComplete = flags & kLPADC_CalibrationReadyFlag;
    if (!calibrationComplete) {
        nv::warn("ADC Calibration not complete\n");
        return false;
    }

    // using vref0
    /*vref_config_t vref_config;
    VREF_GetDefaultConfig(&vref_config);
    VREF_Init(VREF0, &vref_config);
    // VREF_SetTrim21Val(VREF0, 8U);*/

    ADC_Type* baseAddr = get_base(peripheral);
    LPADC_Enable(baseAddr, true);
    LPADC_DoResetFIFO0(baseAddr);
    LPADC_DoResetFIFO1(baseAddr);
    nv::info("Initialized ADC %d\n", peripheral);
    return true;
}

void ADC::trigger_read(AdcPeripheral peripheral, uint32_t triggerCommand)
{
    ADC_Type* baseAddr = get_base(peripheral);
    LPADC_DoSoftwareTrigger(baseAddr, triggerCommand);
}

uint32_t ADC::get_fifo_count(AdcPeripheral peripheral, uint8_t fifo_num)
{
    ADC_Type* baseAddr = get_base(peripheral);
    return LPADC_GetConvResultCount(baseAddr, fifo_num);
}

bool ADC::pop_fifo(AdcPeripheral peripheral,
                   uint8_t       fifo_num,
                   uint16_t&     result,
                   uint32_t&     command)
{
    ADC_Type*           baseAddr = get_base(peripheral);
    lpadc_conv_result_t adc_return;
    if (!LPADC_GetConvResult(baseAddr, &adc_return, fifo_num)) {
        nv::error("Failed to read ADC0 FIFO %d", fifo_num);
        return false;
    }
    result  = adc_return.convValue;
    command = adc_return.commandIdSource;
    return true;
}

bool ADC::adc_ready(AdcPeripheral peripheral)
{
    ADC_Type* baseAddr = get_base(peripheral);
    return (LPADC_GetStatusFlags(baseAddr) & kLPADC_ActiveFlag) == 0U;
}

uint32_t ADC::get_status_flags(AdcPeripheral peripheral)
{
    ADC_Type* baseAddr = get_base(peripheral);
    return LPADC_GetStatusFlags(baseAddr);
}

ADC_Type* ADC::get_base(AdcPeripheral peripheral)
{
    constexpr uint8_t Size = 2U;
    //  NOLINTNEXTLINE: SDK definition
    std::array<ADC_Type*, Size> bases ADC_BASE_PTRS;
    return bases.at(peripheral);
}

void ADC::reset_fifo(AdcPeripheral peripheral, uint8_t fifo_num)
{
    ADC_Type* baseAddr = get_base(peripheral);

    if (fifo_num == 0) {
        LPADC_DoResetFIFO0(baseAddr);
    }

    if (fifo_num == 1) {
        LPADC_DoResetFIFO1(baseAddr);
    }
}

void ADC::enable_vref()
{
    /* Power up analog module in SPC */
    SPC_EnableActiveModeAnalogModules(SPC0, kSPC_controlVref);
}

void ADC::enable_adc(AdcPeripheral peripheral, bool enable)
{
    ADC_Type* baseAddr = get_base(peripheral);
    LPADC_Enable(baseAddr, enable);
}

void ADC::enable_adc_nvic_interrupt(AdcPeripheral peripheral)
{
    EnableIRQ((peripheral == 0) ? ADC0_IRQn : ADC1_IRQn);
}

void ADC::enable_adc_lpadc_interrupt(AdcPeripheral peripheral, uint32_t mask)
{
    ADC_Type* baseAddr = get_base(peripheral);
    LPADC_EnableInterrupts(baseAddr, mask);
}

void ADC::disable_adc_nvic_interrupt(AdcPeripheral peripheral)
{
    DisableIRQ((peripheral == 0) ? ADC0_IRQn : ADC1_IRQn);
}

void ADC::disable_adc_lpadc_interrupt(AdcPeripheral peripheral, uint32_t mask)
{
    ADC_Type* baseAddr = get_base(peripheral);
    LPADC_DisableInterrupts(baseAddr, mask);
}

void ADC::clear_status_flags(AdcPeripheral peripheral, uint32_t flags)
{
    ADC_Type* baseAddr = get_base(peripheral);
    LPADC_ClearStatusFlags(baseAddr, flags);
}

bool ADC::get_adc_reading(AdcPeripheral peripheral, AdcConvResult& result, uint8_t index)
{
    ADC_Type* baseAddr = get_base(peripheral);

    lpadc_conv_result_t convResult;
    if (!LPADC_GetConvResult(baseAddr, &convResult, index)) {
        return false;
    }

    result.commandIdSource = convResult.commandIdSource;
    result.loopCountIndex  = convResult.loopCountIndex;
    result.triggerIdSource = convResult.triggerIdSource;
    result.convValue       = convResult.convValue;
    return true;
}

void ADC::set_adc_command(AdcPeripheral           peripheral,
                          uint32_t                commandId,
                          const AdcCommandConfig& config)
{
    const lpadc_conv_command_config_t newcmd = {
        .sampleChannelMode = static_cast<lpadc_sample_channel_mode_t>(config.sampleChannelMode),
        .channelNumber     = static_cast<uint32_t>(config.channelNumber),
        .channelBNumber    = 0,
        .chainedNextCommandNumber   = static_cast<uint32_t>(config.chainedNextCommandNumber),
        .enableAutoChannelIncrement = false,
        .loopCount                  = 0,
        .hardwareAverageMode        = kLPADC_HardwareAverageCount128,
        .sampleTimeMode             = kLPADC_SampleTimeADCK3,
        .hardwareCompareMode        = kLPADC_HardwareCompareStoreOnTrue,
        .hardwareCompareValueHigh   = config.hwCompareValueHigh,
        .hardwareCompareValueLow    = config.hwCompareValueLow,
        .conversionResolutionMode   = kLPADC_ConversionResolutionHigh,
        .enableWaitTrigger          = false,
        .enableChannelB             = false};

    ADC_Type* baseAddr = get_base(peripheral);
    LPADC_SetConvCommandConfig(baseAddr, commandId, &newcmd);
}

void ADC::set_adc_trigger(AdcPeripheral peripheral,
                          uint32_t      triggerSrc,
                          uint32_t      targetCommandId)
{
    const lpadc_conv_trigger_config_t triggerConfig = {
        .targetCommandId       = static_cast<uint8_t>(targetCommandId),
        .delayPower            = 0UL,
        .priority              = 0,
        .channelAFIFOSelect    = 0,
        .channelBFIFOSelect    = 0,
        .enableHardwareTrigger = false};
    ADC_Type* baseAddr = get_base(peripheral);
    LPADC_SetConvTriggerConfig(baseAddr, triggerSrc, &triggerConfig);
}

}  // namespace sys::adc
