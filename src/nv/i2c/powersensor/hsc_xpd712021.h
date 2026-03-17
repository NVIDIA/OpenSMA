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

#include "nv/i2c/powersensor/sensor.h"
#include <cstdint>

namespace nv::i2c {

/**
 * @brief Driver for IFX XPD712021 HSC (Hot Swap Controller)
 *
 * This driver supports the Infineon XPD712021 Hot Swap Controller
 * used in PG558 GPU modules. Provides basic monitoring and fault management.
 *
 * I2C Addresses:
 * - West HSC: 0x10
 * - East HSC: 0x11
 */
class Xpd712021 : public PowerSensor
{
public:
    /**
     * @brief I2C addresses for XPD712021 HSC
     */
    static constexpr uint8_t HSC_I2C_ADDR_WEST = 0x10;  // West HSC address
    static constexpr uint8_t HSC_I2C_ADDR_EAST = 0x11;  // East HSC address

    /**
     * @brief Constructor
     * @param port I2C port
     * @param address I2C device address
     */
    Xpd712021(Port port, uint8_t address);
};

}  // namespace nv::i2c
