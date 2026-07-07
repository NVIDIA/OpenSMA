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
#include "nv/vrot/platform/lpu_flash_layout.h"

#include <algorithm>
#include <limits>

#include "nv/common/crc.h"

namespace nv::vrot::lpu {

namespace {

constexpr size_t FooterSpiSize = BlockSize;
constexpr size_t MacSize       = BlockSize;

// MSB-first CRC-16 lookup table, polynomial 0xA2EB.
static const std::array<uint16_t, 256> Crc16LookupTable = {
    0x0000, 0xa2eb, 0xe73d, 0x45d6, 0x6c91, 0xce7a, 0x8bac, 0x2947, 0xd922, 0x7bc9, 0x3e1f,
    0x9cf4, 0xb5b3, 0x1758, 0x528e, 0xf065, 0x10af, 0xb244, 0xf792, 0x5579, 0x7c3e, 0xded5,
    0x9b03, 0x39e8, 0xc98d, 0x6b66, 0x2eb0, 0x8c5b, 0xa51c, 0x07f7, 0x4221, 0xe0ca, 0x215e,
    0x83b5, 0xc663, 0x6488, 0x4dcf, 0xef24, 0xaaf2, 0x0819, 0xf87c, 0x5a97, 0x1f41, 0xbdaa,
    0x94ed, 0x3606, 0x73d0, 0xd13b, 0x31f1, 0x931a, 0xd6cc, 0x7427, 0x5d60, 0xff8b, 0xba5d,
    0x18b6, 0xe8d3, 0x4a38, 0x0fee, 0xad05, 0x8442, 0x26a9, 0x637f, 0xc194, 0x42bc, 0xe057,
    0xa581, 0x076a, 0x2e2d, 0x8cc6, 0xc910, 0x6bfb, 0x9b9e, 0x3975, 0x7ca3, 0xde48, 0xf70f,
    0x55e4, 0x1032, 0xb2d9, 0x5213, 0xf0f8, 0xb52e, 0x17c5, 0x3e82, 0x9c69, 0xd9bf, 0x7b54,
    0x8b31, 0x29da, 0x6c0c, 0xcee7, 0xe7a0, 0x454b, 0x009d, 0xa276, 0x63e2, 0xc109, 0x84df,
    0x2634, 0x0f73, 0xad98, 0xe84e, 0x4aa5, 0xbac0, 0x182b, 0x5dfd, 0xff16, 0xd651, 0x74ba,
    0x316c, 0x9387, 0x734d, 0xd1a6, 0x9470, 0x369b, 0x1fdc, 0xbd37, 0xf8e1, 0x5a0a, 0xaa6f,
    0x0884, 0x4d52, 0xefb9, 0xc6fe, 0x6415, 0x21c3, 0x8328, 0x8578, 0x2793, 0x6245, 0xc0ae,
    0xe9e9, 0x4b02, 0x0ed4, 0xac3f, 0x5c5a, 0xfeb1, 0xbb67, 0x198c, 0x30cb, 0x9220, 0xd7f6,
    0x751d, 0x95d7, 0x373c, 0x72ea, 0xd001, 0xf946, 0x5bad, 0x1e7b, 0xbc90, 0x4cf5, 0xee1e,
    0xabc8, 0x0923, 0x2064, 0x828f, 0xc759, 0x65b2, 0xa426, 0x06cd, 0x431b, 0xe1f0, 0xc8b7,
    0x6a5c, 0x2f8a, 0x8d61, 0x7d04, 0xdfef, 0x9a39, 0x38d2, 0x1195, 0xb37e, 0xf6a8, 0x5443,
    0xb489, 0x1662, 0x53b4, 0xf15f, 0xd818, 0x7af3, 0x3f25, 0x9dce, 0x6dab, 0xcf40, 0x8a96,
    0x287d, 0x013a, 0xa3d1, 0xe607, 0x44ec, 0xc7c4, 0x652f, 0x20f9, 0x8212, 0xab55, 0x09be,
    0x4c68, 0xee83, 0x1ee6, 0xbc0d, 0xf9db, 0x5b30, 0x7277, 0xd09c, 0x954a, 0x37a1, 0xd76b,
    0x7580, 0x3056, 0x92bd, 0xbbfa, 0x1911, 0x5cc7, 0xfe2c, 0x0e49, 0xaca2, 0xe974, 0x4b9f,
    0x62d8, 0xc033, 0x85e5, 0x270e, 0xe69a, 0x4471, 0x01a7, 0xa34c, 0x8a0b, 0x28e0, 0x6d36,
    0xcfdd, 0x3fb8, 0x9d53, 0xd885, 0x7a6e, 0x5329, 0xf1c2, 0xb414, 0x16ff, 0xf635, 0x54de,
    0x1108, 0xb3e3, 0x9aa4, 0x384f, 0x7d99, 0xdf72, 0x2f17, 0x8dfc, 0xc82a, 0x6ac1, 0x4386,
    0xe16d, 0xa4bb, 0x0650,
};

bool contains_region(size_t chunk_size, uint32_t region_offset, size_t region_len)
{
    return static_cast<uint64_t>(region_offset) + region_len <= chunk_size;
}

ErrorCode get_encrypted_header_offsets(const PointerBlock& block,
                                       uint32_t&           wrapped_cek_offset_out,
                                       uint32_t&           firmware_header_offset_out)
{
    const uint64_t spi_records_size = static_cast<uint64_t>(block.metadata.num_spi_blocks)
                                    * BlockSize;
    const uint64_t after_spi_records      = sizeof(PointerBlock) + spi_records_size;
    const uint64_t wrapped_cek_offset     = after_spi_records + FooterSpiSize;
    const uint64_t firmware_header_offset = static_cast<uint64_t>(block.metadata.fw_idx)
                                          * BlockSize;
    const uint64_t after_spi_mac = wrapped_cek_offset + sizeof(WrappedKey) + MacSize;

    // FW header must start after pointer block, SPI records, footer, wrapped CEK, and MAC.
    if (firmware_header_offset < after_spi_mac) {
        return ErrorCode::InvalidLayout;
    }
    if (wrapped_cek_offset > std::numeric_limits<uint32_t>::max()
        || firmware_header_offset > std::numeric_limits<uint32_t>::max()) {
        return ErrorCode::InvalidSize;
    }

    wrapped_cek_offset_out     = static_cast<uint32_t>(wrapped_cek_offset);
    firmware_header_offset_out = static_cast<uint32_t>(firmware_header_offset);

    return ErrorCode::Ok;
}

ErrorCode contains_regions_for_patching(std::span<const uint8_t> data,
                                        const PointerBlock&      block)
{
    if (!is_encrypted(block.metadata.is_encrypted)) {
        return ErrorCode::Ok;
    }

    uint32_t wrapped_cek_offset     = 0U;
    uint32_t firmware_header_offset = 0U;
    if (const auto status = get_encrypted_header_offsets(
            block, wrapped_cek_offset, firmware_header_offset);
        status != ErrorCode::Ok) {
        return status;
    }

    if (!contains_region(data.size(), wrapped_cek_offset, WrappedKeyPayloadSize)
        || !contains_region(data.size(), firmware_header_offset, sizeof(HeaderFirmware))) {
        return ErrorCode::InvalidSize;
    }

    return ErrorCode::Ok;
}

void patch_pointer_block(std::span<uint8_t> data, uint8_t uds_slot)
{
    std::array<uint8_t, BlockSize> bytes{};
    std::copy_n(data.begin(), bytes.size(), bytes.begin());

    auto block             = std::bit_cast<PointerBlock>(bytes);
    block.metadata.uds_idx = uds_slot;
    block.crc              = nv::common::crc16(
        std::bit_cast<std::array<uint8_t, sizeof(PointerBlockMetadata)>>(block.metadata),
        Crc16LookupTable);

    bytes = std::bit_cast<std::array<uint8_t, BlockSize>>(block);
    std::copy(bytes.begin(), bytes.end(), data.begin());
}

}  // namespace

PointerHeaderResult parse_pointer_block(std::span<const uint8_t> data)
{
    if (data.size() < sizeof(PointerBlock)) {
        return {.error = ErrorCode::InvalidSize, .block = {}};
    }

    std::array<uint8_t, sizeof(PointerBlock)> block_bytes{};
    std::copy_n(data.begin(), block_bytes.size(), block_bytes.begin());
    const auto block = std::bit_cast<PointerBlock>(block_bytes);

    if (block.metadata.magic != Magic) {
        return {.error = ErrorCode::InvalidMagic, .block = {}};
    }
    if (block.crc
        != nv::common::crc16(
            std::bit_cast<std::array<uint8_t, sizeof(PointerBlockMetadata)>>(block.metadata),
            Crc16LookupTable)) {
        return {.error = ErrorCode::CrcMismatch, .block = {}};
    }

    return {.error = ErrorCode::Ok, .block = block};
}

FirmwareDataSizeResult get_firmware_data_size(const PointerBlock& block)
{
    if (block.metadata.fw_size == 0U) {
        return {.error = ErrorCode::InvalidSize, .size = 0U};
    }

    uint64_t block_count = 0U;
    if (is_encrypted(block.metadata.is_encrypted)) {
        uint32_t wrapped_cek_offset     = 0U;
        uint32_t firmware_header_offset = 0U;
        if (const auto status = get_encrypted_header_offsets(
                block, wrapped_cek_offset, firmware_header_offset);
            status != ErrorCode::Ok) {
            return {.error = status, .size = 0U};
        }

        constexpr uint64_t FirmwareHeaderBlocks = 1U;
        constexpr uint64_t FirmwareMacBlocks    = 1U;
        block_count = static_cast<uint64_t>(block.metadata.fw_idx) + FirmwareHeaderBlocks
                    + block.metadata.fw_size + FirmwareMacBlocks;
    }
    else {
        constexpr uint64_t PointerBlocks        = 1U;
        constexpr uint64_t SpiFooterBlocks      = 1U;
        constexpr uint64_t FirmwareFooterBlocks = 1U;
        block_count = PointerBlocks + block.metadata.num_spi_blocks + SpiFooterBlocks
                    + block.metadata.fw_size + FirmwareFooterBlocks;
    }

    constexpr uint64_t MaxBlocks = std::numeric_limits<uint32_t>::max() / BlockSize;
    if (block_count > MaxBlocks) {
        return {.error = ErrorCode::InvalidSize, .size = 0U};
    }

    return {.error = ErrorCode::Ok, .size = static_cast<uint32_t>(block_count * BlockSize)};
}

PointerHeaderResult parse_pointer_header(std::span<const uint8_t> data)
{
    const auto result = parse_pointer_block(data);
    if (!result.ok()) {
        return result;
    }

    const auto& block = result.block;
    if (const auto status = contains_regions_for_patching(data, block);
        status != ErrorCode::Ok) {
        return {.error = status, .block = {}};
    }

    return {.error = ErrorCode::Ok, .block = block};
}

ErrorCode patch_encrypted_fw_header(std::span<uint8_t>                              data,
                                    const PointerBlock&                             block,
                                    uint8_t                                         uds_slot,
                                    std::span<const uint8_t, WrappedKeyPayloadSize> wrapped_cek)
{
    uint32_t wrapped_cek_offset     = 0U;
    uint32_t firmware_header_offset = 0U;
    if (const auto status = get_encrypted_header_offsets(
            block, wrapped_cek_offset, firmware_header_offset);
        status != ErrorCode::Ok) {
        return status;
    }

    patch_pointer_block(data, uds_slot);

    std::copy(wrapped_cek.begin(), wrapped_cek.end(), data.subspan(wrapped_cek_offset).begin());

    const auto header_offset = static_cast<size_t>(firmware_header_offset);
    std::array<uint8_t, sizeof(HeaderFirmware)> header_bytes{};
    std::copy_n(data.subspan(header_offset).begin(), header_bytes.size(), header_bytes.begin());

    auto header = std::bit_cast<HeaderFirmware>(header_bytes);
    if (!fw_header_is_encrypted(header.fwsize_udsidx_isencr)) {
        return ErrorCode::InvalidLayout;
    }

    const auto packed = make_fw_header_packed(
        fw_header_fw_size(header.fwsize_udsidx_isencr), uds_slot, true);
    if (!packed.ok()) {
        return packed.error;
    }

    header.fwsize_udsidx_isencr = packed.value;
    header_bytes = std::bit_cast<std::array<uint8_t, sizeof(HeaderFirmware)>>(header);
    std::copy(header_bytes.begin(), header_bytes.end(), data.subspan(header_offset).begin());

    return ErrorCode::Ok;
}

}  // namespace nv::vrot::lpu
