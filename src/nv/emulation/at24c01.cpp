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

#include "nv/emulation/at24c01.h"

namespace nv::emulation {

/**
 * @brief Default constructor for AT24C01 emulator
 *
 * Initializes the emulated EEPROM memory to the erased state (0xFF)
 * and sets the current address pointer to 0. This mimics the power-on
 * state of a real AT24C01 EEPROM where all memory locations contain
 * the erased value and the internal address pointer starts at the
 * beginning of memory.
 *
 * The constructor performs the following initialization:
 * - Fills the entire memory array with ERASED_MEMORY (0xFF)
 * - Sets the current address pointer to 0
 * - Prepares the emulator for I2C operations
 */
At24c01::At24c01() : _memory(), _current_address(0)
{
    // Check that the memory size and page size are correct
    static_assert(MEMORY_SIZE == 128, "MEMORY_SIZE must be 128");
    static_assert(PAGE_SIZE == 8, "PAGE_SIZE must be 8");
    static_assert(sys::i2c::I2cSlaveBufferSize >= 32, "I2C buffer size must be >= 32");

    // Initialize memory to 0xFF (unprogrammed state)
    _memory.fill(ERASED_MEMORY);
}

/**
 * @brief Handle I2C read operation
 *
 * Reads a byte from the current memory address and increments the internal
 * address pointer. The address pointer wraps around to 0 when it reaches
 * the end of memory, allowing for continuous sequential reads across the
 * entire memory space.
 *
 * This function implements the standard EEPROM read behavior where:
 * - Data is read from the current address pointer location
 * - The address pointer is automatically incremented
 * - Address wrapping occurs at memory boundaries
 * - Sequential reads can continue indefinitely
 *
 * @param return_data Reference to store the read data byte
 * @param new_transaction Indicates if this is a new I2C transaction (unused in current
 * implementation)
 */
void At24c01::i2c_read(uint8_t& return_data, [[maybe_unused]] bool new_transaction)
{
    // Return data from current address and increment
    return_data      = _memory.at(_current_address);
    _current_address = (_current_address + 1) % MEMORY_SIZE;
}

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
 * automatically handling page boundary crossings. The function implements
 * the following behavior:
 *
 * 1. First byte sets the starting address
 * 2. Subsequent bytes are written sequentially
 * 3. Page boundaries are handled automatically
 * 4. Address pointer is updated to next location
 * 5. Input validation prevents buffer overruns
 *
 * @param i2c_data Array containing the write data (first byte is address)
 * @param data_length Number of bytes to write (including address byte)
 */
void At24c01::i2c_write(const std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& i2c_data,
                        uint8_t                                                  data_length)
{
    // Validate input parameters
    if (data_length == 0) {
        return;  // No data to write
    }
    if (data_length > i2c_data.size()) {
        data_length = i2c_data.size();  // Limit to available buffer size
    }

    // First byte of write transaction is the address
    _current_address = i2c_data[0] & (MEMORY_SIZE - 1);  // Mask to ensure address is within
                                                         // valid range

    // If only address was written, return (no data to write)
    if (data_length == 1) {
        return;
    }

    // Calculate page information for proper page boundary handling
    const size_t page_address = _current_address / PAGE_SIZE;  // Which page we're writing to
    size_t       page_offset  = _current_address % PAGE_SIZE;  // Offset within the page

    // Start writing from the second byte (first byte was address)
    for (uint8_t i = 1; i < data_length; i++) {
        // Write data to memory, handling page boundaries automatically
        _memory.at(page_address * PAGE_SIZE + page_offset) = i2c_data.at(i);

        // Update page offset, wrapping to the beginning of the page as needed
        page_offset = (page_offset + 1) % PAGE_SIZE;
    }

    // Update current address pointer to next location after last written byte
    // This will be within the current page, as page_address was not changed,
    // and page_offset stays within the valie PAGE_SIZE range.
    _current_address = page_address * PAGE_SIZE + page_offset;
}

}  // namespace nv::emulation
