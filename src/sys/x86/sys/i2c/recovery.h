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
#include "nv/i2c/port.h"
#include "nv/i2c/common.h"

namespace sys::i2c {

/*!
 * @brief Performs quick recovery on a specific I2C device.
 *
 * Sends an empty I2C transaction (QUICK command) to the specified device
 * to attempt protocol-level recovery.
 *
 * @param port The I2C port to use
 * @param address The I2C device address to target
 * @return Status of the recovery operation
 */
nv::i2c::I2cStatus quick_recovery(nv::i2c::Port port, uint8_t address);

/*!
 * @brief Performs bus recovery on an I2C port.
 *
 * Generates 9th clock pulses to recover an I2C bus that may be stuck
 * due to a slave device holding SDA low. This affects all devices on
 * the specified I2C bus.
 *
 * @param port The I2C port to recover
 * @return Status of the recovery operation
 */
nv::i2c::I2cStatus bus_recovery(nv::i2c::Port port);

}  // namespace sys::i2c
