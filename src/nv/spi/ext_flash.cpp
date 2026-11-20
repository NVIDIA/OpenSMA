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

#include <algorithm>
#include <climits>
#include "nv/ctimer/ctimer.h"
#include "nv/ipc/supervisor.h"

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

constexpr uint32_t le32(std::span<const uint8_t> b)
{
    return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << CHAR_BIT)
         | (static_cast<uint32_t>(b[2]) << (2 * CHAR_BIT))
         | (static_cast<uint32_t>(b[3]) << (3 * CHAR_BIT));
}

constexpr uint32_t SfdpHeaderAddr = 0x000000;

// JEDEC Basic Flash Parameter Table (JESD216) field layout.
namespace Bfpt {
// Minimum DWORD counts to reach the fields we parse.
constexpr size_t PageSizeDwords   = 11;
constexpr size_t EraseTypesDwords = 9;

// DWORD 2: density value in bits 30:0.
constexpr uint32_t DensityValueMask = 0x7FFFFFFFu;

// DWORD 1: address-byte capability in bits 18:17.
constexpr unsigned AddrBytesShift = 17;
constexpr uint32_t AddrBytesMask  = 0x3u;

// DWORD 11: page size as log2(bytes) in bits 7:4.
constexpr unsigned PageSizeShift = 4;
constexpr uint32_t PageSizeMask  = 0xFu;

// DWORDs 8-9: four (size_log2, opcode) erase-type pairs starting at byte 28.
constexpr size_t  EraseTypeCount    = 4;
constexpr size_t  EraseTypeTableOff = 28;
constexpr uint8_t Sector4kLog2      = 12;  // 2^12 = 4 KB
constexpr uint8_t Block64kLog2      = 16;  // 2^16 = 64 KB
}  // namespace Bfpt

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

Status ExtFlash::read_sfdp(uint32_t sfdp_addr, std::span<uint8_t> buffer)
{
    // Opcode + 24-bit address (MSB first) + 1 dummy byte.
    std::array<uint8_t, 5> cmd = {
        ExtFlashCmd::SFDP,
        static_cast<uint8_t>(sfdp_addr >> (2 * CHAR_BIT)),
        static_cast<uint8_t>(sfdp_addr >> CHAR_BIT),
        static_cast<uint8_t>(sfdp_addr),
        0x00,
    };
    cs_assert();
    _driver.sendRecv(
        nv::ipc::Supervisor::inst().current_task_id(), cmd.size(), cmd, cmd.size(), cmd);
    _driver.sendRecv(nv::ipc::Supervisor::inst().current_task_id(),
                     buffer.size(),
                     buffer,
                     buffer.size(),
                     buffer);
    cs_deassert();
    return Status::Ok;
}

