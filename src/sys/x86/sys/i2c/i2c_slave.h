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

class I2CSlaveDriver
{
public:
    // Constants
    // ---------------------------------------------

    // Number of I2C target addresses
    constexpr static size_t NumI2cTargetAddresses = static_cast<size_t>(
        NUM_I2C_TARGET_ADDRESSES);

    // Size of the I2C data buffers (RX/TX). Increase this if transactions involve more data
    constexpr static size_t BufferSize = 34;

    // Callback Types
    // --------------------

    // Will call this function on its master task when there is an I2C write request
    // Parent task must service the buffer data during this function call since it will be
    // overwritten by next I2C transaction Cannot block this driver or will be stuck forever
    typedef void (*process_data_callback_t)(
        uint8_t&                         address,    // Address of the I2C transaction
        bool                             is_read,    // R/W Bit
        std::array<uint8_t, BufferSize>& buffer,     // Buffer for sending/receiving
        size_t&                          data_size,  // Size of the send/receive data
        void*                            task,  // Context for parent task managing this driver
        bool new_transaction  // Indicates a new transaction started (not repeated read ack)
    );

    // Configuration for driver initialization
    struct Config
    {
        nv::i2c::Port           i2c_bus;                // I2C hardware peripheral
        process_data_callback_t process_data_callback;  // Invoked on read requests for transmit
                                                        // data
        std::array<uint8_t, NumI2cTargetAddresses> target_addresses;  // Target addresses to
                                                                      // ACK/NACK
        void* parent_task_class;  // Parent task class for running callbacks
    };

    // Public Functions
    // ---------------------------------------------

    // Constructor: Initializes the I2C driver with the given configuration
    I2CSlaveDriver(Config& driver_config);
    I2CSlaveDriver();

    // Starts the I2C slave driver. Must be called after initialization
    void start();

    // Callback from HW interrups to service data
    // Do Not Use: Reserved for NXP HW API callback and testing.
    static void callback(void* base, void* transfer, void* user_data);

    // Sets the target addresses to ACK/NACK
    void register_target_address(const uint8_t address);
};
}  // namespace sys::i2c
