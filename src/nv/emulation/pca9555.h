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
Emulates a PCA9555 GPIO Expander

Abstracts the interrupt quirks and memory managment of a PCA9555
The interface to this is I2C reads / writes, 16 GPIO pins, and 1 interrupt output

takes in i2c reads and writes and changes the input and output pins
requires parent task to:
    handle I2C addressing
    propagate output (interrupt and gpio) pin changes to actual physical GPIOs
    propagate input pin changes into this "device"

*/

namespace nv::emulation {

// PCA9555 Emulator Class:
// --------------------

// Enum representing PCA9555 register types.
enum Pca9555Register
{
    Input,
    Output,
    Invert,
    Direction,
};

// Enum representing the two register banks of PCA9555.
enum Pca9555Bank
{
    Zero,
    One
};

class Pca9555
{
public:
    // Public Functions
    // ---------------------------------------------

    // Constructor for PCA9555 initialization with required configuration masks.
    //     initial_input_values initializes pin states.
    //     required_output_mask and required_input_mask force pins to be a certain
    //     direction which guarantees direction will always be synced with MCU pin config
    Pca9555(uint16_t initial_input_values,
            uint16_t required_output_mask = 0x0000,
            uint16_t required_input_mask  = 0x0000);

    // default constructor
    Pca9555();

    // Reads the active register value.
    // Handles new transactions and interrupts (clearing interrupt upon read).
    // Parent task must pull and manage interrupt changes afterwards.
    void i2c_read(uint8_t& return_data, bool new_transaction);

    // Handles I2C write to the device.
    // Parent task must pull and manage interrupts and output changes afterwards.
    void i2c_write(std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& i2c_data,
                   uint8_t                                                    data_length);

    // Updates GPIO input pins specified by `pin_mask` to the values in `target_pin_values`.
    // - Pins configured as outputs will not be modified and will trigger an error if included
    // in the mask.
    // - Parent task is responsible for handling any interrupt changes resulting from this
    // update.
    void update_input_pins(uint16_t target_pin_values, uint16_t pin_mask);
    // same as above but you can specify the bank
    void update_input_pins(uint8_t target_pin_values, uint8_t pin_mask, Pca9555Bank bank);

    // Returns the current pin states (which reflects the current outputs and inputs)
    // This is for parent class to distribute to various outputs
    // NOT SUITABLE FOR I2C TRANSACTIONS just good for routing pin values as control signals
    // internally NOTE: direction_vector has output = 0, input = 1.
    void get_pin_states(uint16_t& pin_states);
    void get_pin_states(uint8_t& pin_states, Pca9555Bank bank);

    // for parent class to distribute outputs
    // NOTE: direction_vector has output = 0, input = 1.
    void get_pin_directions(uint16_t& direction_vector);

    // Returns if there is an active interrupt
    // Parent class will set the interrupt pin
    // interrupts are ACTIVE LOW
    bool get_interrupt_l_state();

private:
    // Internal Constants
    // ---------------------------------------------
    constexpr static uint8_t InterruptVectorDefault = 0xff;
    constexpr static uint8_t OutputValueInit        = 0xff;
    constexpr static uint8_t InversionEnableInit    = 0x00;
    constexpr static uint8_t DirectionInit          = 0xff;

    // dummy data to send during i2c read errors
    constexpr static uint16_t DummyData = 0xff;

    // Helper Functions
    // ---------------------------------------------

    // Helper function that propagates output and direction changes to the pin state register.
    // Call after modifying output or direction register
    // Also applies any interrupt changes
    void propagate_output_or_direction_change(Pca9555Bank bank);

    // Helper function that updates the interrupt vector after any output, direction, or input
    // pin changes
    //     Removes interrupt if the pin goes back to its original state
    //     sets interrupt if an input pin state changes.
    void update_interrupts(Pca9555Bank bank);

    // Helper function to check and correct violations in required inputs/outputs
    // configurations, then apply to direction register
    void correct_direction_violations_and_apply(uint8_t target_direction, Pca9555Bank bank);

    // Switches to the next bank in the register
    // only 2 for a PCA9555
    void next_bank(Pca9555Bank& bank);

    // Member Variables
    // ---------------------------------------------

    // The active register and bank being accessed (default: INPUT[0]).
    Pca9555Register _active_register;
    Pca9555Bank     _active_bank;

    // Indicates if there is a new command for the next read.
    bool _new_command;

    // Interrupt vector for each bank (bank 0 and 1)
    // Interrupts are active low.
    std::array<uint8_t, 2> interrupt_low_vector;

    // The state of the pin from last interrupt refresh.
    // Used to track if a pin with an active interrupt reverts to its original value
    std::array<uint8_t, 2> state_at_last_interrupt_check;

    // GPIO pin states for both banks in a 2-byte array.
    std::array<uint8_t, 2> _gpio_pin_state;  // Stores pin states for both banks (bank 0 and 1).

    // The target output value for both banks in a 2-byte array.
    std::array<uint8_t, 2> _gpio_output;  // Target output values for both banks.

    // Whether the pins are inverted on a read, for both banks.
    std::array<uint8_t, 2> _gpio_inversion_enable;  // Inversion enable for both banks.

    // Direction configuration for both banks: output is 0, input is 1.
    std::array<uint8_t, 2> _gpio_direction_config;  // Direction config for both banks.

    // Sets required inputs and outputs to not violate MCU pin configurations, for both banks.
    std::array<uint8_t, 2> _required_outputs;  // Required output values for both banks.
    std::array<uint8_t, 2> _required_inputs;   // Required input values for both banks.
};

}  // namespace nv::emulation