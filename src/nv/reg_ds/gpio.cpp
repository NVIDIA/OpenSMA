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

#include "nv/reg_ds/gpio.h"

#include "nv/logger/log.h"
#include "nv/gpio/driver.h"

using namespace nv;
using namespace reg_table;
using namespace reg_ds;

bool Gpio::read(Handle handle, DsData& data)
{
    auto index = static_cast<size_t>(handle);
    nv::info("RegGpio read at index %d\n", index);

    // clear data
    std::fill(data.begin(), data.end(), 0);

    // check index against GPIO definition table
    if (index >= _gpio.size()) {
        nv::error("Index out of bounds of gpio table\n");
        return false;
    }
    // bounds check good, get the GPIO definition for this entry
    auto& def = _gpio.at(index);

    // check if index exists in GPIO definition for driver
    if (def >= ipc::GpioSetup.size()) {
        nv::error("GPIO read error is out of bounds of GPIO map\n");
        return false;
    }

    // read the value from the GPIO driver looking up port/pin from driver definition
    uint8_t value     = 0;
    auto& [port, pin] = ipc::GpioSetup.at(def);
    if (gpio::Driver::read(port, pin, value) != gpio::Status::Ok) {
        nv::error("GPIO read error for port %d and pin %d\n", port, pin);
        return false;
    }
    else {
        nv::info("RegGpio read value %d\n", value);
    }

    // place bit in the proper position in the response byte and set valid bit
    constexpr uint8_t GpioLowerByteDataOffs  = 3;
    constexpr uint8_t GpioLowerByteValidOffs = 1;
    data[GpioLowerByteDataOffs]              = value & 1;
    data[GpioLowerByteValidOffs]             = 1;

    return true;
}

// TODO: write GPIO driver and aggregate GPIOs - GFWLYNT1-424
bool Gpio::write(Handle handle, DsData& data)
{
    auto index = static_cast<size_t>(handle);
    nv::info("RegGpio write at index %d\n", index);

    // clear data
    std::fill(data.begin(), data.end(), 0);

    return true;
}
