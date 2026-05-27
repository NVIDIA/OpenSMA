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
 * @file pgood_volt_route.h
 * @brief Compile-time ADC route registration for the Power-Good voltage sensor.
 *
 * PgoodVolt is a single-sensor singleton by hardware design (one PGOOD rail per
 * board). The route registration template still expands by `PgoodIndices...`,
 * so if a future board ever lifts the `PgoodVoltSensorNum <= 1` static_assert
 * in `PgoodVolt::process_reading`, the only change needed here is to plumb
 * `Index` through `handle_route`.
 */

#pragma once

#include <cstddef>
#include <utility>

#include "nv/volt_mon/pgood_volt.h"
#include "nv/volt_mon/timer/route.h"

#include NV_IPC_CONFIG_H

namespace nv::pgood_volt {

template<size_t Index>
inline void handle_route(nv::volt_mon::AdcInstance      instance,
                         const nv::volt_mon::AdcResult& result)
{
    PgoodVolt::inst().process_reading(instance, result);
}

template<size_t Index>
constexpr nv::volt_mon::AdcRoute make_route()
{
    using namespace nv::ipc::voltage_monitor_config;
    constexpr auto sensor = pgood_volt_get_sensor_config<Index>();
    if constexpr (sensor.sensor == nv::volt_mon::Sensor::PgoodVolt) {
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

constexpr void register_routes(nv::volt_mon::AdcRouteTable& table, bool& valid)
{
    using namespace nv::ipc::voltage_monitor_config;
    detail::register_routes_impl(table, valid, std::make_index_sequence<PgoodVoltSensorNum>{});
}

}  // namespace nv::pgood_volt
