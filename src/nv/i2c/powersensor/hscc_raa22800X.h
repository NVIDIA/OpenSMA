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
#include <stdint.h>

namespace nv::i2c {

/**
 * RNS RAA22800X HSCC (Hot Swap Controller with Current sharing) Driver
 * Provides functionality for voltage, current, temperature and power monitoring
 *
 * Data Format: PMBus DIRECT format
 * Conversion formula: x = (1/m) × (Y × 10^(-R) - b)
 * Where: x = real-world value, Y = raw value, m/b/R = coefficients from RNS RAA22800X datasheet
 */
class Raa22800X : public PowerSensor
{
public:
    /**
     * Constructor
     * @param port I2C port to use for communication
     * @param address I2C slave address of the RNS RAA22800X
     */
    Raa22800X(Port port, uint8_t address);
};
}  // namespace nv::i2c
