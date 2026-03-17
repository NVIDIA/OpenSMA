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

#include "nv/i2c/port.h"
#include <stdint.h>
#include <stddef.h>

namespace nv::fru {

/**
 * FRU API - Simple interface to extract chassis info from FRU EEPROM
 *
 * Flow: FRU → EEPROM → I2C → Parser → API
 */

/**
 * Status codes for FRU operations
 */
enum class Status
{
    FruSuccess,           // FRU data successfully read and parsed
    FruI2cBusy,           // I2C bus is busy
    FruI2cNack,           // I2C device not responding (NACK)
    FruI2cTimeout,        // I2C communication timeout
    FruI2cError,          // General I2C communication error
    FruParseError,        // FRU data parsing/validation failed
    FruPartNumberError,   // Part Number TLV parsing failed
    FruSerialError,       // Serial Number TLV parsing failed
    FruCustomFieldError,  // Custom Field TLV parsing failed
    FruChecksumError,     // FRU checksum validation failed
    FruNoChassisArea,     // No Chassis Info Area found in FRU
    FruInvalidData,       // Invalid or corrupted FRU data
};

/**
 * Fixed-size output container to avoid dynamic allocation in API surface.
 *
 * Notes:
 * - Strings are provided as null-terminated byte arrays (uint8_t).
 * - Unused bytes are zero-filled; empty string is "\0".
 * - For current stage we assume at most one Custom Chassis Info field.
 */
struct ChassisInfo
{
    static constexpr size_t MaxSerialLen      = 64;  // includes trailing '\0'
    static constexpr size_t MaxCustomFields   = 1;   // current assumption
    static constexpr size_t MaxCustomFieldLen = 64;  // includes trailing '\0'

    uint8_t chassis_type;  // Chassis Type (IPMI enum: 1=Other, 2=Unknown, 3=Desktop, etc.)
    uint8_t serial[MaxSerialLen];  // Chassis Serial Number (ASCII bytes, NUL-terminated)
    uint8_t num_custom_fields;     // 0..MaxCustomFields
    uint8_t custom_fields[MaxCustomFields][MaxCustomFieldLen];  // Custom fields (ASCII bytes,
                                                                // NUL-terminated)
};

struct BoardInfo
{
    static constexpr size_t MaxBoardNameLen = 64;
    uint8_t                 serial[MaxBoardNameLen];
};

/**
 * @brief Get chassis info (serial + custom fields) in one call.
 *        API uses fixed-size buffers, no dynamic allocation.
 *
 * @param port I2C port
 * @param eeprom_addr EEPROM I2C address (typically 0x50-0x57)
 * @param out [OUT] Populated chassis info structure
 * @return Status::FruSuccess on success, other Status values on failure
 */
Status get_chassis_info(nv::i2c::Port port, uint8_t eeprom_addr, ChassisInfo& out);

/**
 * @brief Get board info (serial number) in one call.
 *        API uses fixed-size buffers, no dynamic allocation.
 *
 * @param port I2C port
 * @param eeprom_addr EEPROM I2C address (typically 0x50-0x57)
 * @param out [OUT] Populated board info structure
 * @return Status::FruSuccess on success, other Status values on failure
 */
Status get_board_info(nv::i2c::Port port, uint8_t eeprom_addr, BoardInfo& out);
}  // namespace nv::fru
