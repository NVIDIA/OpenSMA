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
#include "common.h"
#include "sys/adc/adc.h"

#include NV_IPC_CONFIG_H

namespace nv {

// Forward declaration - only volt_mon::init() may call module init()
namespace volt_mon {
void init(bool, bool, bool, bool);
}

namespace pgood_volt {

using nv::volt_mon::MaxVol;
using nv::volt_mon::MinVol;
using nv::volt_mon::Reading;
using nv::volt_mon::Threshold;
using nv::volt_mon::to_adc_value;

using namespace nv::ipc::voltage_monitor_config;

constexpr uint16_t  HysteresisMv = 20;
constexpr Threshold Hysteresis   = to_adc_value(HysteresisMv);

/**
 * @brief Pgood voltage driver class
 */
class PgoodVolt
{
    friend void volt_mon::init(bool, bool, bool, bool);

public:
    /**
     * @brief Constructor
     */
    explicit PgoodVolt();

    /**
     * @brief Get the instance of the Power Good voltage driver
     * @return The instance of the Power Good voltage driver
     */
    static PgoodVolt& inst();

    /**
     * @brief ADC interrupt callback
     * @param instance ADC instance that triggered interrupt
     * @param result ADC conversion result
     */
    void pgood_volt_adc_isr(AdcInstance instance, const sys::adc::ADC::AdcConvResult& result);

    /**
     * @brief Process one ADC reading from the voltage monitor timer callback.
     * @param instance ADC instance that produced the reading
     * @param result ADC conversion result
     *
     * @note Used by the timer-driven implementation only.
     */
    void process_reading(AdcInstance instance, const sys::adc::ADC::AdcConvResult& result);

private:
    /**
     * @brief Initialize the pgood voltage system
     * @warning Only callable by volt_mon::init(). Direct calls will not start ADC scanning.
     */
    void init();

    // Sensors - Using dedicated PgoodVoltSensor structure (refactored from VoltMonitor)
    std::array<PgoodVoltSensor, PgoodVoltSensorNum> sensor;

    /**
     * @brief Initialize ADC peripheral, configure channels, and setup interrupts
     */
    void initialize_adc();

    /**
     * @brief Update sensor threshold to adc command
     * @param sensorIdx Sensor index
     * @param thLow Low threshold
     * @param thHigh High threshold
     */
    void update_adccmd_threshold(uint8_t sensorIdx, Threshold thLow, Threshold thHigh);
    void update_sensor_threshold(uint8_t    sensorIdx,
                                 uint32_t   convValue,
                                 Threshold& thLow,
                                 Threshold& thHigh);
    void update_sensor_state(uint8_t sensorIdx, Reading adcReading);
};

}  // namespace pgood_volt
}  // namespace nv
