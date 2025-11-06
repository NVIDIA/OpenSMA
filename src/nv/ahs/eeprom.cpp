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

#include "nv/ahs/eeprom.h"
#include "nv/nv.h"

namespace nv::ahs {

/**
 * @brief Constructs an EEPROM instance with AHS configuration
 *
 * Initializes the EEPROM memory with default AHS configuration values.
 * Sets up the EEPROM format version, vendor ID, channel bus size, partition count,
 * mapping version type, and IO expander configurations. This constructor ensures
 * the EEPROM contains valid AHS configuration data for proper system operation.
 *
 * @param io_expander_address I2C address for the IO expander
 */
EEPROM::EEPROM(const uint8_t io_expander_address) : nv::emulation::At24c01()
{
    // Initialize the EEPROM for AHS operation with default configuration values

    // Set EEPROM format version to 0
    _memory.at(EEPROM_FORMAT_VERSION) = EEPROM_FORMAT_0;

    // Initialize vendor ID bytes to 0x00 (no specific vendor)
    _memory.at(VENDOR_ID_0) = 0x00;
    _memory.at(VENDOR_ID_1) = 0x00;
    _memory.at(VENDOR_ID_2) = 0x00;

    // Configure channel bus size for x4 lanes (typical for e.1s drives)
    _memory.at(CHANNEL_BUS_SIZE) = CHANNEL_BUS_SIZE_X4;

    // Set number of partitions to 1 (single partition configuration)
    _memory.at(NUM_OF_PARTITIONS) = NUM_OF_PARTITIONS_1;

    // Use surprise with power mapping version type for AHS operation
    _memory.at(MAPPING_VERSION_TYPE) = MAPPING_VERSION_TYPE_SURPRISE_WITH_POWER;

    // Disable IO expander interrupts by default
    _memory.at(INTERRUPT_INFO_IO_EXPANDER) = INTERRUPT_INFO_IO_EXPANDER_DISABLED;

    // Set IO expander I2C address to disabled (not used in this configuration)
    _memory.at(INTERRUPT_INFO_IO_EXPANDER_I2C_ADDRESS) = INTERRUPT_INFO_IO_EXPANDER_DISABLED;

    // Configure IO expander 0 for two SSD support
    _memory.at(IO_EXPANDER_0_CONFIGURATION) = IO_EXPANDER_TWO_SSD;

    // Set IO expander 0 I2C address to the provided address
    _memory.at(IO_EXPANDER_0_ADDRESS) = io_expander_address;

    // Disable all other IO expanders (not used in AHS configuration)
    _memory.at(IO_EXPANDER_1_CONFIGURATION) = IO_EXPANDER_DISABLED;
    _memory.at(IO_EXPANDER_2_CONFIGURATION) = IO_EXPANDER_DISABLED;
    _memory.at(IO_EXPANDER_3_CONFIGURATION) = IO_EXPANDER_DISABLED;
    _memory.at(IO_EXPANDER_4_CONFIGURATION) = IO_EXPANDER_DISABLED;
    _memory.at(IO_EXPANDER_5_CONFIGURATION) = IO_EXPANDER_DISABLED;
    _memory.at(IO_EXPANDER_6_CONFIGURATION) = IO_EXPANDER_DISABLED;
    _memory.at(IO_EXPANDER_7_CONFIGURATION) = IO_EXPANDER_DISABLED;
}

/**
 * @brief Constructs an EEPROM instance with AHS configuration
 *
 * Initializes the EEPROM memory with default AHS configuration values.
 * Sets up the EEPROM format version, vendor ID, channel bus size, partition count,
 * mapping version type, and IO expander configurations. This constructor ensures
 * the EEPROM contains valid AHS configuration data for proper system operation.
 *
 * @param io_expander_address I2C address for the IO expander
 * @param num_drives Number of drives
 */
EEPROM::EEPROM(const uint8_t io_expander_address, const uint8_t num_drives)
: EEPROM(io_expander_address)
{
    // Initialize the EEPROM for AHS operation with default configuration values
    switch (num_drives) {
        case IO_EXPANDER_ONE_SSD:
        case IO_EXPANDER_TWO_SSD:
        case IO_EXPANDER_DISABLED: _memory.at(IO_EXPANDER_0_CONFIGURATION) = num_drives; break;
        default:
            nv::error("Invalid number of drives: %d\n", num_drives);
            _memory.at(IO_EXPANDER_0_CONFIGURATION) = IO_EXPANDER_DISABLED;
            break;
    }
}
}  // namespace nv::ahs
