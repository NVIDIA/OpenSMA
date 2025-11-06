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

#include "nv/emulation/at24c01.h"

namespace nv::ahs {

/**
 * @brief AHS EEPROM Configuration Constants and Class
 *
 * This namespace contains constants and the EEPROM class for AHS configuration storage.
 * The EEPROM stores various configuration parameters including vendor ID, channel bus size,
 * partition information, mapping version type, and IO expander configurations.
 */

/**
 * @brief EEPROM format version identifier
 */
constexpr static uint8_t EEPROM_FORMAT_VERSION = 0U;

/**
 * @brief EEPROM format version 0 value
 */
constexpr static uint8_t EEPROM_FORMAT_0 = 0x00U;

/**
 * @brief Vendor ID byte 0 storage location
 */
constexpr static uint8_t VENDOR_ID_0 = 1U;

/**
 * @brief Vendor ID byte 1 storage location
 */
constexpr static uint8_t VENDOR_ID_1 = 2U;

/**
 * @brief Vendor ID byte 2 storage location
 */
constexpr static uint8_t VENDOR_ID_2 = 3U;

/**
 * @brief Channel bus size configuration storage location
 */
constexpr static uint8_t CHANNEL_BUS_SIZE = 4U;

/**
 * @brief Channel bus size configuration for x1 lanes
 */
constexpr static uint8_t CHANNEL_BUS_SIZE_X1 = 0x00U;

/**
 * @brief Channel bus size configuration for x2 lanes
 */
constexpr static uint8_t CHANNEL_BUS_SIZE_X2 = 0x01U;

/**
 * @brief Channel bus size configuration for x4 lanes
 */
constexpr static uint8_t CHANNEL_BUS_SIZE_X4 = 0x02U;

/**
 * @brief Channel bus size configuration for x8 lanes
 */
constexpr static uint8_t CHANNEL_BUS_SIZE_X8 = 0x03U;

/**
 * @brief Channel bus size configuration for x16 lanes
 */
constexpr static uint8_t CHANNEL_BUS_SIZE_X16 = 0x04U;

/**
 * @brief Number of partitions configuration storage location
 */
constexpr static uint8_t NUM_OF_PARTITIONS = 5U;

/**
 * @brief Configuration for 1 partition
 */
constexpr static uint8_t NUM_OF_PARTITIONS_1 = 0x00U;

/**
 * @brief Configuration for 2 partitions
 */
constexpr static uint8_t NUM_OF_PARTITIONS_2 = 0x01U;

/**
 * @brief Configuration for 4 partitions
 */
constexpr static uint8_t NUM_OF_PARTITIONS_4 = 0x02U;

/**
 * @brief Configuration for 8 partitions
 */
constexpr static uint8_t NUM_OF_PARTITIONS_8 = 0x03U;

/**
 * @brief Configuration for 16 partitions
 */
constexpr static uint8_t NUM_OF_PARTITIONS_16 = 0x04U;

/**
 * @brief Mapping version type configuration storage location
 */
constexpr static uint8_t MAPPING_VERSION_TYPE = 6U;

/**
 * @brief Mapping version type: present mirror
 */
constexpr static uint8_t MAPPING_VERSION_TYPE_PRESENT_MIRROR = 0x00U;

/**
 * @brief Mapping version type: minimal
 */
constexpr static uint8_t MAPPING_VERSION_TYPE_MINIMAL = 0x01U;

/**
 * @brief Mapping version type: surprise with power
 */
constexpr static uint8_t MAPPING_VERSION_TYPE_SURPRISE_WITH_POWER = 0x02U;

/**
 * @brief IO expander interrupt information storage location
 */
constexpr static uint8_t INTERRUPT_INFO_IO_EXPANDER = 7U;

/**
 * @brief IO expander interrupts disabled
 */
constexpr static uint8_t INTERRUPT_INFO_IO_EXPANDER_DISABLED = 0x00U;

/**
 * @brief IO expander interrupts enabled
 */
constexpr static uint8_t INTERRUPT_INFO_IO_EXPANDER_ENABLED = 0x01U;

/**
 * @brief IO expander I2C address storage location
 */
constexpr static uint8_t INTERRUPT_INFO_IO_EXPANDER_I2C_ADDRESS = 8U;

/**
 * @brief IO expander disabled configuration
 */
constexpr static uint8_t IO_EXPANDER_DISABLED = 0x00U;

/**
 * @brief IO expander configuration for one SSD
 */
constexpr static uint8_t IO_EXPANDER_ONE_SSD = 0x11U;

/**
 * @brief IO expander configuration for two SSDs
 */
constexpr static uint8_t IO_EXPANDER_TWO_SSD = 0x21U;

/**
 * @brief IO expander 0 configuration storage location
 */
constexpr static uint8_t IO_EXPANDER_0_CONFIGURATION = 9U;

/**
 * @brief IO expander 0 I2C address storage location
 */
constexpr static uint8_t IO_EXPANDER_0_ADDRESS = 10U;

/**
 * @brief IO expander 1 configuration storage location
 */
constexpr static uint8_t IO_EXPANDER_1_CONFIGURATION = 11U;

/**
 * @brief IO expander 1 I2C address storage location
 */
constexpr static uint8_t IO_EXPANDER_1_ADDRESS = 12U;

/**
 * @brief IO expander 2 configuration storage location
 */
constexpr static uint8_t IO_EXPANDER_2_CONFIGURATION = 13U;

/**
 * @brief IO expander 2 I2C address storage location
 */
constexpr static uint8_t IO_EXPANDER_2_ADDRESS = 14U;

/**
 * @brief IO expander 3 configuration storage location
 */
constexpr static uint8_t IO_EXPANDER_3_CONFIGURATION = 15U;

/**
 * @brief IO expander 3 I2C address storage location
 */
constexpr static uint8_t IO_EXPANDER_3_ADDRESS = 16U;

/**
 * @brief IO expander 4 configuration storage location
 */
constexpr static uint8_t IO_EXPANDER_4_CONFIGURATION = 17U;

/**
 * @brief IO expander 4 I2C address storage location
 */
constexpr static uint8_t IO_EXPANDER_4_ADDRESS = 18U;

/**
 * @brief IO expander 5 configuration storage location
 */
constexpr static uint8_t IO_EXPANDER_5_CONFIGURATION = 19U;

/**
 * @brief IO expander 5 I2C address storage location
 */
constexpr static uint8_t IO_EXPANDER_5_ADDRESS = 20U;

/**
 * @brief IO expander 6 configuration storage location
 */
constexpr static uint8_t IO_EXPANDER_6_CONFIGURATION = 21U;

/**
 * @brief IO expander 6 I2C address storage location
 */
constexpr static uint8_t IO_EXPANDER_6_ADDRESS = 22U;

/**
 * @brief IO expander 7 configuration storage location
 */
constexpr static uint8_t IO_EXPANDER_7_CONFIGURATION = 23U;

/**
 * @brief IO expander 7 I2C address storage location
 */
constexpr static uint8_t IO_EXPANDER_7_ADDRESS = 24U;

/**
 * @brief AHS EEPROM class for configuration storage
 *
 * The EEPROM class extends the At24c01 emulation class to provide AHS-specific
 * configuration storage. It initializes the EEPROM memory with default AHS
 * configuration values including format version, vendor ID, channel configuration,
 * and IO expander settings.
 */
class EEPROM : public emulation::At24c01
{
public:
    /**
     * @brief Default constructor for EEPROM instance
     *
     * Creates an uninitialized EEPROM instance by calling the base class
     * default constructor.
     */
    EEPROM() : emulation::At24c01(){};

    /**
     * @brief Constructs an EEPROM instance with AHS configuration
     *
     * Initializes the EEPROM memory with default AHS configuration values.
     * Sets up format version, vendor ID, channel bus size, partition count,
     * mapping version type, and IO expander configurations.
     *
     * @param io_expander_address I2C address for the IO expander
     */
    EEPROM(const uint8_t io_expander_address);

    /**
     * @brief Constructs an EEPROM instance with AHS configuration and number of drives
     *
     * Initializes the EEPROM memory with default AHS configuration values.
     * Sets up format version, vendor ID, channel bus size, partition count,
     * mapping version type, and IO expander configurations.
     *
     * @param io_expander_address I2C address for the IO expander
     * @param num_drives Number of drives
     */
    EEPROM(const uint8_t io_expander_address, const uint8_t num_drives);
};

}  // namespace nv::ahs
