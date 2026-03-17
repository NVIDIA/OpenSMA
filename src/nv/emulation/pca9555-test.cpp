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

#include <array>
#include "nv/ut/unittest.h"
#include "nv/emulation/pca9555.h"

using namespace nv;
using namespace ut;

class Pca9555Test : public ut::Fixture
{
public:
    void setup() override
    {
        // Initialize with all pins as inputs (0xFFFF)
        pca = emulation::Pca9555(0xFFFF);
    }

public:
    emulation::Pca9555 pca;
};

// Test constructor initialization
TEST_F(Pca9555Test, ConstructorInitialization)
{
    uint16_t pin_states;
    fixture.pca.get_pin_states(pin_states);
    ensure::is_eq(pin_states, 0xFFFF);  // All pins should be high (inputs)

    uint16_t direction_vector;
    fixture.pca.get_pin_directions(direction_vector);
    ensure::is_eq(direction_vector, 0xFFFF);  // All pins should be inputs
};

// Test I2C read operations
TEST_F(Pca9555Test, I2cReadOperations)
{
    uint8_t data;

    // Test reading input register (bank 0)
    fixture.pca.i2c_read(data, true);
    ensure::is_eq(data, 0xFF);  // Should read all high for bank 0

    // Test reading input register (bank 1)
    fixture.pca.i2c_read(data, false);
    ensure::is_eq(data, 0xFF);  // Should read all high for bank 1

    // Test reading output register
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_data = {0x02, 0x55};  // Write to
                                                                                  // output
                                                                                  // register
    fixture.pca.i2c_write(write_data, 3);
    write_data = {0x02};
    fixture.pca.i2c_write(write_data, 1);
    fixture.pca.i2c_read(data, false);  // Read output register
    ensure::is_eq(data, 0x55);          // Should read the value we wrote
};

// Test I2C write operations
TEST_F(Pca9555Test, I2cWriteOperations)
{
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_data;

    // Test writing to output register
    write_data = {0x02, 0x55};  // Write 0x55 to output register
    fixture.pca.i2c_write(write_data, 2);

    uint8_t data;
    write_data = {0x02};
    fixture.pca.i2c_write(write_data, 1);
    fixture.pca.i2c_read(data, false);  // Read output register
    ensure::is_eq(data, 0x55);

    // Test writing to direction register
    write_data = {0x06, 0x0F};  // Set first 4 pins as outputs
    fixture.pca.i2c_write(write_data, 2);

    uint16_t direction_vector;
    fixture.pca.get_pin_directions(direction_vector);
    ensure::is_eq(direction_vector & 0xFF, 0x0F);  // First 4 pins should be outputs (0)
};

// Test input pin updates
TEST_F(Pca9555Test, InputPinUpdates)
{
    // Set all pins as inputs
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_data = {0x06, 0xFF, 0xFF};
    fixture.pca.i2c_write(write_data, 3);

    // Update input pins
    fixture.pca.update_input_pins(0x55AA, 0xFFFF);  // Set alternating pattern

    uint8_t pin_state;
    fixture.pca.get_pin_states(pin_state, emulation::Pca9555Bank::Zero);
    ensure::is_eq(pin_state, 0xAA);
    fixture.pca.get_pin_states(pin_state, emulation::Pca9555Bank::One);
    ensure::is_eq(pin_state, 0x55);
};

// Test interrupt functionality
TEST_F(Pca9555Test, InterruptFunctionality)
{
    // Set all pins as inputs
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_data = {0x06, 0xFF, 0xFF};
    fixture.pca.i2c_write(write_data, 3);

    // Initial state should have no interrupts
    ensure::is_true(fixture.pca.get_interrupt_l_state());

    // Change input state to trigger interrupt
    fixture.pca.update_input_pins(0x55AA, 0xFFFF);
    ensure::is_false(fixture.pca.get_interrupt_l_state());

    // Reading should clear interrupt
    uint8_t data;
    fixture.pca.i2c_read(data, true);
    ensure::is_false(fixture.pca.get_interrupt_l_state());
    fixture.pca.i2c_read(data, false);
    ensure::is_true(fixture.pca.get_interrupt_l_state());
};

// Test pin inversion
TEST_F(Pca9555Test, PinInversion)
{
    // Set all pins as inputs
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_data = {0x06, 0xFF, 0xFF};
    fixture.pca.i2c_write(write_data, 3);

    // Enable inversion for all pins
    write_data = {0x04, 0xFF, 0xFF};
    fixture.pca.i2c_write(write_data, 3);

    // Set input values
    fixture.pca.update_input_pins(0xAA, 0xFF, emulation::Pca9555Bank::Zero);
    fixture.pca.update_input_pins(0x55, 0xFF, emulation::Pca9555Bank::One);

    // Read should return inverted values
    uint8_t data;
    fixture.pca.i2c_read(data, true);   // Read bank 0
    ensure::is_eq(data, 0x55);          // Inverted 0xAA
    fixture.pca.i2c_read(data, false);  // Read bank 1
    ensure::is_eq(data, 0xAA);          // Inverted 0x55
};

// Test required input/output masks
TEST_F(Pca9555Test, RequiredMasks)
{
    // Create PCA9555 with some required inputs and outputs
    emulation::Pca9555 pca_masked(0xFFFF, 0x00FF, 0xFF00);  // First 8 pins required as outputs,
                                                            // last 8 as inputs

    uint16_t direction_vector;
    pca_masked.get_pin_directions(direction_vector);
    ensure::is_eq(direction_vector, 0xFF00);  // First 8 pins should be outputs (0), last 8
                                              // inputs (1)

    // Try to change direction of required pins
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_data = {0x06, 0xFF, 0x00};
    pca_masked.i2c_write(write_data, 3);

    pca_masked.get_pin_directions(direction_vector);
    ensure::is_eq(direction_vector, 0xFF00);  // Direction should not change for required pins
};