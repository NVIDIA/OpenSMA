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
#include <stddef.h>

#include "nv/i2c/port.h"

#include NV_IPC_CONFIG_H

#ifndef NUM_I2C_TARGET_ADDRESSES
#define NUM_I2C_TARGET_ADDRESSES 16
#endif

namespace sys::i2c {

// Number of I2C target addresses
constexpr static size_t NumI2cTargetAddresses = static_cast<size_t>(NUM_I2C_TARGET_ADDRESSES);

// Size of the I2C data buffers (RX/TX). Increase this if transactions involve more data
constexpr static size_t I2cSlaveBufferSize = 35;

using I2cSlaveBuffer = std::array<uint8_t, I2cSlaveBufferSize>;

// Driver Class:
/*
Acts as an I2C Slave on the given I2C bus pins
Abstracts the clock stretching and buffer logic on the NXP I2C HW API

This driver read and write requests are triggered to the master task with callbacks.
    There is only a single buffer so the master task must service the data in the callback
    as the buffer can be overwritten at any time after the callback
Driver will clock stretch for the duration of the callback ensuring no buffer overwriting until
the data is serviced
*/
// -----------------------------------------------------------------------------------------

template<typename T>
class I2CSlaveDriver
{
public:
    // Public Functions
    // ---------------------------------------------

    I2CSlaveDriver() = default;

    // Starts the I2C slave driver. Must be called after initialization
    void start() {}

    // Binds the driver, for cases where driver config is not used
    void bind([[maybe_unused]] nv::i2c::Port                              port,
              [[maybe_unused]] std::array<uint8_t, NumI2cTargetAddresses> target_addresses,
              [[maybe_unused]] T*                                         parent)
    {}

    // Callback from HW interrups to service data
    static void callback([[maybe_unused]] void* base,
                         [[maybe_unused]] void* transfer,
                         [[maybe_unused]] void* user_data)
    {}

    void peripheral_recovery() {}
};
}  // namespace sys::i2c
