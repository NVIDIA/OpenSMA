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

#include <array>
#include <cstddef>
#include <utility>

#include "nv/volt_mon/busbar_temp.h"
#include "nv/logger/log.h"
#include "nv/nv.h"

#include "nv/iox/task.h"

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

Status BusbarTemp::process_reading(uint8_t sensorIdx, Reading adcReading)
{
    // coverity[unsigned_compare] - BusBarTempSensorNum is not 0 once compiled
    if (sensorIdx >= BusBarTempSensorNum) {
        return Status::InvalidSensorId;
    }

    auto&      _sensor   = sensor.at(sensorIdx);
    const auto lastState = _sensor.state;
    _sensor.reading      = adcReading;
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

    if (_sensor.state != lastState) {
        nv::logger::info(nv::logger::Event::BusbarTempStateChange,
                         nv::volt_mon::make_state_transition_log_data(
                             sensorIdx, lastState, _sensor.state, adcReading),
                         nv::logger::OutputDirection::Flash);

        // Only push to IOX / hardware pin / NSM on a real transition. Re-asserting the
        // same vrgpio state every tick burns I2C bandwidth and floods the IOX queue.
        update_virtual_gpio(sensorIdx, static_cast<VrGpioState>(_sensor.state));
        on_busbar_temp_state_changed();
    }

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

void BusbarTemp::update_virtual_gpio(uint8_t sensorIdx, VrGpioState state)
{
    // update internal virtual gpio state
    vrGpioState = state;

    if constexpr (nv::ipc::EnableIoxEmulation) {
        // update iox virtual gpio values
        auto& _sensor = sensor.at(sensorIdx);

        // BusBarTempSensor uses 1 pin: 0=nominal, 1=fault
        const std::array<uint8_t, 1> ioxPinVals = {
            (state == VrGpioState::Nominal) ? uint8_t{0} : uint8_t{1}};

        if (!nv::iox::Task::send_vrgpio_request(_sensor.ioxAddr,
                                                nv::iox::Operation::Write,
                                                _sensor.ioxPin,
                                                ioxPinVals,
                                                /*trigger_nsm_event=*/true)) {
            nv::logger::error_no_wait(
                nv::logger::Event::BusbarTempVrgpioUpdateFail,
                {sensorIdx, _sensor.ioxAddr, _sensor.ioxPin.at(0), static_cast<uint8_t>(state)},
                nv::logger::OutputDirection::Flash);
        }
    }
}

// Public interface implementations
Status BusbarTemp::get_sensor_info(std::span<BusBarTempSensor> info)
{
    for (size_t i = 0; i < sensor.size(); ++i) {
        info[i] = sensor.at(i);
    }

    return Status::Ok;
}

Status BusbarTemp::get_thresholds(uint8_t sensorIdx, ThresholdBusbar& config)
{
    // coverity[unsigned_compare] - BusBarTempSensorNum is not 0 once compiled
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

    auto& _sensor          = sensor.at(sensorIdx);
    _sensor.busBarHighTemp = nv::volt_mon::to_adc_value(config.busBarHighTemp);
    _sensor.busBarLowTemp  = nv::volt_mon::to_adc_value(config.busBarLowTemp);

    return Status::Ok;
}

}  // namespace nv::busbar_temp
