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

#include "nv/volt_mon/imon.h"

#if defined(NV_VOLT_MON_IMON_ENABLE)

#include <cstddef>
#include <utility>

#include "nv/nv.h"

namespace nv::imon {

using namespace nv::ipc::voltage_monitor_config;

namespace {

template<size_t... Indices>
void init_sensors(nv::volt_mon::ImonSensor* sensors,
                  std::index_sequence<Indices...> /*indices*/)
{
    ((sensors[Indices] = imon_get_sensor_config<Indices>()), ...);
}

}  // namespace

Imon::Imon() : sensors()
{
    init_sensors(sensors.data(), std::make_index_sequence<ImonSensorNum>{});
}

Imon& Imon::inst()
{
    static NV_SHARED_DATA Imon imon;
    return imon;
}

nv::volt_mon::Status Imon::process_reading(uint8_t                             sensorIdx,
                                           const sys::adc::ADC::AdcConvResult& result)
{
    if (sensorIdx >= sensors.size()) {
        return nv::volt_mon::Status::InvalidSensorId;
    }

    auto& sensor   = sensors.at(sensorIdx);
    sensor.reading = static_cast<nv::volt_mon::Reading>(result.convValue);
    sensor.valid   = true;
    return nv::volt_mon::Status::Ok;
}

nv::volt_mon::Status Imon::get_sensor_info(std::span<nv::volt_mon::ImonSensor> info) const
{
    if (info.size() < sensors.size()) {
        return nv::volt_mon::Status::InvalidSensorId;
    }

    for (size_t idx = 0; idx < sensors.size(); ++idx) {
        info[idx] = sensors.at(idx);
    }
    return nv::volt_mon::Status::Ok;
}

}  // namespace nv::imon

#endif  // defined(NV_VOLT_MON_IMON_ENABLE)
