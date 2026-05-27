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
#include <cstddef>
#include <span>

#include "common.h"
#include "sys/adc/adc.h"

#include NV_IPC_CONFIG_H

namespace nv {

// Forward declaration - only volt_mon::init() may call module init()
namespace volt_mon {
void init(bool, bool, bool, bool);
}

namespace busbar_temp {

using nv::volt_mon::Reading;
using nv::volt_mon::Threshold;

using namespace nv::ipc::voltage_monitor_config;

/**
 * @brief Busbar temperature driver class
 */
class BusbarTemp
{
    friend void volt_mon::init(bool, bool, bool, bool);

public:
    /**
     * @brief Constructor
     */
    explicit BusbarTemp();

    /**
     * @brief Get the instance of the busbar temperature driver
     * @return The instance of the busbar temperature driver
     */
    static BusbarTemp& inst();

    /**
     * @brief Get sensor information
     * @param info Span of BusBarTempSensor structures to fill
     * @return Status::Ok if successful, error code otherwise
     */
    Status get_sensor_info(std::span<BusBarTempSensor> info);

    /**
     * @brief Set threshold configuration
     * @param sensorIdx Sensor index in the array
     * @param thConfig Threshold configuration
     * @return true if successful, false otherwise
     */
    Status set_thresholds(uint8_t sensorIdx, const ThresholdBusbar& thConfig);

    /**
     * @brief Get current threshold configuration
     * @param sensorIdx Sensor index in the array
     * @param thConfig Threshold configuration
     * @return true if successful, false otherwise
     */
    Status get_thresholds(uint8_t sensorIdx, ThresholdBusbar& thConfig);

    /**
     * @brief Process one ADC reading from the voltage monitor timer callback.
     * @param sensorIdx Sensor index in the array
     * @param adcReading ADC reading value
     * @return Status::Ok if successful, error code otherwise
     *
     * @note Used by the timer-driven implementation only.
     */
    Status process_reading(uint8_t sensorIdx, Reading adcReading);

    /**
     * @brief Get virtual GPIO state (2-bit state)
     * @return 2-bit state: 00=Nominal, 01=NotUsed, 10=NotUsed, 11=Abnormal
     */
    VrGpioState get_virtual_gpio_state() const { return vrGpioState; }

    /**
     * @brief Get aggregate state across all busbar temp sensors
     * @return HighTemp if any sensor is HighTemp, otherwise Nominal
     */
    VrGpioState aggregate_state() const;

    /**
     * @brief ADC interrupt callback
     * @param instance ADC instance that triggered interrupt
     * @param flags ADC interrupt flags
     * @param result ADC conversion result
     */
    void busbar_temp_adc_isr(AdcInstance instance, const sys::adc::ADC::AdcConvResult& result);

private:
    /**
     * @brief Initialize the busbar temperature system
     * @warning Only callable by volt_mon::init(). Direct calls will not start ADC scanning.
     */
    void init();

    // Sensors - Using dedicated BusBarTempSensor structure (refactored from VoltMonitor)
    std::array<BusBarTempSensor, BusBarTempSensorNum> sensor;

    // GPIO states
    VrGpioState vrGpioState;

    /**
     * @brief Initialize ADC peripheral, configure channels, and setup interrupts
     * @return true if successful, false otherwise
     */
    void initialize_adc();

    /**
     * @brief Initialize virtual GPIO
     * @return true if successful, false otherwise
     */
    void initialize_virtual_gpio();

    /**
     * @brief Check thresholds for sensor
     * @param sensorId Sensor ID
     * @param adcReading ADC reading value
     * @return true if threshold violation detected
     */
    Status update_sensor_state(uint8_t sensorId, Reading adcReading);

    /**
     * @brief Get sensor state threshold
     * @param sensorId Sensor ID
     * @param thLow Low threshold
     * @param thHigh High threshold
     * @return true if successful, false otherwise
     */
    void get_sensor_threshold(uint8_t sensorId, Threshold& thLow, Threshold& thHigh);

    /**
     * @brief Update sensor threshold to adc command
     * @param sensorId Sensor ID
     * @param thLow Low threshold
     * @param thHigh High threshold
     * @return true if successful, false otherwise
     */
    void update_adccmd_threshold(uint8_t sensorId, Threshold thLow, Threshold thHigh);

    /**
     * @brief Update virtual GPIO state (2-bit state)
     * @param state New 2-bit state: 00=Nominal, 01=NotUsed, 10=NotUsed, 11=Abnormal
     * @param sensorId Sensor ID
     */
    void update_virtual_gpio(uint8_t sensorId, VrGpioState state, bool trigger_nsm_event);
    void update_virtual_gpio(uint8_t sensorId, VrGpioState state);
};

}  // namespace busbar_temp
}  // namespace nv
