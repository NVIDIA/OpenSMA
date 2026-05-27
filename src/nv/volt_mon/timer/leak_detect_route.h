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
 * @file leak_detect_route.h
 * @brief Compile-time ADC route registration for leak detect sensors.
 *
 * The dispatcher (timer/volt_mon.cpp) only sees `register_routes()`; the
 * per-index handler and route construction are owned here.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "nv/volt_mon/leak_detect.h"
#include "nv/volt_mon/timer/route.h"

#include NV_IPC_CONFIG_H

namespace nv::leak_detect {

/**
 * @brief Per-index handler — extracts the ADC reading and forwards it to the
 *        leak-detect singleton with the static sensor index.
 */
template<size_t Index>
inline void handle_route([[maybe_unused]] nv::volt_mon::AdcInstance instance,
                         const nv::volt_mon::AdcResult&             result)
{
    LeakDetect::inst().process_reading(static_cast<uint8_t>(Index),
                                       static_cast<nv::volt_mon::Reading>(result.convValue));
}

/**
 * @brief Build the route entry for a single configured leak-detect sensor.
 *        Returns `InvalidAdcRoute` if the sensor slot is unused (e.g. trailing
 *        slots in projects that disable leak detect via VOLTAGE_MONITOR_DISABLE_*).
 */
template<size_t Index>
constexpr nv::volt_mon::AdcRoute make_route()
{
    using namespace nv::ipc::voltage_monitor_config;
    constexpr auto sensor = leak_detect_get_sensor_config<Index>();
    if constexpr (sensor.sensor == nv::volt_mon::Sensor::LeakDetect) {
        return {sensor.adcId, sensor.cmdScanning, handle_route<Index>};
    }
    else {
        return nv::volt_mon::InvalidAdcRoute;
    }
}

namespace detail {

template<size_t... Indices>
constexpr void register_routes_impl(nv::volt_mon::AdcRouteTable& table,
                                    bool&                        valid,
                                    std::index_sequence<Indices...>)
{
    (nv::volt_mon::add_route(table, valid, make_route<Indices>()), ...);
}

}  // namespace detail

/**
 * @brief Register all configured leak-detect routes into `table`.
 *        `valid` is set to false on duplicate (adcId, command) collisions.
 */
constexpr void register_routes(nv::volt_mon::AdcRouteTable& table, bool& valid)
{
    using namespace nv::ipc::voltage_monitor_config;
    detail::register_routes_impl(table, valid, std::make_index_sequence<LeakDetectSensorNum>{});
}

}  // namespace nv::leak_detect
