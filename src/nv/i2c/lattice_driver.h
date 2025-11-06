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
#include <span>
#include <stdint.h>

#include "nv/i2c/common.h"
#include "nv/i2c/port.h"

namespace nv::i2c {

constexpr uint8_t CPLD_I2C_ADDR = 0x40;

constexpr size_t LATTICE_CPLD_PAGE_SIZE = 16;

// Lattice CPLD Commands (CMD2) - Reference: Lattice CPLD Programming Specification
namespace LatticeCmd {
// Device Identification
constexpr uint8_t READ_DEVICE_ID = 0xE0;  // LSC_READ_DEVICEID
constexpr uint8_t READ_USERCODE  = 0xC0;  // LSC_READ_USERCODE

// Configuration Interface Control
constexpr uint8_t ENABLE_CONFIG_INTERFACE = 0x74;   // Enable Configuration Interface
                                                    // (Transparent Mode)
constexpr uint8_t DISABLE_CONFIG_INTERFACE = 0x26;  // Disable Configuration Interface
constexpr uint8_t BYPASS                   = 0xFF;  // Bypass command

// Erase Operations
constexpr uint8_t ERASE_FLASH = 0x0E;  // LSC_ERASE_TAG (erase flash with config)
constexpr uint8_t ERASE_UFM   = 0xCB;  // LSC_ERASE_TAG (erase UFM sector only)

// Address Operations
constexpr uint8_t RESET_CONFIG_ADDRESS = 0x46;  // LSC_INIT_ADDRESS (reset flash address)
constexpr uint8_t INIT_UFM_ADDRESS     = 0x47;  // LSC_INIT_ADDR_UFM (reset UFM address)
constexpr uint8_t SET_ADDRESS = 0xB4;  // LSC_BITSTREAM_BURST (set address for read/write)

// Programming Operations
constexpr uint8_t PROGRAM_PAGE = 0x70;  // LSC_PROG_INCR_NV (program configuration flash page)
constexpr uint8_t PROGRAM_DONE = 0x5E;  // LSC_PROGRAM_DONE (complete programming)
constexpr uint8_t PROGRAM_UFM_PAGE = 0xC9;  // LSC_PROG_TAG (program UFM page)

// Read Operations
constexpr uint8_t READ_CONFIG_PAGE = 0x73;  // LSC_READ_INCR_NV (read configuration flash page)
constexpr uint8_t READ_UFM_PAGE    = 0xCA;  // LSC_READ_TAG (read UFM page)

// Status Operations
constexpr uint8_t CHECK_BUSY  = 0xF0;  // LSC_CHECK_BUSY (check busy flag)
constexpr uint8_t READ_STATUS = 0x3C;  // LSC_READ_STATUS (read status register)
constexpr uint8_t REFRESH     = 0x79;  // LSC_REFRESH (refresh device)
}  // namespace LatticeCmd

// Lattice CPLD Command Operands
namespace LatticeOperand {
// Enable Configuration Interface operands
constexpr uint8_t ENABLE_CONFIG_OP1 = 0x08;
constexpr uint8_t ENABLE_CONFIG_OP2 = 0x00;
constexpr uint8_t ENABLE_CONFIG_OP3 = 0x00;

// Disable Configuration Interface operands
constexpr uint8_t DISABLE_CONFIG_OP1 = 0x00;
constexpr uint8_t DISABLE_CONFIG_OP2 = 0x00;

// Erase Flash operands
constexpr uint8_t ERASE_FLASH_OP1 = 0x0C;
constexpr uint8_t ERASE_FLASH_OP2 = 0x00;
constexpr uint8_t ERASE_FLASH_OP3 = 0x00;

// Common operands for commands
constexpr uint8_t OP_ZERO = 0x00;

// Program/Read page operands
constexpr uint8_t PAGE_OP1 = 0x00;
constexpr uint8_t PAGE_OP2 = 0x00;
constexpr uint8_t PAGE_OP3 = 0x01;

// Read UFM page operands
constexpr uint8_t READ_UFM_OP1 = 0x10;
constexpr uint8_t READ_UFM_OP2 = 0x00;
constexpr uint8_t READ_UFM_OP3 = 0x01;

// UFM address prefix
constexpr uint8_t UFM_ADDR_PREFIX1 = 0x40;
constexpr uint8_t UFM_ADDR_PREFIX2 = 0x00;

// Bypass operand
constexpr uint8_t BYPASS_PATTERN = 0xFF;
}  // namespace LatticeOperand

// Status Register Flags
namespace LatticeStatus {
// Configuration Flash Status Flags (from 0xF0 CHECK_BUSY and 0x3C READ_STATUS)
constexpr uint8_t BUSY_FLAG = 0x80;  // Bit 7: Busy flag for configuration operations
constexpr uint8_t FAIL_FLAG = 0x20;  // Bit 5: Fail flag for configuration operations

// UFM Status Flags (from 0x3C READ_STATUS for UFM operations)
constexpr uint8_t UFM_BUSY_FLAG = 0x10;  // Bit 4 (bit 12 in 32-bit register): UFM busy flag
constexpr uint8_t UFM_FAIL_FLAG = 0x20;  // Bit 5 (bit 13 in 32-bit register): UFM fail flag
}  // namespace LatticeStatus

// Timing and Retry Configuration
namespace LatticeTiming {
// Delay values in milliseconds
constexpr unsigned int FAST_DELAY_MS    = 10;
constexpr unsigned int SLOW_DELAY_MS    = 1000;
constexpr unsigned int UFM_DELAY_MS     = 200;
constexpr unsigned int POLL_DELAY_MS    = 1;
constexpr unsigned int REFRESH_DELAY_MS = 5;

// Retry counts
constexpr uint8_t DEFAULT_RETRY_COUNT  = 10;
constexpr uint8_t UFM_FAST_RETRY_COUNT = 20;
constexpr uint8_t UFM_SLOW_RETRY_COUNT = 10;
}  // namespace LatticeTiming

// Address and Data Masks
namespace LatticeMask {
constexpr uint8_t ADDR_LOW_MASK  = 0xFF;
constexpr uint8_t ADDR_HIGH_MASK = 0x7F;
constexpr uint8_t BYTE_MASK      = 0xFF;
}  // namespace LatticeMask

// Buffer Sizes
namespace LatticeBuffer {
constexpr size_t STATUS_BUFFER_SIZE = 4;
constexpr size_t ID_BUFFER_SIZE     = 5;
constexpr size_t COMMAND_SIZE       = 4;
}  // namespace LatticeBuffer

class LatticeCpld
{
public:
    static bool is_enabled();
    LatticeCpld(Port port, uint8_t address) noexcept;
    I2cStatus read_id();
    I2cStatus enter_transparent_mode();
    I2cStatus exit_transparent_mode();
    I2cStatus erase();
    I2cStatus refresh();
    I2cStatus send_chunk(uint8_t (&img_chunk)[LATTICE_CPLD_PAGE_SIZE], size_t chunk_len);
    I2cStatus end_flash();
    I2cStatus read_usercode();
    I2cStatus begin_read(uint32_t addr);
    I2cStatus read_chunk(uint8_t (&buf)[LATTICE_CPLD_PAGE_SIZE]);

