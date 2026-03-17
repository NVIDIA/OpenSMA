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

#include <numeric>
#include <stdio.h>

#include "nv/ut/unittest.h"
#include "nv/emulation/at24c01.h"

using namespace nv;
using namespace ut;

/**
 * @brief Unit test fixture for testing AT24C01 EEPROM emulation functionality
 *
 * This test class provides test cases for verifying the AT24C01 EEPROM emulation
 * implementation, including constructor behavior, I2C read/write operations,
 * and memory initialization with proper EEPROM behavior.
 */
class at24c01test : public ut::Fixture
{
public:
    /**
     * @brief Sets up test fixtures before each test case
     *
     * Initializes two EEPROM instances for testing:
     * - Default constructor EEPROM (all memory initialized to 0xFF)
     * - EEPROM with sequential data (0-127) for testing read operations
     */
    void setup() override
    {
        eeprom = emulation::At24c01();
        // Create another instance for testing different scenarios
        eeprom2      = emulation::At24c01();
        auto memory2 = eeprom2.get_memory();
        std::iota(memory2.begin(), memory2.end(), 0);
        eeprom2.set_memory(memory2);
    }

    /**
     * @brief Cleans up test fixtures after each test case
     *
     * Currently no cleanup is required as EEPROM objects are automatically
     * destroyed when the test fixture goes out of scope.
     */
    void teardown() override {}

public:
    /** @brief Default EEPROM instance for testing default constructor */
    emulation::At24c01 eeprom;
    /** @brief EEPROM instance with sequential data for testing read operations */
    emulation::At24c01 eeprom2;
};

/**
 * @brief Test case for EEPROM constructor functionality
 *
 * Verifies that EEPROM constructors properly initialize memory with expected
 * values. Tests both default constructor (0xFF initialization) and custom
 * memory setup to ensure correct memory layout and initialization.
 */
TEST_F(at24c01test, test_eeprom_constructor)
{
    auto memory  = fixture.eeprom.get_memory();
    auto memory2 = fixture.eeprom2.get_memory();
    ensure::is_eq(memory.size(), 128);
    ensure::is_eq(memory.at(0), 0xff);
    ensure::is_eq(memory.at(127), 0xff);
    ensure::is_eq(memory2.size(), 128);
    ensure::is_eq(memory2.at(0), 0);
    ensure::is_eq(memory2.at(127), 127);

    // Test that all memory locations are initialized to 0xFF (erased state) and 0-127
    for (size_t i = 0; i < 128; i++) {
        ensure::is_eq(memory.at(i), 0xff);
        ensure::is_eq(memory2.at(i), i);
    }
};

/**
 * @brief Test case for EEPROM I2C read operations
 *
 * Verifies that I2C read operations work correctly, including sequential reads
 * and address wrapping. Tests both default memory (0xFF) and custom memory
 * to ensure proper read behavior and address pointer management.
 */
TEST_F(at24c01test, test_eeprom_i2c_read)
{
    // Test sequential read operations
    uint8_t data = 0x00;

    // First read should start from address 0
    fixture.eeprom.i2c_read(data, true);
    ensure::is_eq(data, 0xff);  // Should read 0xFF from address 0

    // Second read should be from address 1
    fixture.eeprom.i2c_read(data, false);
    ensure::is_eq(data, 0xff);  // Should read 0xFF from address 1

    // Third read should be from address 2
    fixture.eeprom.i2c_read(data, false);
    ensure::is_eq(data, 0xff);  // Should read 0xFF from address 2

    // Ensure remaining data is 0xFF
    for (int i = 0; i < 125; i++) {
        fixture.eeprom.i2c_read(data, false);
        ensure::is_eq(data, 0xff);  // Should read 0xFF from all addresses
    }

    // Test address wrapping with eeprom2
    auto memory2 = fixture.eeprom2.get_memory();
    for (int i = 0; i < 128; i++) {
        fixture.eeprom2.i2c_read(data, false);
        ensure::is_eq(data, memory2.at(i));  // Should read the correct data from address i
    }
    fixture.eeprom2.i2c_read(data, false);
    ensure::is_eq(data, memory2.at(0));  // Should read the correct data from address 0
};

/**
 * @brief Test case for EEPROM I2C write operations
 *
 * Verifies that I2C write operations work correctly, including address setting,
 * data writing, and page boundary handling. Tests writing to different addresses
 * and ensures proper address pointer management.
 */
