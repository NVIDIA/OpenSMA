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

#pragma once

#include <cstdint>

namespace nv::smb_telemetry {

enum class SmbDirectAddress : uint8_t;

// Telemetry type enumeration
enum class TelemetryType : uint8_t
{
    Temperature     = 0,
    Power           = 1,
    Voltage         = 2,
    ProductSpecific = 3,
    Gpio            = ProductSpecific,
    Cpld            = ProductSpecific + 1,
    LeakDetect      = ProductSpecific + 2,
    Smbpbi          = ProductSpecific + 3,
    Cx9_Temp        = ProductSpecific + 4,
    Reserved        = 0xFF
};

// SMBus Direct address to NSM Sensor ID mapping
struct SmbDirectSensorMapping
{
    SmbDirectAddress address;
    TelemetryType    type;
    uint8_t          sensor_id;
    uint8_t          size;  // Data size in bytes
};

}  // namespace nv::smb_telemetry
