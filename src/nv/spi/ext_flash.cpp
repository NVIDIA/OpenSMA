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

#include "nv/spi/ext_flash.h"

#include <climits>
#include "nv/ctimer/ctimer.h"

using namespace nv::spi;
using Status = ExtFlash::Status;

namespace {

constexpr bool in_partition(const FlashPartition& p, uint32_t offset, uint32_t len)
{
    if (offset > p.size) {
        return false;
    }
    return len <= (p.size - offset);
}

}  // namespace

ExtFlash::ExtFlash(sys::spi::EdmaDriver&           driver,
                   nv::gpio::GpioPort              cs_port,
                   nv::gpio::GpioPin               cs_pin,
                   std::span<const FlashPartition> partitions,
                   FlashSpec                       spec) noexcept
: _driver(driver)
, _cs_port(cs_port)
, _cs_pin(cs_pin)
, _partitions(partitions)
, _spec(spec)
{}

void ExtFlash::make(sys::spi::EdmaDriver&           driver,
                    nv::gpio::GpioPort              cs_port,
                    nv::gpio::GpioPin               cs_pin,
                    std::span<const FlashPartition> partitions,
                    FlashSpec                       spec) noexcept
{
    ExtFlashInst::ext_flash.emplace(driver, cs_port, cs_pin, partitions, spec);
}

ExtFlash& ExtFlash::inst()
{
    return *ExtFlashInst::ext_flash;
}

const FlashPartition* ExtFlash::get_partition_at(uint8_t idx) const
{
    if (idx >= _partitions.size()) {
        return nullptr;
    }
    return &_partitions[idx];
}

void ExtFlash::cs_assert()
{
    nv::gpio::Driver::write(_cs_port, _cs_pin, 0);
}

void ExtFlash::cs_deassert()
{
    nv::gpio::Driver::write(_cs_port, _cs_pin, 1);
}

void ExtFlash::clear_last_known_write()
{
    _last_written_addr = UINT32_MAX;
}

Status ExtFlash::verify_id()
{
    uint32_t id   = 0;
    auto     stat = read_id(id);
    if (stat != Status::Ok) {
        return stat;
    }
    if (id != _spec.jedec_id) {
        return Status::IdMismatch;
    }
    return Status::Ok;
}

Status ExtFlash::write_enable()
{
    uint8_t sr   = 0;
    auto    stat = read_status(sr);
    if (stat != Status::Ok) {
        return stat;
    }
    if ((sr & ExtFlashStatusBit::WIP) != 0) {
        return Status::Busy;
    }

    std::array<uint8_t, 1> buf = {ExtFlashCmd::WREN};
    cs_assert();
    _driver.sendRecv(buf.size(), buf, buf.size(), buf);
    cs_deassert();
    return Status::Ok;
}

Status ExtFlash::read_status(uint8_t& status_reg)
{
    // Clock out [RDSR, 0x00], receive [junk, status]
    std::array<uint8_t, 2> buf = {ExtFlashCmd::RDSR, 0x00};
    cs_assert();
    _driver.sendRecv(buf.size(), buf, buf.size(), buf);
    cs_deassert();
    status_reg = buf[1];
    return Status::Ok;
}

Status ExtFlash::wait_busy(uint32_t timeout_ms)
{
    timeout_ms       = ((timeout_ms + 4) / 5) * 5;
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        uint8_t sr   = 0;
        auto    stat = read_status(sr);
        if (stat != Status::Ok) {
            return stat;
        }
        if ((sr & ExtFlashStatusBit::WIP) == 0) {
            return Status::Ok;
        }
        // TODO: remove, replace with nv::ipc::delay() when it lands.
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        nv::ctimer::Driver::delay_for_us(5000);
        elapsed += 5;
    }
    return Status::Timeout;
}

