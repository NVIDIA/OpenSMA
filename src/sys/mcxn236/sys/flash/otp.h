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
#include <array>
#include <bitset>
#include <cstdint>
#include <span>
#include <optional>

#include "fsl_efuse.h"

#include "nv/flash/common.h"
#include "nv/nv.h"

namespace sys::flash {
using OptValueT = uint32_t;

typedef struct
{
    uint8_t bugfix; /* bugfix version [7:0] */
    uint8_t minor;  /* minor version [15:8] */
    uint8_t major;  /* major version [23:16] */
    char name; /* character name is "E", where "E" stands for eFuse. Character name and version
                  are fixed */
} EfuseDeviceVersion;

typedef struct
{
    EfuseDeviceVersion version; /* efuse driver API version number */
    status_t (*init)(void);
    status_t (*deinit)(void);
    status_t (*p_efuse_read)(uint32_t addr, uint32_t* data);
    status_t (*p_efuse_program)(uint32_t addr, uint32_t data);
} EfuseDriverT;

// life cycle status on otp for mcxn236
enum class LifeCycleStatus : uint8_t
{
    Blank           = 0b00000000,
    Fab             = 0b00000001,
    Develop         = 0b00000011,
    Develop2        = 0b00000111,
    InField         = 0b00001111,
    InFieldLock     = 0b11001111,
    FieldReturnOEM  = 0b00011111,
    FailureAnalysis = 0b01111111,
    Bricked         = 0b11111111
};

class OtpDriver
{
private:
    /* data */
    static constexpr uint32_t OtpApiAddr = 0x1303fc30U;
    // cache value
    std::optional<LifeCycleStatus> life_cycle_status;

public:
    status_t                  read(const uint32_t address, uint32_t& data);
    status_t                  program(const uint32_t address, const uint32_t data);
    status_t                  read_life_cycle(LifeCycleStatus& data);
    static EfuseDeviceVersion get_version();
    status_t                  init();
    static status_t           read_crc(uint32_t& data);

    OtpDriver(/* args */) : life_cycle_status(std::nullopt){};
    ~OtpDriver() = default;
};
}  // namespace sys::flash