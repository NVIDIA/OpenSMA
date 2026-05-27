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
 * @file route.h
 * @brief Shared compile-time ADC route table types for the timer-driven volt_mon.
 *
 * Each consumer module (leak_detect, busbar_temp, pgood_volt, mcu_internal_temp)
 * provides a small `*_route.h` header that declares its handler(s) and a
 * `register_routes()` (or `register_route()` for singletons) function that
 * populates an `AdcRouteTable`. `timer/volt_mon.cpp` builds the table once at
 * compile time by calling each family's registration function in turn; it is
 * the only file that knows about the full set of consumers.
 *
 * The dispatch table is indexed by `[adcId][commandIdSource]`, so per-result
 * dispatch is an O(1) array lookup with no runtime branching on family. Adding
 * a new sensor family means adding one new `*_route.h` header and one extra
 * `register_routes()` call in `volt_mon.cpp`; no other timer/ file changes.
 */

#pragma once

#include <array>
#include <cstddef>

#include "nv/volt_mon/common.h"
#include "sys/adc/adc.h"

namespace nv::volt_mon {

using AdcResult = sys::adc::ADC::AdcConvResult;

/**
 * Route handler signature: invoked once per ADC result after the
 * (adcId, commandIdSource) lookup succeeds. Each consumer module wraps its
 * own `process_reading()` in a function matching this signature.
 */
using AdcRouteHandler = void (*)(AdcInstance, const AdcResult&);

struct AdcRoute
{
    AdcInstance     adcId;
    AdcCommand      command;
    AdcRouteHandler handler;
};

constexpr AdcRoute InvalidAdcRoute{AdcInstance::Invalid, AdcCommand::Invalid, nullptr};
constexpr size_t   AdcInstanceCount = static_cast<size_t>(AdcInstance::Total);
constexpr size_t   AdcCommandCount  = static_cast<size_t>(AdcCommand::Invalid);

using AdcRouteTable = std::array<std::array<AdcRoute, AdcCommandCount>, AdcInstanceCount>;

constexpr bool is_valid_route(AdcRoute route)
{
    return route.handler != nullptr && route.adcId < AdcInstance::Total
        && route.command < AdcCommand::Invalid;
}

/**
 * @brief Insert one route into the table at slot `[adcId][command]`.
 *
 * `valid` is set to false if the slot is already occupied (two configured
 * sensors hashing to the same ADC instance + command). Routes whose
 * `is_valid_route()` is false (typically `InvalidAdcRoute` returned by a
 * consumer whose family is disabled in the project config) are silently
 * skipped, leaving `valid` untouched.
 */
constexpr void add_route(AdcRouteTable& table, bool& valid, AdcRoute route)
{
    if (is_valid_route(route)) {
        auto& slot = table.at(static_cast<size_t>(route.adcId))
                         .at(static_cast<size_t>(route.command));
        if (slot.handler != nullptr) {
            valid = false;
            return;
        }
        slot = route;
    }
}

}  // namespace nv::volt_mon
