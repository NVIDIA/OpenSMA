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

#include <chrono>
#include <cstring>
#include <span>

#include "nv/gpio/common.h"

#include "nv/volt_mon/busbar_temp.h"
#include "nv/ctimer/ctimer.h"
#include "nv/nv.h"

#include "nv/gpio/driver.h"
#include "nv/iox/task.h"
#include "nv/ipc/task.h"

#include "nv/volt_mon/adc.h"
#include "nv/logger/log.h"

namespace nv::busbar_temp {

/**
 * @brief Weak hook called when a busbar temperature sensor state changes.
 *        Override in board-specific code to drive physical pins (e.g. MCU_THERM_WARN_N).
 */
__attribute__((weak)) void on_busbar_temp_state_changed() {}

using namespace nv::ipc::voltage_monitor_config;

template<size_t... Indices>
inline void busbar_temp_init_sensors(nv::volt_mon::BusBarTempSensor* sensors,
                                     std::index_sequence<Indices...> /*indices*/)
{
    ((sensors[Indices] = bus_bar_temp_get_sensor_config<Indices>()), ...);
}

BusbarTemp::BusbarTemp() : sensor(), vrGpioState(VrGpioState::Nominal)
{
    busbar_temp_init_sensors(sensor.data(), std::make_index_sequence<BusBarTempSensorNum>{});
}

BusbarTemp& BusbarTemp::inst()
{
    static NV_SHARED_DATA BusbarTemp busbarTemp;
    return busbarTemp;
}

void BusbarTemp::init()
{
    // Step 1: Initialize ADC Peripheral, Configure Channels, and Setup Interrupts
    initialize_adc();

    // Step 2: Initialize Virtual GPIO
    initialize_virtual_gpio();
}

void BusbarTemp::initialize_adc()
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
    // coverity[unsigned_compare] - BusBarTempSensorNum is not 0 once compiled
    for (uint8_t i = 0; i < BusBarTempSensorNum; ++i) {
        const auto& _sensor = sensor.at(i);
        // Set hardware thresholds from config.h
        update_adccmd_threshold(i, _sensor.busBarHighTemp, _sensor.busBarLowTemp);
    }

    // NOTE: ADC scanning will be started by volt_mon::init()
}

void BusbarTemp::initialize_virtual_gpio()
{
    /**
     * virtual gpio is initialized by iox module
     * so NOTHING to do here !!!
     */
}

Status BusbarTemp::update_sensor_state(uint8_t sensorIdx, Reading adcReading)
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorIdx >= BusBarTempSensorNum) {
        return Status::InvalidSensorId;
    }

    auto&      _sensor   = sensor.at(sensorIdx);
    const auto lastState = _sensor.state;
    // NTC: Low ADC = High temp, High ADC = Low temp
    if (adcReading < _sensor.busBarHighTemp) {
        _sensor.state = State::HighTemp;  // ADC too low = temp too high
    }
    else if (adcReading < _sensor.busBarLowTemp) {
        _sensor.state = State::Nominal;
    }
    else {
        _sensor.state = State::LowTemp;  // ADC too high = temp too low (can't happen with NTC)
    }

    const bool stateChanged = (_sensor.state != lastState);
    // Keep transition fact separate from NSM event policy even though they currently match.
    const bool triggerNsmEvent = stateChanged;
    // Always update vrgpio state; only trigger the NSM event conditionally
    // to keep the sync path simple and avoid corner cases.
    update_virtual_gpio(sensorIdx, static_cast<VrGpioState>(_sensor.state), triggerNsmEvent);
    on_busbar_temp_state_changed();

    return Status::Ok;
}

VrGpioState BusbarTemp::aggregate_state() const
{
    for (const auto& _sensor : sensor) {
        if (_sensor.state == State::HighTemp) {
            return VrGpioState::HighTemp;
        }
    }
    return VrGpioState::Nominal;
}

