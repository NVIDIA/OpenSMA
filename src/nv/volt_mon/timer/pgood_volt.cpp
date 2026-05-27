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

#include "nv/volt_mon/pgood_volt.h"

namespace nv::pgood_volt {

/**
 * @brief Weak hook called when a Power Good voltage sensor state changes.
 *        Override in board-specific code to drive physical pins (e.g. MCU_THERM_WARN_N).
 */
__attribute__((weak)) void on_pgood_volt_state_changed(Reading adcValue) {}

using namespace nv::ipc::voltage_monitor_config;

template<size_t... Indices>
inline void pgood_volt_init_sensors(nv::volt_mon::PgoodVoltSensor* sensors,
                                    std::index_sequence<Indices...> /*indices*/)
{
    ((sensors[Indices] = pgood_volt_get_sensor_config<Indices>()), ...);
}

PgoodVolt::PgoodVolt() : sensor()
{
    pgood_volt_init_sensors(sensor.data(), std::make_index_sequence<PgoodVoltSensorNum>{});
}

PgoodVolt& PgoodVolt::inst()
{
    static NV_SHARED_DATA PgoodVolt pgoodVolt;
    return pgoodVolt;
}

void PgoodVolt::update_sensor_state(uint8_t sensorIdx, Reading adcReading)
{
    auto& s   = sensor.at(sensorIdx);
    s.reading = adcReading;

    if (adcReading < s.pgoodMin) {
        s.state = State::PgoodLow;
    }
    else if (adcReading > s.pgoodMax) {
        s.state = State::PgoodHigh;
    }
    else {
        s.state = State::Nominal;
    }
}

/**
 * PgoodVolt is a single-sensor singleton by hardware design: each board exposes
 * exactly one PGOOD voltage rail to the MCU. The static_assert here guards that
 * contract at compile time and matches the long-standing legacy implementation
 * (legacy/pgood_volt.cpp asserts the same bound). If a future board needs more
 * than one PGOOD rail, the route registration in pgood_volt_route.h already
 * expands by sensor index, so the change is local to this file: replace the
 * hard-coded sensor index with `Index` plumbed through the handler.
 */
void PgoodVolt::process_reading([[maybe_unused]] AdcInstance        instance,
                                const sys::adc::ADC::AdcConvResult& result)
{
    static_assert(PgoodVoltSensorNum <= 1, "Multi-sensor not supported by hardware");
    constexpr uint8_t sensorIdx = 0;

    /* update state based on current voltage level */
    const auto adcReading = static_cast<Reading>(result.convValue);
    update_sensor_state(sensorIdx, adcReading);

    /* notify AHS after state is updated */
    on_pgood_volt_state_changed(adcReading);
}

}  // namespace nv::pgood_volt