Status ExtFlash::init()
{
    if (_initialized) {
        return Status::Ok;
    }

    // SFDP header (8 bytes) + first parameter header (8 bytes).
    std::array<uint8_t, 16> hdr{};
    auto                    stat = read_sfdp(SfdpHeaderAddr, hdr);
    if (stat != Status::Ok) {
        return stat;
    }

    if (hdr[0] != 'S' || hdr[1] != 'F' || hdr[2] != 'D' || hdr[3] != 'P') {
        return Status::SfdpInvalid;
    }

    // First parameter header at offset 8. ID LSB must be 0x00 for the
    // mandatory JEDEC Basic Flash Parameter Table.
    if (hdr[8] != 0x00) {
        return Status::SfdpInvalid;
    }
    const uint8_t  bfpt_dwords = hdr[11];
    const uint32_t bfpt_ptr    = static_cast<uint32_t>(hdr[12])
                            | (static_cast<uint32_t>(hdr[13]) << 8)
                            | (static_cast<uint32_t>(hdr[14]) << 16);

    // BFPT: read up to DWORD 11 (page size lives there). Older parts may
    // expose fewer DWORDs — that's fine, we degrade to defaults below.
    constexpr size_t                  MaxBfptDwords = Bfpt::PageSizeDwords;
    constexpr size_t                  MaxBfptBytes  = MaxBfptDwords * 4;
    std::array<uint8_t, MaxBfptBytes> bfpt{};
    const size_t read_dwords = std::min(static_cast<size_t>(bfpt_dwords), MaxBfptDwords);
    stat = read_sfdp(bfpt_ptr, std::span<uint8_t>(bfpt.data(), read_dwords * 4));
    if (stat != Status::Ok) {
        return stat;
    }
    if (read_dwords < 2) {
        return Status::SfdpInvalid;  // density lives in DWORD 2
    }

    // DWORD 2 (offset 4): density. Bit 31 = encoding flag.
    //   bit31 = 0 -> density_bits = (value & 0x7FFFFFFF) + 1
    //   bit31 = 1 -> density_bits = 1 << (value & 0x7FFFFFFF)
    const uint32_t dword2       = le32(std::span(bfpt).subspan(4, 4));
    uint64_t       density_bits = 0;
    if ((dword2 & 0x80000000u) == 0) {
        density_bits = static_cast<uint64_t>(dword2 & Bfpt::DensityValueMask) + 1u;
    }
    else {
        const uint32_t n = dword2 & Bfpt::DensityValueMask;
        if (n >= 64) {
            return Status::SfdpInvalid;
        }
        density_bits = 1ULL << n;
    }
    const uint64_t capacity_bytes = density_bits / 8u;
    if (capacity_bytes == 0 || capacity_bytes > UINT32_MAX) {
        return Status::SfdpInvalid;
    }
    if (static_cast<uint32_t>(capacity_bytes) != _spec.expected_capacity) {
        return Status::CapacityMismatch;
    }
    _max_address = static_cast<uint32_t>(capacity_bytes - 1u);

    // BFPT DWORD 1 bits 18:17 = "Address Bytes": 0 = 3-byte only, 1 = 3-or-4,
    // 2 = 4-byte only. We pick the width from capacity, so for a >16MB part
    // reject one that claims 3-byte-only (inconsistent SFDP).
    if (_max_address > ExtFlashAddr::ThreeByteMax) {
        const uint32_t dword1     = le32(std::span(bfpt).subspan(0, 4));
        const auto     addr_field = static_cast<uint8_t>((dword1 >> Bfpt::AddrBytesShift)
                                                     & Bfpt::AddrBytesMask);
        if (addr_field == 0) {
            return Status::AddressModeUnsupported;
        }
    }

    // DWORD 11 (offset 40): page size in bits 7:4 as log2(bytes). Falls back
    // to 256 (near-universal SPI NOR page) if SFDP didn't expose this DWORD.
    _page_size = 256;
    if (read_dwords >= Bfpt::PageSizeDwords) {
        const uint32_t dword11 = le32(
            std::span(bfpt).subspan((Bfpt::PageSizeDwords - 1) * 4, 4));
        const auto page_log2 = static_cast<uint8_t>((dword11 >> Bfpt::PageSizeShift)
                                                    & Bfpt::PageSizeMask);
        // Reject a nonsensical 0 (1-byte page), keeping the 256-byte default.
        if (page_log2 >= 1) {
            _page_size = 1u << page_log2;
        }
    }

    // DWORDs 8-9: four (size_log2, opcode) erase-type pairs. We issue fixed 4KB
    // (SE) / 64KB (BE) opcodes, so require the part to advertise both and refuse
    // init otherwise rather than erase with an opcode it doesn't implement.
    if (read_dwords >= Bfpt::EraseTypesDwords) {
        bool has_4k_sector = false;
        bool has_64k_block = false;
        for (size_t i = 0; i < Bfpt::EraseTypeCount; ++i) {
            const size_t  off       = Bfpt::EraseTypeTableOff + i * 2;
            const uint8_t size_log2 = bfpt.at(off);
            const uint8_t opcode    = bfpt.at(off + 1);
            if (size_log2 == Bfpt::Sector4kLog2 && opcode == ExtFlashCmd::SE) {
                has_4k_sector = true;
            }
            else if (size_log2 == Bfpt::Block64kLog2 && opcode == ExtFlashCmd::BE) {
                has_64k_block = true;
            }
        }
        if (!has_4k_sector || !has_64k_block) {
            return Status::EraseTypeUnsupported;
        }
    }

    _initialized = true;
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
    _driver.sendRecv(
        nv::ipc::Supervisor::inst().current_task_id(), buf.size(), buf, buf.size(), buf);
    cs_deassert();
    return Status::Ok;
}

