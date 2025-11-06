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

#include <algorithm>
#include <cstring>
#include <span>

#include <FreeRTOS.h>
#include <task.h>

#include "sys/i2c/utils.h"

#include "nv/common/debug.h"
#include "nv/i2c/common.h"
#include "nv/i2c/lattice_driver.h"
#include "nv/i2c/port.h"
#include "nv/nv.h"
#include "nv/ctimer/ctimer.h"
#include NV_IPC_CONFIG_H

using namespace nv::i2c;

// External declaration - cpld must be defined in project's main.cpp
// If not defined and inst() is called, linker will report: "undefined reference to cpld"
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern LatticeCpld cpld;

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
LatticeCpld::LatticeCpld(Port port, uint8_t address) noexcept : _port(port), _address(address)
{}

LatticeCpld& LatticeCpld::inst()
{
    return cpld;
}

bool LatticeCpld::is_enabled()
{
    return CPLD_I2C_PORT != Port::End;
}

I2cStatus LatticeCpld::cpld_write(std::span<uint8_t> buffer)
{
    auto status = sys::i2c::i2c_write(_port, _address, buffer);

    if (status != I2cStatus::Ok) {
        nv::info("cpld_write failed: %d\n", status);
    }

    return status;
}

I2cStatus LatticeCpld::cpld_write_read(std::span<uint8_t> write_buffer,
                                       std::span<uint8_t> read_buffer)
{
    auto status = sys::i2c::i2c_write_read(_port, _address, write_buffer, read_buffer);

    if (status != I2cStatus::Ok) {
        nv::info("cpld_write_read failed: %d\n", status);
    }

    return status;
}

I2cStatus LatticeCpld::cpld_write_retry(std::span<uint8_t> buffer)
{
    I2cStatus     ret         = I2cStatus::Error;
    constexpr int max_retries = 5;
    for (int i = 0; i < max_retries; i++) {
        ret = cpld_write(buffer);

        if (ret == I2cStatus::Ok) {
            return ret;
        }
    }
    nv::info("cpld_write_retry failed: %d\n", ret);
    return ret;
}

I2cStatus LatticeCpld::cpld_write_wait(std::span<uint8_t> buffer, bool high_speed)
{
    const unsigned int delay = (high_speed) ? LatticeTiming::FAST_DELAY_MS
                                            : LatticeTiming::SLOW_DELAY_MS;

    if (auto ret = cpld_write_retry(buffer); ret != I2cStatus::Ok) {
        return ret;
    }

    uint8_t       count = 0;
    const uint8_t retry = LatticeTiming::DEFAULT_RETRY_COUNT;
    // Check busy flag command
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t   busy_cmd[] = {LatticeCmd::CHECK_BUSY,
                            LatticeOperand::OP_ZERO,
                            LatticeOperand::OP_ZERO,
                            LatticeOperand::OP_ZERO};
    I2cStatus ret        = I2cStatus::Error;
    while (count < retry) {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,misc-const-correctness)
        uint8_t buf[] = {0};
        ret           = cpld_write_read(busy_cmd, buf);

        if (!(buf[0] & LatticeStatus::BUSY_FLAG)) {
            break;
        }
        count++;
        vTaskDelay(pdMS_TO_TICKS(delay));
    }

    if (ret != I2cStatus::Ok) {
        nv::error("cpld_write_wait failed: ret != I2cStatus::Ok\n");
        return ret;
    }

    if (count == retry) {
        nv::error("cpld_write_wait failed: count == retry\n");
        return I2cStatus::Error;
    }

    // Read status register command
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t status_cmd[] = {LatticeCmd::READ_STATUS,
                            LatticeOperand::OP_ZERO,
                            LatticeOperand::OP_ZERO,
                            LatticeOperand::OP_ZERO};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,misc-const-correctness)
    uint8_t buf[LatticeBuffer::STATUS_BUFFER_SIZE] = {0};
    if (auto ret = cpld_write_read(status_cmd, buf); ret != I2cStatus::Ok) {
        return ret;
    }

    return (buf[2] & LatticeStatus::FAIL_FLAG) ? I2cStatus::Error : I2cStatus::Ok;
}

