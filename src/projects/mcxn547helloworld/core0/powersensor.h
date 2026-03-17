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

#pragma once

#include "nv/i2c/powersensor/types.h"
#include "nv/gpio/common.h"
#include "nv/i2c/port.h"
#include <array>
#include <cstdint>

// ========== Power Sensor Configuration ==========
namespace nv::i2c::power {

constexpr nv::i2c::Port POWER_SENSOR_PORT = nv::i2c::Port::End;  // (not supported in this
                                                                 // project)

constexpr uint8_t HscVariantCount = 1;
constexpr uint8_t HscDeviceCount  = 0;

// ========== HSC  Sensor List ==========
constexpr std::array<std::array<PowerSensorListConfig, HscDeviceCount>, HscVariantCount>
    HscSensorList{};
// ========== HSCC Sensor List ==========
constexpr uint8_t                                            HsccDeviceCount = 0;
constexpr std::array<PowerSensorListConfig, HsccDeviceCount> HsccSensorList{};
}  // namespace nv::i2c::power