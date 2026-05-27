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
 * @file mcu_internal_temp_route.h
 * @brief Compile-time ADC route registration for the MCU internal temperature sensor.
 *
 * The MCU has exactly one internal temperature sensor (hardware-fixed), so this
 * registration is a single route — `register_route()` instead of the array-style
 * `register_routes()` used by the other families.
 *
 * The MCU temp ADC command emits two FIFO entries per trigger (VBE1 then VBE8);
 * the handler forwards both with their `loopCountIndex` so the singleton can
 * pair them inside `process_reading()`.
 */

#pragma once

#include "nv/volt_mon/mcu_internal_temp.h"
#include "nv/volt_mon/timer/route.h"

#include NV_IPC_CONFIG_H

namespace nv::mcu_internal_temp {

inline void handle_route([[maybe_unused]] nv::volt_mon::AdcInstance instance,
                         const nv::volt_mon::AdcResult&             result)
{
    McuInternalTemp::inst().process_reading(
        static_cast<nv::volt_mon::Reading>(result.convValue), result.loopCountIndex);
}

constexpr nv::volt_mon::AdcRoute make_route()
{
    using namespace nv::ipc::voltage_monitor_config;
    constexpr auto sensor = mcu_internal_temp_get_sensor_config();
    if constexpr (sensor.sensor == nv::volt_mon::Sensor::McuInternalTemp) {
        return {sensor.adcId, sensor.cmdScanning, handle_route};
    }
    else {
        return nv::volt_mon::InvalidAdcRoute;
    }
}

constexpr void register_route(nv::volt_mon::AdcRouteTable& table, bool& valid)
{
    nv::volt_mon::add_route(table, valid, make_route());
}

}  // namespace nv::mcu_internal_temp
