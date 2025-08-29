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
#include <stdint.h>
#include <array>
#include <span>

namespace nv::telemetry {

enum TelemId : uint8_t
{
    Gpu1Temp,
    Gpu2Temp,
    Gpu1Power,
    Gpu2Power,
    ModulePower,
    ModuleTemp1,
    ModuleTemp2,
    InternalTemp,
    MaxModuleTemp,
    Gpio,
    CX8_1_Temp,
    CX8_2_Temp,
    MaxItem
};

uint32_t buffer_to_uint32(std::span<uint8_t> buffer);

}  // namespace nv::telemetry
