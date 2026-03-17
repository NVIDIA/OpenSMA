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

#include "sys/i2c/i2c_slave.h"

/*
Emulates a PCA9544A 4 channel I2C Mux
*/

namespace nv::emulation {

// PCA9544A Emulator Class:
// -----------------------------

class Pca9544a
{
public:
    // Public Functions
    // ---------------------------------------------

    // default constructor
    Pca9544a();

    // Reads the control register value.
    // Parent task must pull and manage interrupt changes afterwards.
    void i2c_read(uint8_t& return_data);

    // Handles I2C write to the device.
    // Parent task must pull and manage interrupts and output changes afterwards.
    void i2c_write(std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& i2c_data,
                   uint8_t                                            data_length);

    // Returns the current channel
    // This is for parent class to route I2C signals internally
    // Check is_enabled as well for whether to pass traffic or not
    uint8_t get_channel();

    // Returns if I2C traffic is enabled
    bool is_enabled();

    // Sets the interrupt pin for that I2C channel
    // Value is interrupt LOW
    // Though PCA9544a stores interrupts internally as high
    void set_interrupt_pin(uint8_t channel, bool value);

    // Returns the aggregated interrupt output
    // PCA9544a outputs interrupts as interrupt LOW
    // Though it stores interrupts internally as high
    bool get_interrupt();

private:
    // bit 7:4 is interrupt (interrupt on high) input pins
    // bit 3 is unused
    // bit 2 is enable
    // bit 1:0 is channel sel
    uint8_t control_register;

    constexpr static uint8_t InterruptShift = 4U;
    constexpr static uint8_t InterruptMask  = 0b11110000;
    constexpr static uint8_t EnableMask     = 0b00000100;
    constexpr static uint8_t ChannelMask    = 0b00000011;

    constexpr static uint8_t BootState = 0U;
};

}  // namespace nv::emulation
