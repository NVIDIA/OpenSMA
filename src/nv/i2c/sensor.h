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
#include <span>
#include <stdint.h>
#include "nv/i2c/common.h"
#include "nv/i2c/port.h"
#include "nv/telemetry/cache.h"

namespace nv::i2c {
class TempSensor
{
public:
    enum Register
    {
        RemoteTempLimit = 0x19,
        LocalTempLimit  = 0x20,
    };

    enum class Status : uint8_t
    {
        SetRegister,
        LocalLimit,
        RemoteLimit,
    };

    TempSensor(Port port, uint8_t address, nv::telemetry::TelemId item);
    I2cStatus get_id(uint8_t& id);
    I2cStatus get_temp(uint8_t& temp);
    I2cStatus get_CX8_temp(uint8_t& temp);
    void      update_cache();
    I2cStatus read_reg(uint8_t offset, uint8_t& value);
    I2cStatus write_reg(uint8_t offset, uint8_t value);

private:
    Port                   _port;
    uint8_t                _address;
    nv::telemetry::TelemId _item;
};

}  // namespace nv::i2c
