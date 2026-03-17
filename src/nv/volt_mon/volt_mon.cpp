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

#include "volt_mon.h"

#include NV_IPC_CONFIG_H
#include "adc.h"
#include "busbar_temp.h"
#include "leak_detect.h"
#include "mcu_internal_temp.h"
#include "pgood_volt.h"

using namespace nv::ipc::voltage_monitor_config;

namespace nv::volt_mon {

void init(bool init_mcu_temp,
          bool init_leak_detect,
          bool init_busbar_temp,
          bool init_pgood_volt)
{
    /**
     * Unified voltage monitoring initialization
     *
     * CRITICAL INITIALIZATION ORDER:
     * ===============================
     * All ADC commands MUST be configured BEFORE starting ADC scanning.
     * This function ensures the correct order:
     *
     * Phase 1: Configure ALL ADC commands (ADC stopped)
     * --------------------------------------------------
     * 1. MCU Internal Temperature: Configures temperature sensor ADC command
     * 2. Leak Detect:              Configures leak detection ADC commands
     * 3. Busbar Temp:              Configures busbar temperature ADC commands
     * 4. Pgood Volt:               Configures Power Good voltage ADC commands
     *
     * Phase 2: Start ADC scanning (UNIFIED)
     * --------------------------------------
     * After all modules have configured their commands, this function
     * calls Adc::init() ONCE to start ADC scanning with all commands ready.
     *
     * Why this architecture:
     * ----------------------
     * - Centralized control: Only volt_mon::init() starts ADC
     * - No race conditions: All commands configured before ADC runs
     * - Clean design: Each module only configures its own commands
     * - No runtime updates during init: Commands set once, correctly
     *
     * Runtime updates (after init):
     * ----------------------------
     * After initialization, leak/busbar can safely update their command
     * thresholds while ADC is running (SDK supports this, used in ISRs).
     */

    // Phase 1: Configure all ADC commands (ADC still stopped)
    // --------------------------------------------------------

    // Step 1: MCU Internal Temperature
    // Configure temperature sensor ADC command and trigger
    if constexpr (mcu_internal_temp_get_sensor_config().adcId != AdcInstance::Invalid) {
        if (init_mcu_temp) {
            nv::mcu_internal_temp::McuInternalTemp::inst().init();
        }
    }

    // Step 2: Leak Detect
    // Configure leak detection ADC commands with hardware thresholds
    if constexpr (LeakDetectSensorNum > 0) {
        nv::leak_detect::LeakDetect::inst().init(init_leak_detect);
    }

    // Step 3: Busbar Temp
    // Configure busbar temperature ADC commands with hardware thresholds
    if constexpr (BusBarTempSensorNum > 0) {
        if (init_busbar_temp) {
            nv::busbar_temp::BusbarTemp::inst().init();
        }
    }

    // Step 4: Pgood Volt
    // Configure pgood voltage sensors with hardware thresholds
    if constexpr (PgoodVoltSensorNum > 0) {
        if (init_pgood_volt) {
            nv::pgood_volt::PgoodVolt::inst().init();
        }
    }

    // Phase 2: Start ADC scanning (UNIFIED - called ONCE here)
    // ---------------------------------------------------------
    // All commands are now configured. Start ADC scanning with command chaining.
    // Specific command numbers and FIFO assignments are defined in config.h.
    Adc::init();
}

}  // namespace nv::volt_mon
