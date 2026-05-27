/**
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
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

#include "nv/volt_mon/volt_mon.h"

#include <chrono>

#include NV_IPC_CONFIG_H
#include "nv/common/console.h"
#include "nv/ipc/timer.h"
#include "nv/volt_mon/adc.h"
#include "nv/volt_mon/leak_detect.h"
#include "nv/volt_mon/mcu_internal_temp.h"
#include "nv/volt_mon/timer/busbar_temp_route.h"
#include "nv/volt_mon/timer/leak_detect_route.h"
#include "nv/volt_mon/timer/mcu_internal_temp_route.h"
#include "nv/volt_mon/timer/pgood_volt_route.h"
#include "nv/volt_mon/timer/route.h"

using namespace nv::ipc::voltage_monitor_config;

namespace nv::volt_mon {

namespace {

using namespace std::chrono_literals;
constexpr auto DebugSamplingPeriod = 2s;

struct AdcRouteBuild
{
    AdcRouteTable routes;
    bool          valid;
};

/**
 * @brief Build the ADC dispatch table at compile time.
 *
 * Each consumer module owns its own per-family registration in
 * `timer/<family>_route.h`. This function only knows the registration
 * entry points; it does not see any handler or sensor index expansion.
 *
 * The returned `valid` flag is false when two configured sensors map to the
 * same (adcId, command); the namespace-scope `static_assert` below rejects
 * that case at compile time.
 */
constexpr AdcRouteBuild make_adc_routes()
{
    AdcRouteBuild build{};
    build.valid = true;
    nv::leak_detect::register_routes(build.routes, build.valid);
    nv::busbar_temp::register_routes(build.routes, build.valid);
    nv::pgood_volt::register_routes(build.routes, build.valid);
    nv::mcu_internal_temp::register_route(build.routes, build.valid);
    return build;
}

constexpr auto AdcRouteBuildResult = make_adc_routes();
static_assert(AdcRouteBuildResult.valid,
              "Duplicate VoltMon ADC route: adcId and command must be unique");
constexpr auto AdcRoutes = AdcRouteBuildResult.routes;

/**
 * @brief Dispatch one ADC result using the static adcId+command lookup table.
 *
 * @return true if a route handler was found and called, false otherwise.
 */
bool dispatch_route(AdcInstance instance, const AdcResult& result)
{
    const auto adcIndex = static_cast<size_t>(instance);
    const auto cmdIndex = static_cast<size_t>(result.commandIdSource);

    if (adcIndex >= AdcInstanceCount || cmdIndex >= AdcCommandCount) {
        return false;
    }

    const auto handler = AdcRoutes.at(adcIndex).at(cmdIndex).handler;
    if (handler == nullptr) {
        return false;
    }

    handler(instance, result);
    return true;
}

/**
 * @brief Drain all available ADC results for one ADC instance.
 *
 * Each result is dispatched through the static adcId+command route table.
 *
 * @note Per-sample diagnostics intentionally use Console::print (UART) rather than
 *       nv::logger. At the production tick rate (tens of ms) every sensor produces
 *       a result every tick, which would saturate flash-backed log storage. Console
 *       output is volatile and is the right channel for high-rate trace.
 */
[[maybe_unused]] void drain_adc_results(AdcInstance adcId)
{
    AdcResult result{};
    while (Adc::pop_result(adcId, result)) {
        if constexpr (EnableDbgInfo) {
            nv::common::Console::print(nv::DebugLevel::Info,
                                       "[volt_mon] adc=%d cmd=%d loop=%d trig=%d val=%d\r\n",
                                       static_cast<int>(adcId),
                                       static_cast<int>(result.commandIdSource),
                                       static_cast<int>(result.loopCountIndex),
                                       static_cast<int>(result.triggerIdSource),
                                       static_cast<int>(result.convValue));
        }

        (void)dispatch_route(adcId, result);
    }
}

/**
 * @brief Periodic voltage monitor sampling entry.
 *
 * Drains results from the previous ADC sampling round, dispatches them to route handlers,
 * then triggers the next one-shot sampling round.
 */
void on_timer([[maybe_unused]] nv::ipc::Timer& timer)
{
    if constexpr (EnableDbgInfo) {
        static int timerCount = 0;
        nv::common::Console::print(
            nv::DebugLevel::Info, "[volt_mon] timer %d\r\n", timerCount++);
    }

    // Reset per-tick state before dispatching this round's ADC results.
    if constexpr (mcu_internal_temp_get_sensor_config().sensor == Sensor::McuInternalTemp) {
        nv::mcu_internal_temp::McuInternalTemp::inst().reset_readings();
    }

    if constexpr (SensorOnAdc0) {
        drain_adc_results(AdcInstance::_0);
    }
    if constexpr (SensorOnAdc1) {
        drain_adc_results(AdcInstance::_1);
    }
    if constexpr (SensorOnAdc0) {
        Adc::trigger_sampling(AdcInstance::_0);
    }
    if constexpr (SensorOnAdc1) {
        Adc::trigger_sampling(AdcInstance::_1);
    }
}

}  // namespace

/**
 * @brief Initialize voltage monitor resources and start the periodic sampling timer.
 *
 * @note ADC command chains are configured by the Config Tool. The timer callback owns the
 *       drain-and-trigger sampling loop.
 *
 * @note Only LeakDetect::init() is called explicitly because it has external setup steps
 *       (load thresholds from PDS, initialize the hardware alert GPIO). BusbarTemp,
 *       PgoodVolt, and McuInternalTemp do not need an init() call here: their sensor
 *       arrays are populated by the singleton constructor (`*_init_sensors` fan-out)
 *       and they have no PDS / hardware-pin setup of their own.
 *
 * @note When `EnableDbgInfo` is true the caller-provided `period` is intentionally
 *       overridden with `DebugSamplingPeriod` (2 s). The default production period is
 *       on the order of tens of milliseconds; at that rate the per-sample Console
 *       trace in `drain_adc_results()` is unreadable. This override slows sampling to
 *       a human-observable cadence while debugging. Production builds set
 *       `EnableDbgInfo = false`, so this override is dormant outside debug.
 */
void init(nv::ipc::TimerId timerId, std::chrono::microseconds period)
{
    Adc::init();

    if constexpr (LeakDetectSensorNum > 0) {
        nv::leak_detect::LeakDetect::inst().init();
    }

    if constexpr (EnableDbgInfo) {
        period = DebugSamplingPeriod;
    }

    auto& timer = nv::ipc::Timer::make(timerId, period, on_timer);
    timer.start();
}

}  // namespace nv::volt_mon