    I2cStatus write_offset(const uint8_t* buf, uint32_t addr, uint32_t len);
    I2cStatus read_offset(uint8_t* buf, uint32_t addr, uint32_t len);
    I2cStatus update_complete();

    // UFM (User Flash Memory) operations
    I2cStatus erase_ufm();
    I2cStatus init_ufm_address();
    I2cStatus write_ufm_page(uint8_t (&page_data)[LATTICE_CPLD_PAGE_SIZE]);
    I2cStatus
    write_ufm(const uint8_t* buffer, uint32_t size, uint32_t offset, bool is_erase = true);
    I2cStatus read_ufm_page(uint8_t (&page_data)[LATTICE_CPLD_PAGE_SIZE], uint32_t page_offset);
    I2cStatus read_ufm(uint8_t* buffer, uint32_t size, uint32_t offset);
    static LatticeCpld& inst();
    I2cStatus           set_address(uint32_t addr, bool is_UFM);

private:
    Port    _port;
    uint8_t _address;

    I2cStatus cpld_write(std::span<uint8_t> buffer);
    I2cStatus cpld_write_read(std::span<uint8_t> write_buffer, std::span<uint8_t> read_buffer);
    I2cStatus cpld_write_retry(std::span<uint8_t> buffer);
    I2cStatus cpld_write_wait(std::span<uint8_t> buffer, bool high_speed);
    I2cStatus cpld_write_wait_ufm(std::span<uint8_t> buffer, bool high_speed);
};

}  // namespace nv::i2c
