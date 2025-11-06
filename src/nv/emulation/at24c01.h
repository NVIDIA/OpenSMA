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
#include <cstdint>
#include "sys/i2c/i2c_slave.h"

namespace nv::emulation {

/**
 * @brief AT24C01 EEPROM Emulator
 *
 * This class emulates the behavior of an AT24C01 128x8 (1Kbit) EEPROM device.
 * It handles I2C read and write operations to the emulated memory, providing
 * a software implementation that mimics the hardware behavior of the actual
 * EEPROM chip.
 *
 * The emulator supports:
 * - Random access reads from any memory location
 * - Sequential reads with automatic address increment
 * - Page-based writes (8-byte pages)
 * - Memory initialization to erased state (0xFF)
 *
 * The parent task is responsible for:
 * - Handling I2C addressing
 * - Managing the I2C bus state
 * - Routing I2C operations to this emulator
 */

class At24c01
{
public:
    // Internal Constants
    // ---------------------------------------------

    /**
     * @brief Total memory size in bytes
     *
     * The AT24C01 provides 128 bytes of memory, equivalent to 1Kbit of storage.
     * This constant defines the total addressable memory space.
     */
    static constexpr size_t MEMORY_SIZE = 128;  // 128 bytes (1Kbit)

    /**
     * @brief Default constructor for AT24C01 emulator
     *
     * Initializes the emulated EEPROM memory to the erased state (0xFF)
     * and sets the current address pointer to 0. This mimics the power-on
     * state of a real AT24C01 EEPROM.
     */
    At24c01();

    /**
     * @brief Default destructor for AT24C01 emulator
     *
     * Provides proper cleanup of the emulator instance.
     */
    ~At24c01() = default;

    /**
     * @brief Handle I2C read operation
     *
     * Reads a byte from the current memory address and increments the internal
     * address pointer. The address pointer wraps around to 0 when it reaches
     * the end of memory, allowing for continuous sequential reads.
     *
     * @param return_data Reference to store the read data byte
     * @param new_transaction Indicates if this is a new I2C transaction (unused in current
     * implementation)
     */
    void i2c_read(uint8_t& return_data, bool new_transaction);

    /**
     * @brief Handle I2C write operation
     *
     * Writes data to the emulated EEPROM memory. The first byte of the write
     * transaction is interpreted as the starting address. Subsequent bytes are
     * written to consecutive memory locations, with automatic page boundary
     * handling. The internal address pointer is updated to point to the next
     * location after the last written byte.
     *
     * Write operations respect the 8-byte page size limitation of the AT24C01,
     * automatically handling page boundary crossings.
     *
     * @param i2c_data Array containing the write data (first byte is address)
     * @param data_length Number of bytes to write (including address byte)
     */
    void i2c_write(const std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& i2c_data,
                   uint8_t data_length);

    /**
     * @brief Get the current memory contents
     *
     * Returns a const reference to the internal memory array, allowing
     * external code to inspect the current state of the emulated EEPROM.
     * This is useful for debugging, testing, and verification purposes.
     *
     * @return Const reference to the 128-byte memory array
     */
    const std::array<uint8_t, MEMORY_SIZE>& get_memory() const { return _memory; }

    /**
     * @brief Set the memory contents
     *
     * Allows external code to set the entire memory contents, typically
     * for initialization, testing, or restoring a saved state. This
     * operation replaces the entire memory array with new data.
     *
     * @param new_memory New memory contents to set (must be 128 bytes)
     */
    void set_memory(const std::array<uint8_t, MEMORY_SIZE>& new_memory)
    {
        _memory = new_memory;
    }

protected:
    /**
     * @brief Emulated EEPROM memory array
     *
     * Internal storage that represents the 128 bytes of EEPROM memory.
     * This array is protected to allow derived classes to access the
     * memory while maintaining encapsulation from external code.
     */
    std::array<uint8_t, MEMORY_SIZE> _memory;  // Emulated EEPROM memory

private:
    // Internal Constants
    // ---------------------------------------------

    /**
     * @brief Page size for write operations
     *
     * The AT24C01 uses 8-byte pages for write operations. This constant
     * defines the page size used in the emulation for proper hardware
     * behavior simulation.
     */
    static constexpr size_t PAGE_SIZE = 8;  // Page size for writes

    /**
     * @brief Erased memory value
     *
     * Represents the value of unprogrammed/erased memory cells in the
     * EEPROM. This is the default state of all memory locations after
     * power-on or after an erase operation.
     */
    static constexpr uint8_t ERASED_MEMORY = 0xFF;  // Erased memory value

    // Member Variables
    // ---------------------------------------------

    /**
     * @brief Current read/write address pointer
     *
     * Internal address pointer that tracks the current memory location
     * for read and write operations. This pointer is automatically
     * incremented during sequential operations and wraps around to 0
     * when it reaches the end of memory.
     */
    uint8_t _current_address;  // Current read/write address
};

}  // namespace nv::emulation