Status ExtFlash::read_status(uint8_t& status_reg)
{
    // Clock out [RDSR, 0x00], receive [junk, status]
    std::array<uint8_t, 2> buf = {ExtFlashCmd::RDSR, 0x00};
    cs_assert();
    _driver.sendRecv(
        nv::ipc::Supervisor::inst().current_task_id(), buf.size(), buf, buf.size(), buf);
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
    _driver.sendRecv(
        nv::ipc::Supervisor::inst().current_task_id(), buf.size(), buf, buf.size(), buf);
    cs_deassert();
    jedec_id = (static_cast<uint32_t>(buf[1]) << 16) | (static_cast<uint32_t>(buf[2]) << 8)
             | static_cast<uint32_t>(buf[3]);
    return Status::Ok;
}

uint8_t ExtFlash::addr_bytes() const
{
    return _max_address > ExtFlashAddr::ThreeByteMax ? ExtFlashAddr::FourByteLen
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
    auto init_stat = init();
    if (init_stat != Status::Ok) {
        return init_stat;
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
    _driver.sendRecv(nv::ipc::Supervisor::inst().current_task_id(), cmd_len, cmd, cmd_len, cmd);
    _driver.sendRecv(nv::ipc::Supervisor::inst().current_task_id(),
                     buffer.size(),
                     buffer,
                     buffer.size(),
                     buffer);
    cs_deassert();
    return Status::Ok;
}

Status ExtFlash::page_program(uint32_t address, std::span<const uint8_t> data)
{
    if (data.empty() || data.size() > _page_size) {
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
    _driver.sendRecv(nv::ipc::Supervisor::inst().current_task_id(), cmd_len, cmd, cmd_len, cmd);
    _driver.sendRecv(nv::ipc::Supervisor::inst().current_task_id(),
                     data.size(),
                     data_tx,
                     data.size(),
                     rx_discard);
    cs_deassert();

    return wait_busy(_spec.op_timeout_ms);
}

Status ExtFlash::write(uint8_t partition_idx, uint32_t offset, std::span<const uint8_t> buffer)
{
    auto init_stat = init();
    if (init_stat != Status::Ok) {
        return init_stat;
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
    const uint32_t sector_size  = _sector_size;

    uint32_t buf_offset = 0;
    while (buf_offset < buffer.size()) {
        const uint32_t abs_addr       = base_address + buf_offset;
        const uint32_t page_offset    = abs_addr % _page_size;
        const uint32_t bytes_to_bound = _page_size - page_offset;
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
    if (!in_partition(partition, offset, _sector_size)) {
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
    _driver.sendRecv(nv::ipc::Supervisor::inst().current_task_id(), cmd_len, cmd, cmd_len, cmd);
    cs_deassert();

    return wait_busy(_spec.op_timeout_ms);
}

Status ExtFlash::erase_chip()
{
    auto init_stat = init();
    if (init_stat != Status::Ok) {
        return init_stat;
    }
    auto stat = write_enable();
    if (stat != Status::Ok) {
        return stat;
    }

    std::array<uint8_t, 1> cmd = {ExtFlashCmd::CE};
    cs_assert();
    _driver.sendRecv(
        nv::ipc::Supervisor::inst().current_task_id(), cmd.size(), cmd, cmd.size(), cmd);
    cs_deassert();

    return wait_busy(_spec.op_timeout_ms);
}

Status ExtFlash::erase_partition(uint8_t partition_idx)
{
    auto init_stat = init();
    if (init_stat != Status::Ok) {
        return init_stat;
    }
    const auto* partition = get_partition_at(partition_idx);
    if (partition == nullptr) {
        return Status::InvalidParam;
    }

    const uint32_t block_size = _block_size;
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
    if (!in_partition(partition, offset, _block_size)) {
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
    _driver.sendRecv(nv::ipc::Supervisor::inst().current_task_id(), cmd_len, cmd, cmd_len, cmd);
    cs_deassert();

    return wait_busy(_spec.op_timeout_ms);
}
