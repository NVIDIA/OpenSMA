/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include <optional>
#include <span>

#include "nv/common/preproc.h"
#include "nv/gpio/driver.h"
#include "nv/spi/ext_flash_config.h"
#include "sys/spi/spi_edma.h"

namespace nv::spi {

// JEDEC SPI NOR flash commands
namespace ExtFlashCmd {
constexpr uint8_t WREN  = 0x06;  // Write Enable
constexpr uint8_t RDSR  = 0x05;  // Read Status Register
constexpr uint8_t READ  = 0x03;  // Read Data (3-byte address)
constexpr uint8_t PP    = 0x02;  // Page Program (3-byte address)
constexpr uint8_t SE    = 0x20;  // Sector Erase 4KB (3-byte address)
constexpr uint8_t BE    = 0xD8;  // Block Erase 64KB (3-byte address)
constexpr uint8_t READ4 = 0x13;  // Read Data (4-byte address)
constexpr uint8_t PP4   = 0x12;  // Page Program (4-byte address)
constexpr uint8_t SE4   = 0x21;  // Sector Erase 4KB (4-byte address)
constexpr uint8_t BE4   = 0xDC;  // Block Erase 64KB (4-byte address)
constexpr uint8_t CE    = 0xC7;  // Chip Erase (whole device)
constexpr uint8_t RDID  = 0x9F;  // Read JEDEC ID
}  // namespace ExtFlashCmd

namespace ExtFlashAddr {
constexpr uint32_t ThreeByteMax = 0xFFFFFFu;
constexpr uint8_t  ThreeByteLen = 3;
constexpr uint8_t  FourByteLen  = 4;
constexpr uint8_t  MaxLen       = FourByteLen;
}  // namespace ExtFlashAddr

// Status register bits (universal for JEDEC SPI NOR)
namespace ExtFlashStatusBit {
constexpr uint8_t WIP = 0x01;  // Write In Progress (bit 0)
}  // namespace ExtFlashStatusBit

class ExtFlash
{
public:
    enum class Status : uint8_t
    {
        Ok,
        Error,
        Timeout,
        InvalidParam,
        Busy,
        IdMismatch,
    };

    ExtFlash(sys::spi::EdmaDriver&           driver,
             nv::gpio::GpioPort              cs_port,
             nv::gpio::GpioPin               cs_pin,
             std::span<const FlashPartition> partitions,
             FlashSpec                       spec) noexcept;

    static void      make(sys::spi::EdmaDriver&           driver,
                          nv::gpio::GpioPort              cs_port,
                          nv::gpio::GpioPin               cs_pin,
                          std::span<const FlashPartition> partitions = {},
                          FlashSpec                       spec       = {}) noexcept;
    static ExtFlash& inst();

    std::span<const FlashPartition> partitions() const { return _partitions; }

    Status read_id(uint32_t& jedec_id);
    Status read_status(uint8_t& status_reg);
    Status erase_chip();

    Status read(uint8_t partition_idx, uint32_t offset, std::span<uint8_t> buffer);
    Status write(uint8_t partition_idx, uint32_t offset, std::span<const uint8_t> buffer);
    Status erase_partition(uint8_t partition_idx);

    // Reset the lazy-sector-erase tracker. Call to start a fresh write session
    // (e.g. between firmware updates) so the next write unconditionally erases
    // the sector it lands in.
    void clear_last_known_write();

private:
    sys::spi::EdmaDriver&           _driver;
    nv::gpio::GpioPort              _cs_port;
    nv::gpio::GpioPin               _cs_pin;
    std::span<const FlashPartition> _partitions;
    FlashSpec                       _spec;
    uint32_t                        _last_written_addr = UINT32_MAX;

    const FlashPartition* get_partition_at(uint8_t idx) const;

    void cs_assert();
    void cs_deassert();

    Status verify_id();
    Status write_enable();
    Status wait_busy(uint32_t timeout_ms);
    Status page_program(uint32_t address, std::span<const uint8_t> data);
    Status erase_sector(const FlashPartition& partition, uint32_t offset);
    Status erase_block(const FlashPartition& partition, uint32_t offset);

    uint8_t addr_bytes() const;

    uint8_t build_addressed_cmd(std::array<uint8_t, ExtFlashAddr::MaxLen + 1>& cmd_buf,
                                uint8_t                                        opcode_3byte,
                                uint8_t                                        opcode_4byte,
                                uint32_t                                       address) const;
};

namespace ExtFlashInst {
NV_SHARED_BSS inline std::optional<ExtFlash> ext_flash;
}  // namespace ExtFlashInst

}  // namespace nv::spi
