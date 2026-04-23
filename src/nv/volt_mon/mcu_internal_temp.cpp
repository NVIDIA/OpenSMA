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

#include "mcu_internal_temp.h"
#include "nv/nv.h"
#include "nv/common/debug.h"
#include "nv/logger/log.h"
#include "nv/ctimer/ctimer.h"
#include "nv/volt_mon/adc.h"
#include "sys/adc/adc.h"

namespace nv::mcu_internal_temp {

using namespace nv::ipc::voltage_monitor_config;

McuInternalTemp::McuInternalTemp() : sensor(mcu_internal_temp_get_sensor_config()) {}

McuInternalTemp& McuInternalTemp::inst()
{
    static NV_SHARED_DATA McuInternalTemp mcuInternalTemp;
    return mcuInternalTemp;
}

void McuInternalTemp::init()
{
    /**
     * Configure ADC command for MCU internal temperature sensor
     *
     * IMPORTANT: This is called by volt_mon::init() which manages initialization order.
     * Do NOT call this function directly from main.cpp!
     *
     * Configuration details:
     * - Uses unified VoltMon interface (but with independent trigger, not part of scanning
     * chain)
     * - OneShot mode with independent Trigger 1 (unlike leak_detect/busbar which use Trigger 0
     * chain)
     * - Differential mode for temperature sensor
     * - Long sample time and loop count for accuracy
     * - Does NOT chain to other commands (cmdNext should be None/Invalid)
     *
     * @note Config tool handles ADC peripheral base init (clock, VREF, etc.)
     */

    const sys::adc::ADC::AdcCommandConfig tempSensorCmd = {
        .sampleChannelMode        = static_cast<uint8_t>(sensor.scanMode),
        .channelNumber            = static_cast<uint8_t>(sensor.channel),
        .chainedNextCommandNumber = static_cast<uint8_t>(sensor.cmdNext),
        .hwCompareValueHigh       = 0xFFFF,  // Not used when compare disabled
        .hwCompareValueLow        = 0x0000,  // Not used when compare disabled
        .loopCount                = 1,       // FSL_FEATURE_LPADC_TEMP_SENS_BUFFER_SIZE - 1
        .sampleTimeMode           = nv::volt_mon::TempAdcSampleTime,
        .hardwareAverageMode      = nv::volt_mon::TempAdcHwAverage,
        .conversionResolutionMode = sys::adc::ADC_RESOLUTION_HIGH,
        .hardwareCompareMode = sys::adc::ADC_COMPARE_DISABLED  // Compare disabled: always store
                                                               // all results
    };

    const auto adcId = static_cast<uint32_t>(sensor.adcId);
    const auto cmdId = static_cast<uint32_t>(sensor.cmdOneShot);
    sys::adc::ADC::set_adc_command(adcId, cmdId, tempSensorCmd);

    // Configure Trigger 1 → CMD 15 → FIFO 1 (independent from leak detect)
    sys::adc::ADC::set_adc_trigger(adcId,
                                   static_cast<uint32_t>(sensor.cmdTriggerSrc),
                                   cmdId,
                                   nv::volt_mon::AdcTempFifoSelect);
}

Status McuInternalTemp::get_temperature_celsius(float& temperature)
{
    /**
     * Software trigger approach: Independent trigger with dedicated FIFO
     *
     * Architecture:
     *   Trigger 0: CMD 1 → CMD 2 → ... → CMD N → CMD 1 (leak/busbar chain loop) → FIFO 0
     *   Trigger 1: CMD 15 → END (MCU temp, independent) → FIFO 1
     *
     * Reading strategy:
     *   1. Acquire oneshot lock with RAII guard (prevent conflicts)
     *   2. Stop sampling (disable ADC + reset FIFO 0)
     *   3. Clear FIFO 1 to ensure fresh data
     *   4. Enable ADC (without triggering Trigger 0)
     *   5. Manually trigger Trigger 1 (CMD 15 → FIFO 1)
     *   6. Wait for 2 results (VBE1, VBE8) - ADC auto-stops after completion (cmdNext=None)
     *   7. Read from FIFO 1
     *   8. Restart scanning mode (resume leak detect chain with Trigger 0)
     *   9. Lock automatically released when guard goes out of scope
     */

    // Acquire oneshot lock with RAII guard
    auto guard = nv::volt_mon::Adc::OneShotGuard();
    if (!guard) {
        temperature = AbsoluteZero;
        return Status::AdcOneShotConversionOnGoing;
    }

    const auto                   adcId       = static_cast<uint32_t>(sensor.adcId);
    sys::adc::ADC::AdcConvResult conv_result = {};

    // Stop ADC to ensure Trigger 0 (leak detect chain) doesn't interfere
    nv::volt_mon::Adc::stop_sampling(sensor.adcId);

    // Clear FIFO 1 to ensure fresh data
    sys::adc::ADC::reset_fifo(adcId, nv::volt_mon::AdcTempFifoSelect);

    // Enable ADC (without triggering Trigger 0)
    sys::adc::ADC::enable_adc(adcId, true);

    // Manually trigger Trigger 1 (CMD 15 → FIFO 1)
    sys::adc::ADC::trigger_read(adcId, 1 << static_cast<uint32_t>(sensor.cmdTriggerSrc));

    // Wait for conversion (CMD 15 with loopCount=1 produces 2 results)
    constexpr nv::ctimer::NV_Ticks MaxTimeoutUs   = 10000;  // 10ms timeout
    constexpr nv::ctimer::NV_Ticks WaitIntervalUs = 50;     // Check every 50μs

    for (auto startTime = nv::ctimer::Driver::read_ticks();
         sys::adc::ADC::get_fifo_count(adcId, nv::volt_mon::AdcTempFifoSelect) < 2
         && (nv::ctimer::Driver::read_ticks() - startTime < MaxTimeoutUs);
         nv::ctimer::Driver::delay_for_us(WaitIntervalUs)) {}

    // Check for timeout (ADC auto-stops after CMD 15 completes, cmdNext=None)
    if (sys::adc::ADC::get_fifo_count(adcId, nv::volt_mon::AdcTempFifoSelect) < 2) {
        nv::logger::error(nv::logger::Event::McuTempFifoTimeout,
                          nv::logger::data_from_u32(static_cast<uint32_t>(adcId)));
        temperature = AbsoluteZero;
        // Restart scanning mode before return
        nv::volt_mon::Adc::start_scanning(sensor.adcId);
        return Status::AdcIsrNoValidReading;  // Guard auto-releases lock
    }

    // Read VBE1 and VBE8 (guaranteed to have 2 results in FIFO)
    sys::adc::ADC::get_adc_reading(adcId, conv_result, nv::volt_mon::AdcTempFifoSelect);
    const uint16_t vbe1 = conv_result.convValue >> ConvResultShift;

    sys::adc::ADC::get_adc_reading(adcId, conv_result, nv::volt_mon::AdcTempFifoSelect);
    const uint16_t vbe8 = conv_result.convValue >> ConvResultShift;

    // Calculate temperature using NXP bandgap formula
    temperature = calculate_temperature(vbe1, vbe8);

    // Update sensor temperature cache
    sensor.tempCelsius = temperature;

    // Restart scanning mode (leak detect chain)
    nv::volt_mon::Adc::start_scanning(sensor.adcId);

    return Status::Ok;  // Guard auto-releases lock
}

}  // namespace nv::mcu_internal_temp
