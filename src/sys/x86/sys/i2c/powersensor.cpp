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
#include "nv/i2c/powersensor/sensor.h"

using namespace nv::i2c;

// Stub implementations for x86 platform (unit testing)

I2cStatus PowerSensor::send_command([[maybe_unused]] uint8_t offset)
{
    return I2cStatus::Ok;
}

I2cStatus PowerSensor::write_reg([[maybe_unused]] uint8_t offset,
                                 [[maybe_unused]] uint8_t value)
{
    return I2cStatus::Ok;
}

I2cStatus PowerSensor::write_reg_16bits([[maybe_unused]] uint8_t  offset,
                                        [[maybe_unused]] uint16_t value)
{
    return I2cStatus::Ok;
}

I2cStatus PowerSensor::read_reg([[maybe_unused]] uint8_t  offset,
                                [[maybe_unused]] uint8_t& value)
{
    return I2cStatus::Ok;
}

I2cStatus PowerSensor::read_reg_16bits([[maybe_unused]] uint8_t   offset,
                                       [[maybe_unused]] uint16_t& value)
{
    return I2cStatus::Ok;
}

I2cStatus PowerSensor::read_block([[maybe_unused]] uint8_t            offset,
                                  [[maybe_unused]] std::span<uint8_t> value,
                                  [[maybe_unused]] uint8_t&           length)
{
    return I2cStatus::Ok;
}