void BusbarTemp::update_virtual_gpio(uint8_t     sensorIdx,
                                     VrGpioState state,
                                     bool        trigger_nsm_event)
{
    // update internal virtual gpio state
    vrGpioState = state;

    // update iox virtual gpio values
    auto& _sensor = sensor.at(sensorIdx);

    // BusBarTempSensor uses 1 pin: 0=nominal, 1=fault
    const std::array<uint8_t, 1> ioxPinVals = {(state == VrGpioState::Nominal) ? uint8_t{0}
                                                                               : uint8_t{1}};

    nv::iox::Task::send_vrgpio_request(_sensor.ioxAddr,
                                       nv::iox::Operation::Write,
                                       _sensor.ioxPin,
                                       ioxPinVals,
                                       trigger_nsm_event);
}

// Public interface implementations
Status BusbarTemp::get_sensor_info(std::span<BusBarTempSensor> info)
{
    auto status = Status::Ok;

    for (size_t i = 0; i < sensor.size(); ++i) {
        const auto& adcId = sensor.at(i).adcId;
        if (volt_mon::Adc::is_adc_error(adcId)) {
            status = volt_mon::Adc::adcerr.at(static_cast<uint32_t>(adcId));
            break;
        }

        // start adc one-shot sampling
        status = volt_mon::Adc::start_oneshot(sensor.at(i));
        if (status != Status::Ok) {
            break;
        }

        // update sensor state based on current reading
        update_sensor_state(i, sensor.at(i).reading);

        // Return BusBarTempSensor directly
        info[i] = sensor.at(i);

        volt_mon::Adc::dbginfo(AdcMode::OneShot,
                               sensor.at(i),
                               sensor.at(i).reading,
                               sensor.at(i).busBarHighTemp,
                               sensor.at(i).busBarLowTemp);
    }

    /**
     * if no error, start adc scanning for all sensors
     * although sensors may not be on the same adc
     * for simplicity, we start adc scanning for all adcs
     */
    if (status == Status::Ok) {
        if constexpr (SensorOnAdc0) {
            volt_mon::Adc::start_scanning(AdcInstance::_0);
        }
        if constexpr (SensorOnAdc1) {
            volt_mon::Adc::start_scanning(AdcInstance::_1);
        }
    }

    return status;
}

Status BusbarTemp::get_thresholds(uint8_t sensorIdx, ThresholdBusbar& config)
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorIdx >= BusBarTempSensorNum) {
        return Status::InvalidSensorId;
    }

    config.busBarHighTemp = sensor.at(sensorIdx).busBarHighTemp;
    config.busBarLowTemp  = sensor.at(sensorIdx).busBarLowTemp;

    return Status::Ok;
}

Status BusbarTemp::set_thresholds(uint8_t sensorIdx, const ThresholdBusbar& config)
{
    // public API — caller (e.g. NSM handler) handles error response
    // coverity[unsigned_compare] - BusBarTempSensorNum is not 0 once compiled
    if (sensorIdx >= BusBarTempSensorNum) {
        return Status::InvalidSensorId;
    }

    /**
     * simple check if the thresholds are valid
     * NTC: busBarHighTemp (low ADC) < busBarLowTemp (high ADC)
     */
    if (!((config.busBarHighTemp < config.busBarLowTemp)
          && (config.busBarLowTemp <= to_adc_value(MaxVol)))) {
        return Status::InvalidThreshold;
    }

    auto& _sensor = sensor.at(sensorIdx);

    /**
     * stop adc first as we need to update threshold to adc command
     */
    volt_mon::Adc::stop_sampling(_sensor.adcId);

    /**
     * update threshold to sensor struct
     */
    _sensor.busBarHighTemp = nv::volt_mon::to_adc_value(config.busBarHighTemp);
    _sensor.busBarLowTemp  = nv::volt_mon::to_adc_value(config.busBarLowTemp);

    /**
     * update threshold to adc command
     *
     * @note adc was just stopped, and we update the normal low and high thresholds
     *       and set adc state to nominal temporarily.
     *
     *       The caller is responsible for reading the sensor value and updating
     *       the actual state and GPIO alerts, then restarting ADC scanning.
     */
    _sensor.state = State::Nominal;
    update_adccmd_threshold(sensorIdx, _sensor.busBarHighTemp, _sensor.busBarLowTemp);

    /**
     * @note ADC is left in stopped state. The caller must:
     *       1. Read sensor value to determine actual state
     *       2. Update GPIO alerts based on actual state
     *       3. Restart ADC scanning mode
     */

    return Status::Ok;
}

