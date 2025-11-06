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

#include "nv/ut/unittest.h"
#include "nv/ahs/eeprom.h"

using namespace nv;
using namespace ut;

/**
 * @brief Unit test fixture for testing AHS EEPROM functionality
 * 
 * This test class provides test cases for verifying the AHS EEPROM
 * implementation, including constructor behavior, I2C read/write operations,
 * and memory initialization with AHS configuration values.
 */
class eepromtest : public ut::Fixture
{
public:
    /**
     * @brief Sets up test fixtures before each test case
     *
     * Initializes three EEPROM instances for testing:
     * - Default constructor EEPROM
     * - EEPROM with IO expander address 0x50
     * - EEPROM with IO expander address 0x52 and single SSD configuration
     */
    void setup() override
    {
        //eeprom = ahs::EEPROM();
        ahs_eeprom = ahs::EEPROM(0x50U);
        ahs_eeprom2 = ahs::EEPROM(0x52U, ahs::IO_EXPANDER_ONE_SSD);
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
    ahs::EEPROM eeprom;
    /** @brief EEPROM instance with IO expander address 0x50 for testing */
    ahs::EEPROM ahs_eeprom;
    /** @brief EEPROM instance with IO expander address 0x52 and single SSD config */
    ahs::EEPROM ahs_eeprom2;
};

/**
 * @brief Test case for EEPROM constructor functionality
 *
 * Verifies that EEPROM constructors properly initialize memory with expected
 * AHS configuration values. Tests both default constructor and parameterized
 * constructors to ensure correct memory layout and configuration data.
 */
TEST_F(eepromtest, test_eeprom_constructor)
{
    auto memory = fixture.eeprom.get_memory();
    ensure::is_eq(memory.size(), 128);
    for (int i = 0; i < 128; i++) {
        ensure::is_eq(memory.at(i), 0xff);
    }

    auto ahs_memory = fixture.ahs_eeprom.get_memory();
    ensure::is_eq(ahs_memory.size(), 128);
    ensure::is_eq(ahs_memory.at(ahs::EEPROM_FORMAT_VERSION), ahs::EEPROM_FORMAT_0);
    ensure::is_eq(ahs_memory.at(ahs::VENDOR_ID_0), 0x00);
    ensure::is_eq(ahs_memory.at(ahs::VENDOR_ID_1), 0x00);
    ensure::is_eq(ahs_memory.at(ahs::VENDOR_ID_2), 0x00);
    ensure::is_eq(ahs_memory.at(ahs::CHANNEL_BUS_SIZE), ahs::CHANNEL_BUS_SIZE_X4);
    ensure::is_eq(ahs_memory.at(ahs::NUM_OF_PARTITIONS), ahs::NUM_OF_PARTITIONS_1);
    ensure::is_eq(ahs_memory.at(ahs::MAPPING_VERSION_TYPE), ahs::MAPPING_VERSION_TYPE_SURPRISE_WITH_POWER);
    ensure::is_eq(ahs_memory.at(ahs::INTERRUPT_INFO_IO_EXPANDER), ahs::INTERRUPT_INFO_IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory.at(ahs::INTERRUPT_INFO_IO_EXPANDER_I2C_ADDRESS), ahs::INTERRUPT_INFO_IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_0_CONFIGURATION), ahs::IO_EXPANDER_TWO_SSD);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_0_ADDRESS), 0x50);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_1_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_2_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_3_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_4_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_5_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_6_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory.at(ahs::IO_EXPANDER_7_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);

    auto ahs_memory2 = fixture.ahs_eeprom2.get_memory();
    ensure::is_eq(ahs_memory2.size(), 128);
    ensure::is_eq(ahs_memory2.at(ahs::EEPROM_FORMAT_VERSION), ahs::EEPROM_FORMAT_0);
    ensure::is_eq(ahs_memory2.at(ahs::VENDOR_ID_0), 0x00);
    ensure::is_eq(ahs_memory2.at(ahs::VENDOR_ID_1), 0x00);
    ensure::is_eq(ahs_memory2.at(ahs::VENDOR_ID_2), 0x00);
    ensure::is_eq(ahs_memory2.at(ahs::CHANNEL_BUS_SIZE), ahs::CHANNEL_BUS_SIZE_X4);
    ensure::is_eq(ahs_memory2.at(ahs::NUM_OF_PARTITIONS), ahs::NUM_OF_PARTITIONS_1);
    ensure::is_eq(ahs_memory2.at(ahs::MAPPING_VERSION_TYPE), ahs::MAPPING_VERSION_TYPE_SURPRISE_WITH_POWER);
    ensure::is_eq(ahs_memory2.at(ahs::INTERRUPT_INFO_IO_EXPANDER), ahs::INTERRUPT_INFO_IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory2.at(ahs::INTERRUPT_INFO_IO_EXPANDER_I2C_ADDRESS), ahs::INTERRUPT_INFO_IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_0_CONFIGURATION), ahs::IO_EXPANDER_ONE_SSD);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_0_ADDRESS), 0x52);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_1_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_2_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_3_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_4_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_5_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_6_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
    ensure::is_eq(ahs_memory2.at(ahs::IO_EXPANDER_7_CONFIGURATION), ahs::IO_EXPANDER_DISABLED);
};
