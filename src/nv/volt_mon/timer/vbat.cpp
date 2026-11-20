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

#include "nv/volt_mon/vbat.h"

#include "nv/gpio/driver.h"
#include "nv/nv.h"

namespace nv::vbat {

using namespace nv::ipc::voltage_monitor_config;

Vbat::Vbat() : sensor(vbat_get_sensor_config()) {}

Vbat& Vbat::inst()
{
    static NV_SHARED_DATA Vbat vbat;
    return vbat;
}

void Vbat::start_sampling() const
{
    // The ADC pin is isolated from VBAT to avoid battery discharge through resistor divider
    // The isolation is controlled by the MCU through VBAT_SENSE_EN pin, assert before
    // conversion start
    nv::gpio::Driver::write(sensor.senseEnPort, sensor.senseEnPin, 1U);
}

void Vbat::stop_sampling() const
{
    // Technically it would be more optimal to deassert this on trigger completion, while this
    // runs later in the 50ms timer callback But this would require us to enable ADC interrupts
    // and with the infrequent sampling the cost of discharging the battery for the full 50ms
    // through the divider is negligible
    nv::gpio::Driver::write(sensor.senseEnPort, sensor.senseEnPin, 0U);
}

void Vbat::process_reading(Reading adcReading)
{
    sensor.reading = adcReading;
    sensor.valid   = true;
}

Status Vbat::get_millivolts(uint32_t& millivolts) const
{
    if (!sensor.valid) {
        return Status::AdcIsrNoValidReading;
    }

    const uint32_t adcMillivolts = nv::volt_mon::to_vol_value(sensor.reading);
    millivolts = adcMillivolts * (DividerTopOhms + DividerBottomOhms) / DividerBottomOhms;
    return Status::Ok;
}

}  // namespace nv::vbat
