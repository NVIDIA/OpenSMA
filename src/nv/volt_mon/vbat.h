/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
 * @file vbat.h
 * @brief VBAT (RTC backup battery) voltage sensor.
 *
 * Consumers must guard calls with
 * `if constexpr (vbat_get_sensor_config().sensor == Sensor::Vbat)` because the
 * implementation is only built with `volt_mon/timer`.
 */

#pragma once

#include <cstdint>

#include "nv/volt_mon/common.h"

#include NV_IPC_CONFIG_H

namespace nv::vbat {

using nv::volt_mon::Reading;
using nv::volt_mon::Status;

// VBAT is connected to ADC input through a resistor divider like so
//
// VBAT (SMM) -- 1k (SMM) -- 5k (HPM) --+-- ADC in (HPM)
//                                      |
//                                   15k (HPM)
//                                      |
//                                     GND
//
constexpr uint32_t DividerTopOhms    = 1000U + 5000U;
constexpr uint32_t DividerBottomOhms = 15000U;

/**
 * @brief VBAT voltage driver class
 */
class Vbat
{
public:
    /**
     * @brief Constructor
     */
    explicit Vbat();

    /**
     * @brief Get the instance of the VBAT voltage driver
     * @return The instance of the VBAT voltage driver
     */
    static Vbat& inst();

    /**
     * @brief Assert VBAT_SENSE_EN ahead of the VBAT conversion.
     *
     * @note Used by the timer-driven implementation only. The pin stays asserted until
     *       stop_sampling() is called.
     */
    void start_sampling() const;

    /**
     * @brief Deassert VBAT_SENSE_EN after the VBAT conversion.
     */
    void stop_sampling() const;

    /**
     * @brief Store one raw ADC reading produced by the VBAT command.
     * @param adcReading Raw ADC reading
     *
     * @note Used by the timer-driven implementation only.
     */
    void process_reading(Reading adcReading);

    /**
     * @brief Get the latest VBAT voltage at the ADC input.
     * @param millivolts Output voltage in mV
     * @return Status::Ok, or Status::AdcIsrNoValidReading before the first sample
     */
    Status get_millivolts(uint32_t& millivolts) const;

private:
    // Single VBAT sensor
    nv::volt_mon::VbatSensor sensor;
};

}  // namespace nv::vbat
