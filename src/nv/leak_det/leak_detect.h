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

/**
 * @file leak_detect.h
 * @brief Leak Detection Driver Header
 *
 * This driver implements leak detection using ADC sensors with dynamic threshold
 * management and virtual GPIO functionality.
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <span>

#include "common.h"

#include NV_IPC_CONFIG_H

namespace nv {
namespace leak_detect {

using namespace nv::ipc::leak_detect_config;

/**
 * @brief Leak detection driver class
 */
class LeakDetect
{
public:
    /**
     * @brief Constructor
     */
    explicit LeakDetect();

    /**
     * @brief Get the instance of the leak detection driver
     * @return The instance of the leak detection driver
     */
    static LeakDetect& inst();

    /**
     * @brief Initialize the leak detection system
     * @return true if initialization successful, false otherwise
     */
    void init();

    /**
     * @brief Get sensor information
     * @param info Pointer to sensor info structure
     * @return true if successful, false otherwise
     */
    Status get_sensor_info(std::span<LeakDetectSensor> info);

    /**
     * @brief Set threshold configuration
     * @param sensorIdx Sensor index in the array
     * @param thConfig Threshold configuration
     * @return true if successful, false otherwise
     */
    Status set_thresholds(uint8_t sensorIdx, const ThresholdConfig& thConfig);

    /**
     * @brief Find sensor index by sensor ID
     * @param sensorId Sensor ID to search for
     * @param sensorIdx Output parameter for the found sensor index
     * @return true if sensor found, false otherwise
     */
    Status find_sensor_index(uint8_t sensorId, uint8_t& sensorIdx) const;

    /**
     * @brief Get current threshold configuration
     * @param sensorIdx Sensor index in the array
     * @param thConfig Threshold configuration
     * @return true if successful, false otherwise
     */
    Status get_thresholds(uint8_t sensorIdx, ThresholdConfig& thConfig);

    /**
     * @brief Get virtual GPIO state (2-bit state)
     * @return 2-bit state: 00=Nominal, 01=Leak, 10=Open Fault, 11=Short Fault
     */
    VrGpioState get_virtual_gpio_state() const { return vrGpioState; }

    /**
     * @brief Get hardware GPIO level
     * @return true if leak detected, false otherwise
     */
    HwGpioState get_hardware_gpio_level() const { return hwGpioLevel; }

    /**
     * @brief Start ADC sampling
     * @param adcId ADC ID
     * @return true if successful, false otherwise
     */
    void start_adc_sampling(AdcInstance adcId);

    /**
     * @brief Stop ADC sampling
     * @param adcId ADC ID
     * @return true if successful, false otherwise
     */
    void stop_adc_sampling(AdcInstance adcId);

    /**
     * @brief Get sensor one-shot sampling result
     * @param sensorId Sensor ID
     * @param reading Reading value
     * @return true if successful, false otherwise
     */
    Status start_adc_oneshot(uint8_t sensorId, Reading& reading);

    /**
     * @brief start adc scanning for all sensors on the same adc
     * @param adcId ADC ID
     * @return true if successful, false otherwise
     */
    Status start_adc_scanning(AdcInstance adcId);

    /**
     * @brief reset sensor
     * @param sensorId Sensor ID
     * @return true if successful, false otherwise
     */
    Status reset_sensor(uint8_t sensorId);

    /**
     * @brief ADC interrupt callback
     * @param instance ADC instance that triggered interrupt
     * @param flags ADC interrupt flags
     * @param result ADC conversion result
     */
    void leak_detect_adc_isr(AdcInstance instance);

    /**
     * @brief debug info
     * @param adcId ADC ID
     * @param sensorId Sensor ID
     * @param result ADC result
     *
     * @note remove after debug
     */
    void dbginfo(
        AdcMode adcMode, uint8_t sensorId, Reading reading, Threshold thLow, Threshold thHigh);

private:
    // Sensors
    std::array<LeakDetectSensor, LeakDetectSensorNum>             sensor;
    std::array<Status, static_cast<uint32_t>(AdcInstance::Total)> adcerr;

    // Status
    bool alertActive;

    // GPIO states
    HwGpioState hwGpioLevel;
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
     * @brief Initialize hardware GPIO
     * @return true if successful, false otherwise
     */
    void initialize_hardware_gpio();

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
    void get_sensor_state_threshold(uint8_t sensorId, Threshold& thLow, Threshold& thHigh);

    /**
     * @brief Update sensor threshold to adc command
     * @param sensorId Sensor ID
     * @param thLow Low threshold
     * @param thHigh High threshold
     * @return true if successful, false otherwise
     */
    void update_adccmd_threshold(uint8_t sensorId, Threshold thLow, Threshold thHigh);

    /**
     * @brief Update sensor state
     * @param sensorId Sensor ID
     * @param state New state
     */
    void update_gpio_alert(uint8_t sensorId, State state);

    /**
     * @brief Generate alert
     */
    void generate_alert();

    /**
     * @brief Update virtual GPIO state (2-bit state)
     * @param state New 2-bit state: 00=Nominal, 01=Leak, 10=Open Fault, 11=Short Fault
     */
    void update_virtual_gpio(uint8_t sensorId, VrGpioState state);

    /**
     * @brief Update hardware GPIO level
     * @param state New state
     */
    void update_hardware_gpio(HwGpioState state);

    /**
     * @brief Send NSM event
     * @param event_type Event type
     * @param sensorId Sensor ID (if applicable)
     */
    void send_nsm_event(NsmEventType event_type, uint8_t sensorId = 0xFF);

    /**
     * @brief Convert value to float string
     * @param value Value to convert
     * @return Float string
     */
    const char* to_float_string(uint16_t value);

    /**
     * @brief Check if adc error
     * @param adcId ADC ID
     * @return true if adc error, false otherwise
     */
    bool is_adc_error(AdcInstance adcId) const
    {
        return adcerr.at(static_cast<uint32_t>(adcId)) != Status::Ok;
    };
};

}  // namespace leak_detect
}  // namespace nv
