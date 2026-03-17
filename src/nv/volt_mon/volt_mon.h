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

/**
 * @file volt_mon.h
 * @brief Unified voltage monitoring initialization interface
 *
 * Provides a single entry point for initializing all voltage monitoring modules
 * (leak detect, busbar temp, MCU internal temp). Automatically determines which
 * modules are enabled based on config.h and ensures correct initialization order
 * to avoid ADC command configuration conflicts.
 */

#pragma once

namespace nv::volt_mon {

/**
 * @brief Unified voltage monitoring initialization
 *
 * This function automatically initializes all enabled voltage monitoring modules
 * in the correct order:
 * 1. MCU Internal Temperature - Configures temperature sensor ADC command (if enabled)
 * 2. Leak Detect - Configures leak detection ADC commands (if enabled)
 * 3. Busbar Temp - Configures busbar temperature ADC commands (if enabled)
 * 4. Starts ADC scanning with all commands configured
 *
 * @param init_mcu_temp Enable MCU internal temperature initialization (default: true)
 * @param init_leak_detect Enable leak detection initialization (default: true)
 * @param init_busbar_temp Enable busbar temperature initialization (default: true)
 * @param init_pgood_hotplug Enable hotplug pgood monitor initialization (default: false)
 *
 * @note This is the ONLY function that should be called from main.cpp.
 *       Do NOT call individual module init() functions directly.
 *
 * @note Module enablement is determined at compile-time based on config.h:
 *       - LeakDetectSensorNum > 0 → leak detect enabled
 *       - BusBarTempSensorNum > 0 → busbar temp enabled
 *       - McuInternalTempSensor configured → MCU temp enabled
 */
void init(bool init_mcu_temp,
          bool init_leak_detect,
          bool init_busbar_temp,
          bool init_pgood_hotplug = false);

}  // namespace nv::volt_mon