Status ExtFlash::read_id(uint32_t& jedec_id)
{
    // Clock out [RDID, 0, 0, 0], receive [junk, mfg, mem_type, capacity]
    std::array<uint8_t, 4> buf = {ExtFlashCmd::RDID, 0x00, 0x00, 0x00};
    cs_assert();
    _driver.sendRecv(buf.size(), buf, buf.size(), buf);
    cs_deassert();
    jedec_id = (static_cast<uint32_t>(buf[1]) << 16) | (static_cast<uint32_t>(buf[2]) << 8)
             | static_cast<uint32_t>(buf[3]);
    return Status::Ok;
}

uint8_t ExtFlash::addr_bytes() const
{
    return _spec.max_address > ExtFlashAddr::ThreeByteMax ? ExtFlashAddr::FourByteLen
                                                          : ExtFlashAddr::ThreeByteLen;
}

uint8_t ExtFlash::build_addressed_cmd(std::array<uint8_t, ExtFlashAddr::MaxLen + 1>& cmd_buf,
                                      uint8_t  opcode_3byte,
                                      uint8_t  opcode_4byte,
                                      uint32_t address) const
{
    const uint8_t n = addr_bytes();
    cmd_buf[0]      = (n == ExtFlashAddr::FourByteLen) ? opcode_4byte : opcode_3byte;
    for (uint8_t i = 0; i < n; ++i) {
        const uint8_t shift = (n - 1 - i) * CHAR_BIT;
        cmd_buf.at(1 + i)   = static_cast<uint8_t>(address >> shift);
    }
    return n + 1;
}

Status ExtFlash::read(uint8_t partition_idx, uint32_t offset, std::span<uint8_t> buffer)
{
    auto vstat = verify_id();
    if (vstat != Status::Ok) {
        return vstat;
    }
    if (buffer.empty()) {
        return Status::InvalidParam;
    }
    const auto* partition = get_partition_at(partition_idx);
    if (partition == nullptr) {
        return Status::InvalidParam;
    }
    if (!in_partition(*partition, offset, buffer.size())) {
        return Status::InvalidParam;
    }

    const uint32_t address = partition->base + offset;

    std::array<uint8_t, ExtFlashAddr::MaxLen + 1> cmd{};
    const uint8_t                                 cmd_len = build_addressed_cmd(
        cmd, ExtFlashCmd::READ, ExtFlashCmd::READ4, address);
    cs_assert();
    _driver.sendRecv(cmd_len, cmd, cmd_len, cmd);
    _driver.sendRecv(buffer.size(), buffer, buffer.size(), buffer);
    cs_deassert();
    return Status::Ok;
}

Status ExtFlash::page_program(uint32_t address, std::span<const uint8_t> data)
{
    if (data.empty() || data.size() > _spec.page_size) {
        return Status::InvalidParam;
    }

    auto stat = write_enable();
    if (stat != Status::Ok) {
        return stat;
    }

    std::array<uint8_t, ExtFlashAddr::MaxLen + 1> cmd{};
    const uint8_t                                 cmd_len = build_addressed_cmd(
        cmd, ExtFlashCmd::PP, ExtFlashCmd::PP4, address);

    // EdmaDriver::sendRecv takes a mutable TX span but the underlying SDK never
    // writes to txData. Cast confined to this single SDK boundary.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    const std::span<uint8_t> data_tx(const_cast<uint8_t*>(data.data()), data.size());
    const std::span<uint8_t> rx_discard{};

    cs_assert();
    _driver.sendRecv(cmd_len, cmd, cmd_len, cmd);
    _driver.sendRecv(data.size(), data_tx, data.size(), rx_discard);
    cs_deassert();

    return wait_busy(_spec.op_timeout_ms);
}

