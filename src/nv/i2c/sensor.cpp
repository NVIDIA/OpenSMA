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
#include "nv/i2c/sensor.h"
#include "nv/nv.h"

using namespace nv::i2c;

TempSensor::TempSensor(Port port, uint8_t address, nv::telemetry::TelemId item)
: _port(port)
, _address(address)
, _item(item)
{}

I2cStatus TempSensor::get_id(uint8_t& id)
{
    constexpr uint8_t IdOffset = 0xFE;
    return read_reg(IdOffset, id);
}

I2cStatus TempSensor::get_temp(uint8_t& temp)
{
    constexpr uint8_t TempOffset = 0x00;
    auto              status     = read_reg(TempOffset, temp);
    return status;
}

I2cStatus TempSensor::get_CX8_temp(uint8_t& temp)
{
    // From TM421 datasheet p.13 remote temp high byte offset is 0x01
    constexpr uint8_t TempOffset = 0x01;
    auto              status     = read_reg(TempOffset, temp);

    // Temperature conversion from CX8 raw data to Celsius. Conversion formula found in TM421
    // datasheet p.12
    constexpr uint8_t BitMask  = 0x0F;
    constexpr uint8_t BitShift = 4;
    temp                       = (temp & BitMask) + ((temp >> BitShift) & BitMask) * 16;

    return status;
}

void TempSensor::update_cache()
{
    using namespace nv::telemetry;
    uint8_t   temp   = 0;
    I2cStatus status = I2cStatus::Ok;
    if (_item == TelemId::CX8_1_Temp || _item == TelemId::CX8_2_Temp) {
        status = get_CX8_temp(temp);
    }
    else {
        status = get_temp(temp);
    }

    const Cache::Value value = status == I2cStatus::Ok ? temp : Cache::InvalidItem;
    Cache::inst().set_cache(_item, value);
    if (status != I2cStatus::Ok) {
        Cache::inst().set_error(Cache::ErrorItem::I2cSensorAlert);
    }
}
