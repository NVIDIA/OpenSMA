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

#pragma once

#include NV_IPC_CONFIG_H

#if defined(NV_VOLT_MON_IMON_ENABLE)

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "nv/volt_mon/common.h"
#include "sys/adc/adc.h"

namespace nv::imon {

using namespace nv::ipc::voltage_monitor_config;

/**
 * @brief Timer-driven IMON ADC consumer.
 *
 * The product configuration owns the ADC command/channel mapping. This class
 * stores the latest raw ADC code for each rail.
 */
class Imon
{
public:
    Imon();

    static Imon& inst();

    /**
     * @brief Process one routed ADC result.
     *
     * @param sensorIdx Index in the product IMON sensor table.
     * @param result ADC result produced by the configured command.
     * @return Status::Ok on success, otherwise an invalid sensor error.
     */
    nv::volt_mon::Status process_reading(uint8_t                             sensorIdx,
                                         const sys::adc::ADC::AdcConvResult& result);

    /**
     * @brief Copy the latest IMON sensor state.
     *
     * @param info Destination span with room for every configured IMON sensor.
     * @return Status::Ok on success, otherwise an invalid sensor error.
     */
    nv::volt_mon::Status get_sensor_info(std::span<nv::volt_mon::ImonSensor> info) const;

private:
    std::array<nv::volt_mon::ImonSensor, ImonSensorNum> sensors;
};

}  // namespace nv::imon

#endif  // defined(NV_VOLT_MON_IMON_ENABLE)
