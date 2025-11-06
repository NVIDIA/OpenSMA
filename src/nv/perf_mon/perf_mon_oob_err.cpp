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
#include "nv/perf_mon/perf_mon.h"

using namespace nv;
using namespace nv::perf_mon;

void Driver::set_transaction_error(OobBus bus, uint8_t error_type)
{
    auto& driver = Driver::inst();
    auto  index  = static_cast<uint32_t>(bus);
    if (!driver.is_oob_bus_valid(bus)) {
        return;
    }
    driver.oob_bus_error_buf.at(index).at(error_type) = nv::common::add(
        driver.oob_bus_error_buf.at(index).at(error_type), static_cast<uint32_t>(1));

    driver.oob_bus_error_latched.at(index) = error_type;
}

uint32_t Driver::get_transaction_error(OobBus bus, uint8_t error_type)
{
    auto& driver = Driver::inst();
    auto  index  = static_cast<uint32_t>(bus);
    if (!driver.is_oob_bus_valid(bus)) {
        return Driver::InvalidErrorCount;
    }
    return driver.oob_bus_error_buf.at(index).at(error_type);
}

void Driver::reset_transaction_error(OobBus bus)
{
    auto& driver = Driver::inst();
    auto  index  = static_cast<uint32_t>(bus);
    if (!driver.is_oob_bus_valid(bus)) {
        return;
    }
    driver.oob_bus_error_buf.at(index)     = {};
    driver.oob_bus_error_latched.at(index) = 0;
}
uint8_t Driver::get_latached_error(OobBus bus)
{
    auto& driver = Driver::inst();
    auto  index  = static_cast<uint32_t>(bus);
    if (!driver.is_oob_bus_valid(bus)) {
        return Driver::InvalidErrorType;
    }
    return driver.oob_bus_error_latched.at(index);
}

void Driver::reset_transaction_error_all()
{
    auto& driver                 = Driver::inst();
    driver.oob_bus_error_buf     = {};
    driver.oob_bus_error_latched = {};
}

void Driver::set_oob_bus_valid(OobBus bus)
{
    if (bus < OobBus::End) {
        auto& driver                   = Driver::inst();
        auto  index                    = static_cast<uint32_t>(bus);
        driver.oob_bus_valid.at(index) = true;
    }
}

bool Driver::is_oob_bus_valid(OobBus bus)
{
    if (bus >= OobBus::End) {
        return false;
    }
    auto& driver = Driver::inst();
    auto  index  = static_cast<uint32_t>(bus);
    return driver.oob_bus_valid.at(index);
}
