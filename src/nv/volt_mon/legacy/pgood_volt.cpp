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

#include "nv/volt_mon/pgood_volt.h"
#include "nv/nv.h"
#include "nv/volt_mon/adc.h"

namespace nv::pgood_volt {

/**
 * @brief Weak hook called when a Power Good voltage sensor state changes.
 *        Override in board-specific code to drive physical pins (e.g. MCU_THERM_WARN_N).
 */
__attribute__((weak)) void on_pgood_volt_state_changed(Reading adcValue) {}

using namespace nv::ipc::voltage_monitor_config;

template<size_t... Indices>
inline void pgood_volt_init_sensors(nv::volt_mon::PgoodVoltSensor* sensors,
                                    std::index_sequence<Indices...> /*indices*/)
{
    ((sensors[Indices] = pgood_volt_get_sensor_config<Indices>()), ...);
}

PgoodVolt::PgoodVolt() : sensor()
{
    pgood_volt_init_sensors(sensor.data(), std::make_index_sequence<PgoodVoltSensorNum>{});
}

PgoodVolt& PgoodVolt::inst()
{
    static NV_SHARED_DATA PgoodVolt pgoodVolt;
    return pgoodVolt;
}

void PgoodVolt::init()
{
    // Step 1: Initialize ADC Peripheral, Configure Channels, and Setup Interrupts
    initialize_adc();
}

void PgoodVolt::initialize_adc()
{
    /**
     * Configure ADC commands with hardware thresholds
     *
     * CRITICAL: This ONLY configures ADC commands. It does NOT start ADC scanning.
     * ADC will be started later by volt_mon::init() after ALL modules have
     * configured their commands.
     *
     * @note Hardware thresholds are explicitly configured in config.h to ensure
     *       what you see is what you get, eliminating hidden Config Tool settings.
     */
    // coverity[unsigned_compare] - PgoodVoltSensorNum is not 0 once compiled
    for (uint8_t i = 0; i < PgoodVoltSensorNum; ++i) {
        const auto& _sensor = sensor.at(i);
        // Set hardware thresholds from config.h
        update_adccmd_threshold(i, _sensor.pgoodMin, _sensor.pgoodMax);
    }

    // NOTE: ADC scanning will be started by volt_mon::init()
}

void PgoodVolt::update_adccmd_threshold(uint8_t sensorIdx, uint16_t thLow, uint16_t thHigh)
{
    /** sensor id is guaranteed to be valid */

    const auto&                           _sensor = sensor.at(sensorIdx);
    const sys::adc::ADC::AdcCommandConfig newcmd  = {
         .sampleChannelMode        = static_cast<uint8_t>(_sensor.scanMode),
         .channelNumber            = static_cast<uint8_t>(_sensor.channel),
         .chainedNextCommandNumber = static_cast<uint8_t>(_sensor.cmdNext),
         .hwCompareValueHigh       = thHigh,
         .hwCompareValueLow        = thLow,
         .loopCount                = 0,
         .sampleTimeMode           = PgoodVoltAdcSampleTime,
         .hardwareAverageMode      = PgoodVoltAdcHwAverage,
         .conversionResolutionMode = sys::adc::ADC_RESOLUTION_HIGH,
         .hardwareCompareMode      = sys::adc::ADC_COMPARE_STORE_TRUE};
    sys::adc::ADC::set_adc_command(static_cast<uint32_t>(_sensor.adcId),
                                   static_cast<uint32_t>(_sensor.cmdScanning),
                                   newcmd);
}

void PgoodVolt::update_sensor_threshold(uint8_t    sensorIdx,
                                        uint32_t   convValue,
                                        Threshold& thLow,
                                        Threshold& thHigh)
{
    auto&               s      = sensor.at(sensorIdx);
    constexpr Threshold H      = Hysteresis;
    constexpr Threshold adcMin = to_adc_value(MinVol);
    constexpr Threshold adcMax = to_adc_value(MaxVol);

    if (convValue < s.pgoodMin) {
        s.state = State::PgoodLow;
        thLow   = adcMin;
        thHigh  = ((s.pgoodMin + H) <= s.pgoodMax) ? static_cast<Threshold>(s.pgoodMin + H)
                                                   : s.pgoodMax;
    }
    else if (convValue > s.pgoodMax) {
        s.state = State::PgoodHigh;
        thLow   = (s.pgoodMax >= H && (s.pgoodMax - H) >= s.pgoodMin)
                    ? static_cast<Threshold>(s.pgoodMax - H)
                    : s.pgoodMin;
        thHigh  = adcMax;
    }
    else {
        s.state = State::Nominal;
        thLow   = (s.pgoodMin >= H) ? static_cast<Threshold>(s.pgoodMin - H) : adcMin;
        thHigh = ((s.pgoodMax + H) <= adcMax) ? static_cast<Threshold>(s.pgoodMax + H) : adcMax;
    }
    update_adccmd_threshold(sensorIdx, thLow, thHigh);
}

void PgoodVolt::pgood_volt_adc_isr(AdcInstance                         instance,
                                   const sys::adc::ADC::AdcConvResult& result)
{
    static_assert(PgoodVoltSensorNum <= 1, "Multi-sensor not supported");
    constexpr uint8_t sensorIdx = 0;
    Threshold         thLow = 0, thHigh = 0;

    const auto& s = sensor.at(sensorIdx);
    if (s.adcId != instance || s.sensor != Sensor::PgoodVolt
        || static_cast<uint32_t>(s.cmdScanning) != result.commandIdSource) {
        return;
    }

    /**
     * stop adc and reset fifo0
     *
     * @note if trigger mode is one-shot, then adc is already stopped
     *       if trigger mode is scanning, then that means we need to update new threshold
     *       thus, we need to stop adc and reset fifo0 in both cases
     */
    volt_mon::Adc::stop_sampling(instance);

    /* update state and ADC threshold based on current voltage level */
    update_sensor_threshold(sensorIdx, result.convValue, thLow, thHigh);

    /* notify AHS after state is updated */
    on_pgood_volt_state_changed(static_cast<Reading>(result.convValue));

    volt_mon::Adc::dbginfo(
        AdcMode::Scanning, sensor.at(sensorIdx), result.convValue, thLow, thHigh);

    /**
     * restart adc scanning
     *
     * @note adc should always be in scanning mode
     *       it can only be stopped temporarily
     *       if user queries sensor reading
     */
    volt_mon::Adc::start_scanning(instance);
}

}  // namespace nv::pgood_volt