Status ExtFlash::write(uint8_t partition_idx, uint32_t offset, std::span<const uint8_t> buffer)
{
    auto vstat = verify_id();
    if (vstat != Status::Ok) {
        return vstat;
    }
    if (buffer.empty()) {
        return Status::InvalidParam;
    }
    const auto* partition = get_partition_at(partition_idx);
    if (partition == nullptr) {
        return Status::InvalidParam;
    }
    if (!in_partition(*partition, offset, buffer.size())) {
        return Status::InvalidParam;
    }

    const uint32_t base_address = partition->base + offset;
    const uint32_t sector_size  = _spec.sector_size;

    uint32_t buf_offset = 0;
    while (buf_offset < buffer.size()) {
        const uint32_t abs_addr       = base_address + buf_offset;
        const uint32_t page_offset    = abs_addr % _spec.page_size;
        const uint32_t bytes_to_bound = _spec.page_size - page_offset;
        const uint32_t remaining      = buffer.size() - buf_offset;
        const uint32_t chunk_size = (remaining < bytes_to_bound) ? remaining : bytes_to_bound;

        // Lazy sector erase: skip when extending forward into the same sector
        // as the last write; erase otherwise (first write, backward, new sector).
        const bool same_sector_forward = (abs_addr > _last_written_addr)
                                      && (abs_addr / sector_size
                                          == _last_written_addr / sector_size);
        if (!same_sector_forward) {
            const uint32_t sector_part_offset = ((abs_addr - partition->base) / sector_size)
                                              * sector_size;
            auto erase_stat = erase_sector(*partition, sector_part_offset);
            if (erase_stat != Status::Ok) {
                return erase_stat;
            }
        }

        auto stat = page_program(abs_addr, buffer.subspan(buf_offset, chunk_size));
        if (stat != Status::Ok) {
            return stat;
        }
        _last_written_addr  = abs_addr + chunk_size - 1;
        buf_offset         += chunk_size;
    }
    return Status::Ok;
}

Status ExtFlash::erase_sector(const FlashPartition& partition, uint32_t offset)
{
    if (!in_partition(partition, offset, _spec.sector_size)) {
        return Status::InvalidParam;
    }

    auto stat = write_enable();
    if (stat != Status::Ok) {
        return stat;
    }

    const uint32_t address = partition.base + offset;

    std::array<uint8_t, ExtFlashAddr::MaxLen + 1> cmd{};
    const uint8_t                                 cmd_len = build_addressed_cmd(
        cmd, ExtFlashCmd::SE, ExtFlashCmd::SE4, address);
    cs_assert();
    _driver.sendRecv(cmd_len, cmd, cmd_len, cmd);
    cs_deassert();

    return wait_busy(_spec.op_timeout_ms);
}

Status ExtFlash::erase_chip()
{
    auto vstat = verify_id();
    if (vstat != Status::Ok) {
        return vstat;
    }
    auto stat = write_enable();
    if (stat != Status::Ok) {
        return stat;
    }

    std::array<uint8_t, 1> cmd = {ExtFlashCmd::CE};
    cs_assert();
    _driver.sendRecv(cmd.size(), cmd, cmd.size(), cmd);
    cs_deassert();

    return wait_busy(_spec.op_timeout_ms);
}

Status ExtFlash::erase_partition(uint8_t partition_idx)
{
    auto vstat = verify_id();
    if (vstat != Status::Ok) {
        return vstat;
    }
    const auto* partition = get_partition_at(partition_idx);
    if (partition == nullptr) {
        return Status::InvalidParam;
    }

    const uint32_t block_size = _spec.block_size;
    for (uint32_t offset = 0; offset < partition->size; offset += block_size) {
        auto stat = erase_block(*partition, offset);
        if (stat != Status::Ok) {
            return stat;
        }
    }
    return Status::Ok;
}

Status ExtFlash::erase_block(const FlashPartition& partition, uint32_t offset)
{
    if (!in_partition(partition, offset, _spec.block_size)) {
        return Status::InvalidParam;
    }

    auto stat = write_enable();
    if (stat != Status::Ok) {
        return stat;
    }

    const uint32_t address = partition.base + offset;

    std::array<uint8_t, ExtFlashAddr::MaxLen + 1> cmd{};
    const uint8_t                                 cmd_len = build_addressed_cmd(
        cmd, ExtFlashCmd::BE, ExtFlashCmd::BE4, address);
    cs_assert();
    _driver.sendRecv(cmd_len, cmd, cmd_len, cmd);
    cs_deassert();

    return wait_busy(_spec.op_timeout_ms);
}