void BusbarTemp::get_sensor_threshold(uint8_t sensorIdx, uint16_t& thLow, uint16_t& thHigh)
{
    /** sensor id is guaranteed to be valid */

    const auto& _sensor = sensor.at(sensorIdx);
    switch (_sensor.state) {
        case State::HighTemp:
            // ADC too low (temp too high), detect ADC rising back to Nominal
            thLow  = to_adc_value(MinVol);
            thHigh = _sensor.busBarHighTemp;
            break;
        case State::Nominal:
            // Normal range, detect crossing either threshold
            thLow  = _sensor.busBarHighTemp;
            thHigh = _sensor.busBarLowTemp;
            break;
        case State::LowTemp:
            // ADC too high (temp too low), detect ADC falling back to Nominal
            thLow  = _sensor.busBarLowTemp;
            thHigh = to_adc_value(MaxVol);
            break;
        default:
            nv::error("BusbarTemp::get_sensor_threshold: unknown state=%d\r\n", _sensor.state);
            break;
    }
}

void BusbarTemp::update_adccmd_threshold(uint8_t sensorIdx, uint16_t thLow, uint16_t thHigh)
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
         .sampleTimeMode           = BusbarAdcSampleTime,
         .hardwareAverageMode      = BusbarAdcHwAverage,
         .conversionResolutionMode = sys::adc::ADC_RESOLUTION_HIGH,
         .hardwareCompareMode      = sys::adc::ADC_COMPARE_STORE_TRUE};
    sys::adc::ADC::set_adc_command(static_cast<uint32_t>(_sensor.adcId),
                                   static_cast<uint32_t>(_sensor.cmdScanning),
                                   newcmd);
}

void BusbarTemp::busbar_temp_adc_isr(AdcInstance                         instance,
                                     const sys::adc::ADC::AdcConvResult& result)
{
    nv::logger::Logger::add_from_isr(nv::logger::Event::BusbarTempIsr.unique_id,
                                     nv::logger::Event::BusbarTempIsr.default_level,
                                     volt_mon::make_adc_isr_log_data(instance,
                                                                     result.commandIdSource,
                                                                     result.triggerIdSource,
                                                                     result.loopCountIndex,
                                                                     result.convValue),
                                     nv::logger::OutputDirection::Flash);

    uint8_t   sensorIdx = 0;
    Threshold thLow = 0, thHigh = 0;

    /**
     * identify sensor id and trigger mode from command id
     *
     * @note we have very limited number of sensors, so just loop through
     */
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    for (; sensorIdx < BusBarTempSensorNum; sensorIdx++) {
        // coverity[dead_error_line] - LeakDetectSensorNum is not 0 once compiled
        if (static_cast<uint32_t>(sensor.at(sensorIdx).cmdScanning) == result.commandIdSource) {
            break;
        }
    }

    if (sensorIdx == BusBarTempSensorNum || sensor.at(sensorIdx).sensor != Sensor::BusBarTemp) {
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

    /* update sensor state due to voltage out of range */
    // coverity[dead_error_begin] - BusBarTempSensorNum is not 0 once compiled
    update_sensor_state(sensorIdx, static_cast<Reading>(result.convValue));

    /* get new threshold based on new state */
    get_sensor_threshold(sensorIdx, thLow, thHigh);

    /* update new threshold to sensor's adc command */
    update_adccmd_threshold(sensorIdx, thLow, thHigh);

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

}  // namespace nv::busbar_temp