I2cStatus LatticeCpld::read_id()
{
    nv::info("read_id\n");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[] = {LatticeCmd::READ_DEVICE_ID,
                     LatticeOperand::OP_ZERO,
                     LatticeOperand::OP_ZERO,
                     LatticeOperand::OP_ZERO};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t buf[LatticeBuffer::ID_BUFFER_SIZE] = {0};

    if (auto ret = cpld_write_read(cmd, buf); ret != I2cStatus::Ok) {
        return ret;
    }

    // Sanity check if id is non-zero
    uint32_t device_id = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    std::memcpy(&device_id, buf, sizeof(uint32_t));

    nv::info("cpld id = 0x%x 0x%x 0x%x 0x%x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);

    return (device_id != 0) ? I2cStatus::Ok : I2cStatus::Error;
}

I2cStatus LatticeCpld::enter_transparent_mode()
{
    nv::info("enter_transparent_mode\n");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[] = {LatticeCmd::ENABLE_CONFIG_INTERFACE,
                     LatticeOperand::ENABLE_CONFIG_OP1,
                     LatticeOperand::ENABLE_CONFIG_OP2};
    return cpld_write_wait(cmd, false);
}

I2cStatus LatticeCpld::exit_transparent_mode()
{
    nv::info("exit_transparent_mode\n");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[] = {LatticeCmd::DISABLE_CONFIG_INTERFACE,
                     LatticeOperand::DISABLE_CONFIG_OP1,
                     LatticeOperand::DISABLE_CONFIG_OP2};
    if (auto ret = cpld_write(cmd); ret != I2cStatus::Ok) {
        return ret;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t bypass[] = {LatticeOperand::BYPASS_PATTERN,
                        LatticeOperand::BYPASS_PATTERN,
                        LatticeOperand::BYPASS_PATTERN,
                        LatticeOperand::BYPASS_PATTERN};
    if (auto ret = cpld_write(bypass); ret != I2cStatus::Ok) {
        return ret;
    }

    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::erase()
{
    nv::info("erase flash\n");
    // Erase flash (with config)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[] = {LatticeCmd::ERASE_FLASH,
                     LatticeOperand::ERASE_FLASH_OP1,
                     LatticeOperand::ERASE_FLASH_OP2,
                     LatticeOperand::ERASE_FLASH_OP3};
    if (auto ret = cpld_write_wait(cmd, false); ret != I2cStatus::Ok) {
        return ret;
    }

    // Reset address
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd_reset[] = {LatticeCmd::RESET_CONFIG_ADDRESS,
                           LatticeOperand::OP_ZERO,
                           LatticeOperand::OP_ZERO,
                           LatticeOperand::OP_ZERO};
    if (auto ret = cpld_write_wait(cmd_reset, false); ret != I2cStatus::Ok) {
        return ret;
    }

    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::refresh()
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[] = {LatticeCmd::REFRESH, LatticeOperand::OP_ZERO, LatticeOperand::OP_ZERO};
    return cpld_write(cmd);
}

I2cStatus LatticeCpld::set_address(uint32_t addr, bool is_UFM)
{
    // Reset cpld page address pointer

    // TODO: what if addr is not page aligned?
    addr /= LATTICE_CPLD_PAGE_SIZE;

    // nv::info("set_address: page offset: 0x%x is_UFM %d\n", addr, is_UFM);

    const uint8_t addr_0 = addr & LatticeMask::BYTE_MASK;
    const uint8_t addr_1 = (addr >> 8) & LatticeMask::BYTE_MASK;

    // nv::info("addr_1 0x%x, addr_0 0x%x\n", addr_1, addr_0);

    // Set address command
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd_set_page[] = {LatticeCmd::SET_ADDRESS,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              addr_1,
                              addr_0};

    if (is_UFM) {
        cmd_set_page[4] = LatticeOperand::UFM_ADDR_PREFIX1;
        cmd_set_page[5] = LatticeOperand::UFM_ADDR_PREFIX2;
    }

    if (auto ret = cpld_write_wait(cmd_set_page, true); ret != I2cStatus::Ok) {
        return ret;
    }
    return I2cStatus::Ok;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
I2cStatus LatticeCpld::send_chunk(uint8_t (&img_chunk)[LATTICE_CPLD_PAGE_SIZE],
                                  size_t /*chunk_len*/)
{
    // Send page (program configuration flash page)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[(LATTICE_CPLD_PAGE_SIZE + 4)] = {LatticeCmd::PROGRAM_PAGE,
                                                 LatticeOperand::PAGE_OP1,
                                                 LatticeOperand::PAGE_OP2,
                                                 LatticeOperand::PAGE_OP3};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    std::memcpy(&cmd[4], img_chunk, LATTICE_CPLD_PAGE_SIZE);
    if (auto ret = cpld_write_wait(cmd, true); ret != I2cStatus::Ok) {
        return ret;
    }
    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::end_flash()
{
    nv::info("end_flash\n");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd_done[] = {LatticeCmd::PROGRAM_DONE,
                          LatticeOperand::OP_ZERO,
                          LatticeOperand::OP_ZERO,
                          LatticeOperand::OP_ZERO};
    if (auto ret = cpld_write_wait(cmd_done, true); ret != I2cStatus::Ok) {
        return ret;
    }

    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::read_usercode()
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[] = {LatticeCmd::READ_USERCODE,
                     LatticeOperand::OP_ZERO,
                     LatticeOperand::OP_ZERO,
                     LatticeOperand::OP_ZERO};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,misc-const-correctness)
    uint8_t buf[LatticeBuffer::ID_BUFFER_SIZE] = {0};
    if (auto ret = cpld_write_read(cmd, buf); ret != I2cStatus::Ok) {
        return ret;
    }

    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::begin_read(uint32_t addr)
{
    // Reset cpld page address pointer

    addr /= LATTICE_CPLD_PAGE_SIZE;

    const uint8_t addr_0 = addr & LatticeMask::BYTE_MASK;
    const uint8_t addr_1 = (addr >> 8) & LatticeMask::ADDR_HIGH_MASK;

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd_set_page[] = {LatticeCmd::SET_ADDRESS,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              addr_1,
                              addr_0};
    if (auto ret = cpld_write_wait(cmd_set_page, true); ret != I2cStatus::Ok) {
        return ret;
    }
    return I2cStatus::Ok;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
I2cStatus LatticeCpld::read_chunk(uint8_t (&buf)[LATTICE_CPLD_PAGE_SIZE])
{
    // Read 16 bytes from cpld (read configuration flash page)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd_read_page[] = {LatticeCmd::READ_CONFIG_PAGE,
                               LatticeOperand::PAGE_OP1,
                               LatticeOperand::PAGE_OP2,
                               LatticeOperand::PAGE_OP3};
    if (auto ret = cpld_write_read(cmd_read_page, buf); ret != I2cStatus::Ok) {
        return ret;
    }

    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::write_offset(const uint8_t* buf, uint32_t addr, uint32_t len)
{
    if (len == 0) {
        return I2cStatus::Ok;  // Nothing to write
    }

    // Check alignment: both addr and len must be multiples of LATTICE_CPLD_PAGE_SIZE
    if ((addr % LATTICE_CPLD_PAGE_SIZE) != 0) {
        nv::info("write_offset: addr 0x%x is not aligned to page size %u\n",
                 addr,
                 LATTICE_CPLD_PAGE_SIZE);
        return I2cStatus::Error;
    }
    if ((len % LATTICE_CPLD_PAGE_SIZE) != 0) {
        nv::info("write_offset: len 0x%x is not aligned to page size %u\n",
                 len,
                 LATTICE_CPLD_PAGE_SIZE);
        return I2cStatus::Error;
    }

    const uint32_t loop_size = len / LATTICE_CPLD_PAGE_SIZE;
    const uint32_t cpld_addr = addr / LATTICE_CPLD_PAGE_SIZE;

    nv::info(
        "write_offset: buffer 0x%x addr 0x%x len 0x%x page 0x%x\n", buf, addr, len, cpld_addr);

    set_address(addr, false);

    for (uint32_t i = 0; i < loop_size; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
        uint8_t tmp_buf[LATTICE_CPLD_PAGE_SIZE] = {0};
        std::memcpy(&tmp_buf[0], buf, LATTICE_CPLD_PAGE_SIZE);

        if (auto status = send_chunk(tmp_buf, LATTICE_CPLD_PAGE_SIZE);
            status != I2cStatus::Ok) {
            return status;
        }
        buf += LATTICE_CPLD_PAGE_SIZE;
    }
    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::read_offset(uint8_t* buf, uint32_t addr, uint32_t len)
{
    if (len == 0) {
        return I2cStatus::Ok;  // Nothing to read
    }

    // Check alignment: both addr and len must be multiples of LATTICE_CPLD_PAGE_SIZE
    if ((addr % LATTICE_CPLD_PAGE_SIZE) != 0) {
        nv::info("read_offset: addr 0x%x is not aligned to page size %u\n",
                 addr,
                 LATTICE_CPLD_PAGE_SIZE);
        return I2cStatus::Error;
    }
    if ((len % LATTICE_CPLD_PAGE_SIZE) != 0) {
        nv::info("read_offset: len 0x%x is not aligned to page size %u\n",
                 len,
                 LATTICE_CPLD_PAGE_SIZE);
        return I2cStatus::Error;
    }

    const uint32_t loop_size = len / LATTICE_CPLD_PAGE_SIZE;
    const uint32_t cpld_addr = addr / LATTICE_CPLD_PAGE_SIZE;

    nv::info(
        "read_offset: buffer 0x%x addr 0x%x len 0x%x page 0x%x\n", buf, addr, len, cpld_addr);

    set_address(addr, false);

    for (uint32_t i = 0; i < loop_size; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
        uint8_t tmp_buf[LATTICE_CPLD_PAGE_SIZE] = {0};

        if (auto status = read_chunk(tmp_buf); status != I2cStatus::Ok) {
            return status;
        }
        std::memcpy(buf, &tmp_buf[0], LATTICE_CPLD_PAGE_SIZE);

        buf += LATTICE_CPLD_PAGE_SIZE;
    }

    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::update_complete()
{
    auto status = end_flash();
    if (status != I2cStatus::Ok) {
        return status;
    }

    status = exit_transparent_mode();
    if (status != I2cStatus::Ok) {
        return status;
    }
    // refresh();

    // wait out t_refresh ~5ms (3.8ms per spec but scheduler tick is 5ms)
    vTaskDelay(pdMS_TO_TICKS(LatticeTiming::REFRESH_DELAY_MS));
    return I2cStatus::Ok;
}

// Erase UFM sector only
I2cStatus LatticeCpld::erase_ufm()
{
    nv::info("erase_ufm\n");
    // Erase UFM sector only - LSC_ERASE_TAG
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[] = {LatticeCmd::ERASE_UFM,
                     LatticeOperand::OP_ZERO,
                     LatticeOperand::OP_ZERO,
                     LatticeOperand::OP_ZERO};
    if (auto ret = cpld_write_wait_ufm(cmd, false); ret != I2cStatus::Ok) {
        return ret;
    }

    return I2cStatus::Ok;
}

// Initialize UFM address to 0x0000
I2cStatus LatticeCpld::init_ufm_address()
{
    // Reset UFM Address - LSC_INIT_ADDR_UFM
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[] = {LatticeCmd::INIT_UFM_ADDRESS,
                     LatticeOperand::OP_ZERO,
                     LatticeOperand::OP_ZERO,
                     LatticeOperand::OP_ZERO};
    return cpld_write_wait_ufm(cmd, true);
}

// UFM-specific write and wait function using status register polling
I2cStatus LatticeCpld::cpld_write_wait_ufm(std::span<uint8_t> buffer, bool high_speed)
{
    const unsigned int delay = (high_speed) ? LatticeTiming::FAST_DELAY_MS
                                            : LatticeTiming::UFM_DELAY_MS;

    if (auto ret = cpld_write_retry(buffer); ret != I2cStatus::Ok) {
        return ret;
    }

    uint8_t       count = 0;
    const uint8_t retry = (high_speed) ? LatticeTiming::UFM_FAST_RETRY_COUNT
                                       : LatticeTiming::UFM_SLOW_RETRY_COUNT;
    // Poll Configuration Status Register as per datasheet Table 16.16
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t   status_cmd[] = {LatticeCmd::READ_STATUS,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO};
    I2cStatus ret          = I2cStatus::Error;

    // Status register bit definitions (in buf[2]):
    // bit 12 (B): Busy Flag (1 = busy) - buf[2] bit 4 = 0x10
    // bit 13 (F): Fail Flag (1 = operation failed) - buf[2] bit 5 = 0x20

    while (count < retry) {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,misc-const-correctness)
        uint8_t buf[LatticeBuffer::STATUS_BUFFER_SIZE] = {0};
        ret                                            = cpld_write_read(status_cmd, buf);

        // Check Busy Flag (bit 12) in buf[2]
        if (!(buf[2] & LatticeStatus::UFM_BUSY_FLAG)) {
            // Busy flag cleared, check for Fail flag (bit 13)
            if (buf[2] & LatticeStatus::UFM_FAIL_FLAG) {
                return I2cStatus::Error;
            }
            return I2cStatus::Ok;
        }
        count++;
        vTaskDelay(pdMS_TO_TICKS(delay));
    }

    if (ret != I2cStatus::Ok) {
        return ret;
    }

    if (count == retry) {
        return I2cStatus::Error;
    }

    return I2cStatus::Ok;
}

// Write single UFM page (16 bytes)
// Note: Address automatically increments after each write
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
I2cStatus LatticeCpld::write_ufm_page(uint8_t (&page_data)[LATTICE_CPLD_PAGE_SIZE])
{
    // Write UFM Page Data - LSC_PROG_TAG
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd[(LATTICE_CPLD_PAGE_SIZE + 4)] = {LatticeCmd::PROGRAM_UFM_PAGE,
                                                 LatticeOperand::PAGE_OP1,
                                                 LatticeOperand::PAGE_OP2,
                                                 LatticeOperand::PAGE_OP3};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    std::memcpy(&cmd[4], page_data, LATTICE_CPLD_PAGE_SIZE);
    return cpld_write_wait_ufm(cmd, true);
}

// Write UFM from offset 0 with given buffer and size
I2cStatus
LatticeCpld::write_ufm(const uint8_t* buffer, uint32_t size, uint32_t offset, bool is_erase)
{
    nv::info("write_ufm: buffer 0x%x size 0x%x offset 0x%x\n", buffer, size, offset);

    // Check for potential overflow: if size > (UINT32_MAX - LATTICE_CPLD_PAGE_SIZE + 1)
    if (size == 0) {
        return I2cStatus::Ok;  // Nothing to write
    }

    // Erase UFM if requested
    if (is_erase) {
        if (auto ret = erase_ufm(); ret != I2cStatus::Ok) {
            return ret;
        }
    }

    set_address(offset, true);

    // Calculate number of pages to write
    const uint32_t num_pages = (size + LATTICE_CPLD_PAGE_SIZE - 1) / LATTICE_CPLD_PAGE_SIZE;

    // Write each page
    for (uint32_t i = 0; i < num_pages; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
        uint8_t page_data[LATTICE_CPLD_PAGE_SIZE] = {0};

        // Calculate bytes to copy for this page
        const size_t buffer_offset = i * LATTICE_CPLD_PAGE_SIZE;
        const size_t remaining     = (buffer_offset < size) ? (size - buffer_offset) : 0;
        const size_t bytes_to_copy = std::min<size_t>(LATTICE_CPLD_PAGE_SIZE, remaining);

        // Skip if nothing to write (should not happen with correct num_pages calculation)
        if (bytes_to_copy == 0) {
            continue;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
        std::memcpy(page_data, buffer + buffer_offset, bytes_to_copy);

        // Write UFM page (address automatically increments)
        if (auto ret = write_ufm_page(page_data); ret != I2cStatus::Ok) {
            return ret;
        }
    }
    return I2cStatus::Ok;
}

// Read one UFM page at specified page offset (following Table 19.2)
// page_offset: UFM page number to read (0-based)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
I2cStatus LatticeCpld::read_ufm_page(uint8_t (&page_data)[LATTICE_CPLD_PAGE_SIZE],
                                     uint32_t page_offset)
{
    // Step 2: Poll Configuration Status Register
    // Repeat until Busy Flag not set, or wait 5 us if not polling
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t status_cmd[] = {LatticeCmd::READ_STATUS,
                            LatticeOperand::OP_ZERO,
                            LatticeOperand::OP_ZERO,
                            LatticeOperand::OP_ZERO};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays,misc-const-correctness)
    uint8_t       status_buf[LatticeBuffer::STATUS_BUFFER_SIZE] = {0};
    const uint8_t retry = LatticeTiming::DEFAULT_RETRY_COUNT;  // Maximum retry count

    for (uint8_t i = 0; i < retry; i++) {
        const I2cStatus ret = cpld_write_read(status_cmd, status_buf);
        if (ret != I2cStatus::Ok) {
            return ret;
        }

        // Check if device is busy (bit 12)
        if (!(status_buf[2] & LatticeStatus::UFM_BUSY_FLAG)) {
            // Busy flag cleared, device is ready
            break;
        }

        // Wait 5 us before next poll (as per datasheet)
        // Note: vTaskDelay has minimum 1ms resolution, so we delay 1ms
        vTaskDelay(pdMS_TO_TICKS(LatticeTiming::POLL_DELAY_MS));

        if (i == retry - 1) {
            return I2cStatus::Error;
        }
    }

    // Step 3: Set UFM Address to specified page offset
    // Operand format: 40 00 00 [page_offset]
    const uint8_t addr_low  = page_offset & LatticeMask::ADDR_LOW_MASK;
    const uint8_t addr_high = (page_offset >> 8) & LatticeMask::ADDR_LOW_MASK;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd_set_addr[] = {LatticeCmd::SET_ADDRESS,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::OP_ZERO,
                              LatticeOperand::UFM_ADDR_PREFIX1,
                              LatticeOperand::UFM_ADDR_PREFIX2,
                              addr_high,
                              addr_low};
    if (auto ret = cpld_write_wait_ufm(cmd_set_addr, true); ret != I2cStatus::Ok) {
        return ret;
    }

    // Step 4: Read one page UFM
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t cmd_read[] = {LatticeCmd::READ_UFM_PAGE,
                          LatticeOperand::READ_UFM_OP1,
                          LatticeOperand::READ_UFM_OP2,
                          LatticeOperand::READ_UFM_OP3};
    if (auto ret = cpld_write_read(cmd_read, page_data); ret != I2cStatus::Ok) {
        return ret;
    }

    // Step 5: Disable Configuration Interface
    // Step 6: Bypass
    return I2cStatus::Ok;
}

I2cStatus LatticeCpld::read_ufm(uint8_t* buffer, uint32_t size, uint32_t offset)
{
    nv::info("read_ufm: buffer 0x%x size 0x%x offset 0x%x\n", buffer, size, offset);

    if (size == 0) {
        return I2cStatus::Ok;  // Nothing to read
    }

    // Check alignment: both offset and size must be multiples of LATTICE_CPLD_PAGE_SIZE
    if ((offset % LATTICE_CPLD_PAGE_SIZE) != 0) {
        nv::info("read_ufm: offset 0x%x is not aligned to page size %u\n",
                 offset,
                 LATTICE_CPLD_PAGE_SIZE);
        return I2cStatus::Error;
    }
    if ((size % LATTICE_CPLD_PAGE_SIZE) != 0) {
        nv::info("read_ufm: size 0x%x is not aligned to page size %u\n",
                 size,
                 LATTICE_CPLD_PAGE_SIZE);
        return I2cStatus::Error;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    uint8_t buf[LATTICE_CPLD_PAGE_SIZE] = {0};

    for (size_t i = 0; i < size / LATTICE_CPLD_PAGE_SIZE; i++) {
        if (auto ret = read_ufm_page(buf, i + offset / LATTICE_CPLD_PAGE_SIZE);
            ret != I2cStatus::Ok) {
            return ret;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
        std::memcpy(buffer + i * LATTICE_CPLD_PAGE_SIZE, buf, LATTICE_CPLD_PAGE_SIZE);
    }

    return I2cStatus::Ok;
}