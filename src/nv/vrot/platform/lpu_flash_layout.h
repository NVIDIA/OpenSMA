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
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace nv::vrot::lpu {

// LPU fw-data layout (ApFwMetadata 4096 B is written separately; tables are fw-data only).
//
// Units:
//   WORD  = 16 bits (2 B).  Table row = 4 WORDs = 8 B.
//   Block# = 16 B block index (byte offset = Block# × 16).  Two consecutive rows with the
//   same Block# describe one block (bytes 0–7 then bytes 8–15).
//
// Cleartext (pointer block is_encrypted == 0):
//
// Blk  Name          W0[15:0]         W1[15:0]         W2[15:0]         W3[15:0]
// ---  ------------ ---------------- ---------------- ---------------- ----------------
// 0    PointerBlock  magic=0x1F27     num_spi_blocks   fw_idx[15:0]     fw_idx[31:16]
// 0                  fw_size[15:0]    fw_size[31:16]   uds_idx,enc=0    crc16
// 1    SPI record    spi_cmd          spi_addr         data0            data1
// 1                  record data      record data      record data      record data
// ...  SPI records   (blocks 2..N, same 16 B / 2-row layout as block 1)
// N+1  SPI footer    reserved         reserved         reserved         reserved
// N+1                reserved         reserved         reserved         crc16
// idx  FW image      firmware data    firmware data    firmware data    firmware data
// idx                firmware data    firmware data    firmware data    firmware data
// ...  FW image      (... fw_size payload spans multiple blocks ...)
// end  FW footer     footer/reserved  footer/reserved  footer/reserved  footer/reserved
// end                footer/reserved  footer/reserved  footer/reserved  crc16
//
// Encrypted (pointer block is_encrypted != 0):
//
// Blk  Name          W0[15:0]         W1[15:0]         W2[15:0]         W3[15:0]
// ---  ------------ ---------------- ---------------- ---------------- ----------------
// 0    PointerBlock  magic=0x1F27     num_spi_blocks   fw_idx[15:0]     fw_idx[31:16]
// 0                  fw_size[15:0]    fw_size[31:16]   uds_idx,enc=1    crc16
// 1    SPI record    spi_cmd          spi_addr         data0            data1
// 1                  record data      record data      record data      record data
// ...  SPI records   (blocks 2..N, same 16 B / 2-row layout as block 1)
// N+1  SPI footer    spi_iv[15:0]     spi_iv[31:16]    spi_iv[47:32]    spi_iv[63:48]
// N+1                spi_iv[79:64]    spi_iv[95:80]    reserved         reserved
// N+2  WrappedKey    kw_cek[15:0]     kw_cek[31:16]    kw_cek[47:32]    kw_cek[63:48]
// N+2                kw_cek[79:64]    kw_cek[95:80]    kw_cek[111:96]   kw_cek[127:112]
// N+3  WrappedKey    kw_cek[143:128]  kw_cek[159:144]  kw_cek[175:160]  kw_cek[191:176]
// N+3                kw_cek[207:192]  kw_cek[223:208]  kw_cek[239:224]  kw_cek[255:240]
// N+4  WrappedKey    kw_cek[271:256]  kw_cek[287:272]  kw_cek[303:288]  kw_cek[319:304]
// N+4                reserved         reserved         reserved         reserved
// N+5  SPI MAC       spi_mac[15:0]    spi_mac[31:16]   spi_mac[47:32]   spi_mac[63:48]
// N+5                spi_mac[79:64]   spi_mac[95:80]   spi_mac[111:96]  spi_mac[127:112]
// ...  Padding       (blocks until fw_idx)
// idx  HeaderFw      hdr_packed[15:0] hdr_packed[31:16] fw_iv[15:0]     fw_iv[31:16]
// idx                fw_iv[47:32]     fw_iv[63:48]     fw_iv[79:64]     fw_iv[95:80]
// +1   FW image      ciphertext       ciphertext       ciphertext       ciphertext
// +1                 ciphertext       ciphertext       ciphertext       ciphertext
// ...  FW image      (... fw_size payload spans multiple blocks ...)
// end  FW MAC        fw_mac[15:0]     fw_mac[31:16]    fw_mac[47:32]    fw_mac[63:48]
// end                fw_mac[79:64]    fw_mac[95:80]    fw_mac[111:96]   fw_mac[127:112]
//
// PointerBlock is_encrypted uses EncryptedFlagMask (bit 0). MCU patches encrypted images
// on the first PLDM chunk: uds_idx, WrappedKey (UDS-KW-wrapped CEK), hdr_packed uds_idx.
// HeaderFirmware hdr_packed: fw_size[19:0], uds_idx[23:20], encrypted[31] (HeaderFirmware).

constexpr size_t   BlockSize = 16;
constexpr uint16_t Magic     = 0x1F27;

static_assert(std::endian::native == std::endian::little,
              "LPU flash layout serialization assumes a little-endian target");

constexpr uint16_t CrcPolynomial = 0xA2EB;

constexpr uint8_t EncryptedFlagMask = 0x01;

constexpr bool is_encrypted(uint8_t flags)
{
    return (flags & EncryptedFlagMask) != 0;
}

enum class ErrorCode : uint8_t
{
    Ok,
    InvalidSize,
    InvalidMagic,
    CrcMismatch,
    InvalidLayout,
};

struct [[gnu::packed]] PointerBlockMetadata
{
    uint16_t magic;
    uint16_t num_spi_blocks;
    uint32_t fw_idx;
    uint32_t fw_size;
    uint8_t  uds_idx;
    uint8_t  is_encrypted;
};

struct [[gnu::packed]] PointerBlock
{
    PointerBlockMetadata metadata;
    uint16_t             crc;
};

struct [[gnu::packed]] WrappedKey
{
    uint64_t blob1;
    uint64_t blob2;
    uint64_t blob3;
    uint64_t blob4;
    uint64_t blob5;
    uint64_t reserved;
};

struct [[gnu::packed]] HeaderFirmware
{
    // LPU spec packs the first dword as one little-endian uint32 (not separate bytes):
    //   [31]     is_encrypted (1 bit)
    //   [30:24]  reserved (7 bits, must be 0 in packaged images)
    //   [23:20]  uds_idx (4 bits)
    //   [19:0]   fw_size (20 bits)
    // Unpack/pack via fw_header_fw_size(), fw_header_is_encrypted(), make_fw_header_packed().
    uint32_t fwsize_udsidx_isencr;
    uint32_t iv1;
    uint64_t iv2;
};

static_assert(sizeof(PointerBlockMetadata) == BlockSize - sizeof(uint16_t));
static_assert(sizeof(PointerBlock) == BlockSize);
static_assert(sizeof(WrappedKey) == BlockSize * 3);
static_assert(sizeof(HeaderFirmware) == BlockSize);

constexpr uint32_t HeaderFwSizeMask     = 0xFFFFFU;
constexpr uint32_t HeaderUdsIdxShift    = 20U;
constexpr uint32_t HeaderUdsIdxMask     = 0xFU;
constexpr uint32_t HeaderEncryptedShift = 31U;

constexpr uint32_t fw_header_fw_size(uint32_t packed)
{
    return packed & HeaderFwSizeMask;
}

constexpr bool fw_header_is_encrypted(uint32_t packed)
{
    return (packed & (1U << HeaderEncryptedShift)) != 0U;
}

struct PackedFwHeaderResult
{
    ErrorCode error;
    uint32_t  value;

    constexpr bool ok() const { return error == ErrorCode::Ok; }
};

constexpr PackedFwHeaderResult
make_fw_header_packed(uint32_t fw_size, uint8_t uds_idx, bool encrypted)
{
    if ((fw_size & ~HeaderFwSizeMask) != 0U) {
        return {.error = ErrorCode::InvalidSize, .value = 0U};
    }
    if ((uds_idx & ~HeaderUdsIdxMask) != 0U) {
        return {.error = ErrorCode::InvalidSize, .value = 0U};
    }
    return {.error = ErrorCode::Ok,
            .value = fw_size | (static_cast<uint32_t>(uds_idx) << HeaderUdsIdxShift)
                   | (encrypted ? (1U << HeaderEncryptedShift) : 0U)};
}

constexpr size_t WrappedKeyPayloadSize = sizeof(WrappedKey);

struct PointerHeaderResult
{
    ErrorCode    error;
    PointerBlock block;

    constexpr bool ok() const { return error == ErrorCode::Ok; }
};

struct FirmwareDataSizeResult
{
    ErrorCode error;
    uint32_t  size;

    constexpr bool ok() const { return error == ErrorCode::Ok; }
};

// Parse and validate the 16-byte LPU pointer block at the start of `data`.
PointerHeaderResult parse_pointer_block(std::span<const uint8_t> data);

// Return the complete LPU fw-data extent, including required headers and
// footer/MAC blocks. PointerBlock fw_size is expressed in 16-byte blocks.
FirmwareDataSizeResult get_firmware_data_size(const PointerBlock& block);

// Validate the pointer block and verify that the first PLDM fw-data chunk
// contains every region needed for encrypted-header patching.
PointerHeaderResult parse_pointer_header(std::span<const uint8_t> data);

// Patch pointer block, wrapped CEK, and FW header in the first PLDM chunk.
// Caller must have already validated region containment via parse_pointer_header().
ErrorCode
patch_encrypted_fw_header(std::span<uint8_t>                              data,
                          const PointerBlock&                             block,
                          uint8_t                                         uds_slot,
                          std::span<const uint8_t, WrappedKeyPayloadSize> wrapped_cek);

}  // namespace nv::vrot::lpu
