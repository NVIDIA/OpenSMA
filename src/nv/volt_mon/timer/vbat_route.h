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
 * @file vbat_route.h
 * @brief Compile-time ADC route registration for the VBAT voltage sensor.
 *
 * The board has a single VBAT rail, so this registration is a single route —
 * `register_route()` instead of the array-style `register_routes()` used by the
 * other families.
 */

#pragma once

#include "nv/volt_mon/timer/route.h"
#include "nv/volt_mon/vbat.h"

#include NV_IPC_CONFIG_H

namespace nv::vbat {

inline void handle_pre_trigger([[maybe_unused]] nv::volt_mon::AdcInstance instance)
{
    Vbat::inst().start_sampling();
}

inline void handle_post_drain([[maybe_unused]] nv::volt_mon::AdcInstance instance)
{
    // This assumes the whole ADC chain can finish within the timer interval (50ms)
    Vbat::inst().stop_sampling();
}

inline void handle_route([[maybe_unused]] nv::volt_mon::AdcInstance instance,
                         const nv::volt_mon::AdcResult&             result)
{
    Vbat::inst().process_reading(static_cast<Reading>(result.convValue));
}

constexpr nv::volt_mon::AdcRoute make_route()
{
    using namespace nv::ipc::voltage_monitor_config;
    constexpr auto sensor = vbat_get_sensor_config();
    if constexpr (sensor.sensor == nv::volt_mon::Sensor::Vbat) {
        return {sensor.adcId,
                sensor.cmdScanning,
                handle_route,
                handle_pre_trigger,
                handle_post_drain};
    }
    else {
        return nv::volt_mon::InvalidAdcRoute;
    }
}

constexpr void register_route(nv::volt_mon::AdcRouteTable& table, bool& valid)
{
    nv::volt_mon::add_route(table, valid, make_route());
}

}  // namespace nv::vbat