TEST_F(at24c01test, test_eeprom_i2c_write)
{
    // Test writing data to specific addresses
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_buffer = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

    // Write 8 bytes starting at address 0
    fixture.eeprom.i2c_write(write_buffer, 8);

    // Verify the data was written correctly
    auto memory = fixture.eeprom.get_memory();
    ensure::is_eq(memory.at(0), 0x01);  // First data byte at address 0
    ensure::is_eq(memory.at(1), 0x02);  // Second data byte at address 1
    ensure::is_eq(memory.at(2), 0x03);  // Third data byte at address 2
    ensure::is_eq(memory.at(3), 0x04);  // Fourth data byte at address 3
    ensure::is_eq(memory.at(4), 0x05);  // Fifth data byte at address 4
    ensure::is_eq(memory.at(5), 0x06);  // Sixth data byte at address 5
    ensure::is_eq(memory.at(6), 0x07);  // Seventh data byte at address 6

    // Test writing to a different address
    write_buffer.at(0) = 0x10;  // New address
    write_buffer.at(1) = 0xAA;
    write_buffer.at(2) = 0xBB;
    fixture.eeprom.i2c_write(write_buffer, 3);

    // Verify data was written to the new address
    memory = fixture.eeprom.get_memory();
    ensure::is_eq(memory.at(0x10), 0xAA);
    ensure::is_eq(memory.at(0x11), 0xBB);

    // Test page boundary handling - write across page boundary
    write_buffer.at(0) = 0x07;  // Start at address 7 (end of first page)
    write_buffer.at(1) = 0xCC;
    write_buffer.at(2) = 0xDD;
    fixture.eeprom.i2c_write(write_buffer, 3);

    // Verify data was written correctly across page boundary
    memory = fixture.eeprom.get_memory();
    ensure::is_eq(memory.at(0x07), 0xCC);  // Last byte of first page
    ensure::is_ne(memory.at(0x08), 0xDD);  // First byte of second page should not be written
    ensure::is_ne(memory.at(0x00), 0x01);  // First byte was overwritten due to page wrap
    ensure::is_eq(memory.at(0x00), 0xDD);  // Wrapped data written to address 0
};

TEST_F(at24c01test, test_eeprom_address_wrapping)
{
    // Test that addresses wrap around correctly
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_buffer = {0x7F, 0xAA};  // Address
                                                                                    // 127 (last
                                                                                    // address)
    fixture.eeprom.i2c_write(write_buffer, 2);

    // Write to address 128 should wrap to address 0
    write_buffer.at(0) = 0x80;  // Address 128 (wraps to 0)
    write_buffer.at(1) = 0xBB;
    fixture.eeprom.i2c_write(write_buffer, 2);

    auto memory = fixture.eeprom.get_memory();
    ensure::is_eq(memory.at(127), 0xAA);  // Data at address 127
    ensure::is_eq(memory.at(0), 0xBB);    // Data at address 0 (wrapped)
};

TEST_F(at24c01test, test_eeprom_read_after_write)
{
    // Test reading data after writing it
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_buffer = {0x20, 0x11, 0x22, 0x33};
    fixture.eeprom.i2c_write(write_buffer, 4);

    // Read the data back
    uint8_t data = 0x00;
    fixture.eeprom.i2c_read(data, true);  // Start new transaction at address 0
    ensure::is_eq(data, 0xff);            // Should read 0xFF from address 0

    // Read from address 0x20 where we wrote data
    fixture.eeprom.i2c_write(write_buffer, 1);  // Set address to 0x20
    fixture.eeprom.i2c_read(data, true);        // Start new transaction
    ensure::is_eq(data, 0x11);                  // Should read first data byte

    fixture.eeprom.i2c_read(data, false);
    ensure::is_eq(data, 0x22);  // Should read second data byte

    fixture.eeprom.i2c_read(data, false);
    ensure::is_eq(data, 0x33);  // Should read third data byte
};

TEST_F(at24c01test, test_eeprom_memory_manipulation)
{
    // Test the set_memory and get_memory methods
    std::array<uint8_t, 128> test_memory;
    for (int i = 0; i < 128; i++) {
        test_memory.at(i) = static_cast<uint8_t>(i);
    }

    fixture.eeprom.set_memory(test_memory);

    auto memory = fixture.eeprom.get_memory();
    for (int i = 0; i < 128; i++) {
        ensure::is_eq(memory.at(i), static_cast<uint8_t>(i));
    }

    // Test that changes persist
    memory.at(0) = 0xFF;
    ensure::is_eq(memory.at(0), 0xFF);

    // Test that the original memory is unchanged
    auto original_memory = fixture.eeprom.get_memory();
    ensure::is_eq(original_memory.at(0), 0x00);  // Should still be the original value
};

TEST_F(at24c01test, test_eeprom_edge_cases)
{
    // Test edge cases
    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize> write_buffer = {0x00, 0x01};
    write_buffer.fill(0x01);
    write_buffer.at(0) = 0x00;

    // Test writing 0 bytes (should do nothing)
    fixture.eeprom.i2c_write(write_buffer, 0);
    auto memory = fixture.eeprom.get_memory();
    ensure::is_eq(memory.at(0), 0xff);  // Should still be 0xFF

    // Test writing only address (should set address but not write data)
    fixture.eeprom.i2c_write(write_buffer, 1);
    memory = fixture.eeprom.get_memory();
    ensure::is_eq(memory.at(0), 0xff);  // Should still be 0xFF

    // Test writing more bytes than buffer size (should be limited)
    fixture.eeprom.i2c_write(write_buffer, 40);  // More than buffer size
    memory = fixture.eeprom.get_memory();
    ensure::is_eq(memory.at(0), 0x01);  // Should have written the data
};
