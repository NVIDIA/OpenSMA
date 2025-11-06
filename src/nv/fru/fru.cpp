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

#include "fru.h"
#include "nv/nv.h"
#include "nv/logger/log.h"
#include "nv/i2c/common.h"
#include "sys/i2c/utils.h"
#include <cstring>
#include <algorithm>
#include <memory>

namespace nv::fru {

// FRU format constants
static constexpr uint8_t FRU_BLOCK_SIZE      = 8;     // Each FRU block is 8 bytes
static constexpr uint8_t FRU_CHECKSUM_MASK   = 0xFF;  // Checksum calculation mask
static constexpr uint8_t FRU_TLV_EOF_MARKER  = 0xC1;  // TLV End-of-Fields marker
static constexpr uint8_t FRU_TLV_LENGTH_MASK = 0x3F;  // TLV length field mask (6 bits)
static constexpr uint8_t FRU_TLV_TYPE_SHIFT  = 6;     // TLV type field shift (upper 2 bits)
static constexpr uint8_t FRU_CHASSIS_TLV_START_OFFSET = 3;  // TLVs start at byte 3 in chassis
                                                            // area
// TLV Type field constants
static constexpr uint8_t FRU_TLV_TYPE_MASK       = 0x3;  // Extract 2-bit type field
static constexpr uint8_t FRU_TLV_TYPE_6BIT_ASCII = 2;    // 6-bit ASCII (packed)

// 6-bit ASCII decoding constants
static constexpr uint8_t SIXBIT_ASCII_SPACE_OFFSET = 0x20;  // ASCII space character offset
static constexpr uint8_t SIXBIT_ASCII_MASK         = 0x3F;  // 6-bit mask (63)
static constexpr uint8_t SIXBIT_CHARS_PER_GROUP    = 4;     // 4 characters per 3-byte group
static constexpr uint8_t SIXBIT_CASE1_SHIFT        = 2;  // Shift for case 1 (low 4 bits << 2)
static constexpr uint8_t SIXBIT_CASE2_SHIFT = 4;  // Shift for case 2 (high 4 bits >> 4, low 2
                                                  // << 4)
static constexpr uint8_t SIXBIT_CASE3_SHIFT = 2;  // Shift for case 3 (high 6 bits >> 2)

/**
 * Simple FRU structures (minimal implementation)
 */
struct FruHeader
{
    uint8_t format_version;
    uint8_t internal_offset;
    uint8_t chassis_offset;  // This is what we need - in 8-byte blocks
    uint8_t board_offset;
    uint8_t product_offset;
    uint8_t multirecord_offset;
    uint8_t pad;
    uint8_t checksum;
};

/**
 * Helper: Calculate checksum (sum of bytes must equal 0 mod 256)
 */
bool verify_checksum(const std::span<const uint8_t>& data)
{
    uint32_t sum = 0;
    for (const uint8_t byte : data) {
        // Overflow is intentional for checksum calculation
        // coverity[cert_int30_c_violation]
        sum += byte;
    }
    return (sum & FRU_CHECKSUM_MASK) == 0;
}

/**
 * Convert I2C status to FRU status
 */
Status i2c_to_fru_status(nv::i2c::I2cStatus i2c_status)
{
    switch (i2c_status) {
        case nv::i2c::I2cStatus::Ok     : return Status::FruSuccess;
        case nv::i2c::I2cStatus::Busy   : return Status::FruI2cBusy;
        case nv::i2c::I2cStatus::Nak    : return Status::FruI2cNack;
        case nv::i2c::I2cStatus::Timeout: return Status::FruI2cTimeout;
        default                         : return Status::FruI2cError;
    }
}

Status read_eeprom_bytes(nv::i2c::Port      port,
                         uint8_t            eeprom_addr,
                         uint16_t           offset,
                         std::span<uint8_t> buffer)
{
    // EEPROM bank constants
    static constexpr uint16_t EEPROM_BANK_SIZE = 256;   // 256-byte banks
    static constexpr uint8_t  BANK_SHIFT_BITS  = 8;     // Shift to get bank number
    static constexpr uint8_t  BANK_OFFSET_MASK = 0xFF;  // Mask to get offset within bank

    // Implement Random Read per byte with 256-byte bank splitting.
    if (buffer.empty()) {
        return Status::FruInvalidData;
    }

    // EEPROM operations are limited to reasonable buffer sizes (typically < 8KB)
    // coverity[cert_int31_c_violation]
    auto     remaining = static_cast<uint16_t>(buffer.size());
    uint16_t written   = 0;

    while (remaining > 0) {
        // Determine 256-byte bank and in-bank offset
        const auto bank = static_cast<uint8_t>(offset >> BANK_SHIFT_BITS);
        // I2C addresses are 7-bit (0-127), bank addition is bounded by EEPROM size
        // coverity[cert_int31_c_violation]
        const auto     device_address = static_cast<uint8_t>(eeprom_addr + bank);
        const auto     bank_offset    = static_cast<uint8_t>(offset & BANK_OFFSET_MASK);
        const uint16_t space_in_bank  = EEPROM_BANK_SIZE - static_cast<uint16_t>(bank_offset);
        const uint16_t chunk          = std::min(remaining, space_in_bank);

        // EEPROM Sequential Read
        const uint8_t          memory_offset = bank_offset;
        std::array<uint8_t, 1> write_data    = {memory_offset};

        auto status = sys::i2c::i2c_write_read(port,
                                               device_address,
                                               std::span(write_data),
                                               std::span(buffer.data() + written, chunk));

        if (status != nv::i2c::I2cStatus::Ok) {
            nv::logger::error(nv::logger::Event::FruI2cError,
                              {eeprom_addr,
                               static_cast<uint8_t>(offset >> BANK_SHIFT_BITS),
                               static_cast<uint8_t>(offset & BANK_OFFSET_MASK)});
            return i2c_to_fru_status(status);
        }

        // Advance to next bank/offset region
        // offset and chunk are both uint16_t, sum bounded by EEPROM address space
        // coverity[cert_int31_c_violation]
        offset = static_cast<uint16_t>(offset + chunk);
        // written is bounded by buffer.size(), chunk <= remaining by loop invariant
        // coverity[cert_int30_c_violation]
        written += chunk;
        // remaining >= chunk by loop invariant, subtraction result always positive
        // coverity[cert_int31_c_violation]
        remaining = remaining - chunk;
    }

    return Status::FruSuccess;
}

/**
 * Main function: Extract chassis info (serial + custom fields) into fixed buffers
 */
Status get_chassis_info(nv::i2c::Port port, uint8_t eeprom_addr, ChassisInfo& out)
{
    // Initialize output
    out = {};

    // Read FRU Common Header (8 bytes at offset 0)
    std::array<uint8_t, 8> header_buf{};
    Status result = read_eeprom_bytes(port, eeprom_addr, 0, std::span(header_buf));
    if (result != Status::FruSuccess) {
        return result;
    }

    // Parse and validate header
    if (!verify_checksum(std::span<const uint8_t>(header_buf))) {
        nv::logger::error(nv::logger::Event::FruChecksumError, {eeprom_addr});
        return Status::FruChecksumError;
    }

    FruHeader header{};
    std::memcpy(&header, header_buf.data(), sizeof(header));

    // Check if Chassis Info Area exists
    if (header.chassis_offset == 0) {
        nv::logger::error(nv::logger::Event::FruNoChassisArea, {eeprom_addr});
        return Status::FruNoChassisArea;
    }

    // Read Chassis Info Area
    const auto chassis_byte_offset = static_cast<uint16_t>(header.chassis_offset)
                                   * FRU_BLOCK_SIZE;  // Convert
                                                      // blocks to
                                                      // bytes

    // Read first 2 bytes to get area length
    std::array<uint8_t, 2> chassis_header{};
    result = read_eeprom_bytes(
        port, eeprom_addr, chassis_byte_offset, std::span(chassis_header));
    if (result != Status::FruSuccess) {
        return result;
    }

    const uint8_t  len_blocks          = chassis_header[1];
    const uint16_t chassis_total_bytes = static_cast<uint16_t>(len_blocks) * FRU_BLOCK_SIZE;

    // Calculate precise maximum chassis data size based on actual TLV requirements:
    // - Chassis area header: 3 bytes (format, length, chassis_type)
    // - TLV1 (Part Number): 1 + 63 = 64 bytes max (Type/Length + data)
    // - TLV2 (Serial Number): 1 + 63 = 64 bytes max (Type/Length + data)
    // - TLV3 (Custom Field): 1 + 63 = 64 bytes max (Type/Length + data)
    // - EOF marker: 1 byte (0xC1)
    // - Checksum: 1 byte
    // - Padding to 8-byte boundary: up to 7 bytes
    // Total: 3 + 64 + 64 + 64 + 1 + 1 + 7 = 204 bytes
    // Round up to nearest 8-byte block: 208 bytes (26 blocks)
    static constexpr uint16_t MAX_CHASSIS_DATA_SIZE = 208;
    if (chassis_total_bytes > MAX_CHASSIS_DATA_SIZE) {
        nv::logger::error(nv::logger::Event::FruInvalidData, {eeprom_addr});
        return Status::FruInvalidData;
    }
    std::array<uint8_t, MAX_CHASSIS_DATA_SIZE> chassis_data{};
    result = read_eeprom_bytes(port,
                               eeprom_addr,
                               chassis_byte_offset,
                               std::span(chassis_data.data(), chassis_total_bytes));
    if (result != Status::FruSuccess) {
        return result;
    }

    // Validate chassis area checksum
    if (!verify_checksum(std::span<const uint8_t>(chassis_data.data(), chassis_total_bytes))) {
        nv::logger::error(nv::logger::Event::FruChecksumError, {eeprom_addr});
        return Status::FruChecksumError;
    }

    // Extract Chassis Type (byte 2, NOT a TLV - fixed field)
    out.chassis_type = chassis_data[2];

    // Parse TLVs: TLVs start at Byte3
    // chassis_total_bytes is bounded by FRU specification (max ~200 bytes)
    // coverity[cert_int31_c_violation]
    const uint16_t area_data_end = chassis_total_bytes - 1;  // exclude checksum byte at the end
    uint16_t       idx           = FRU_CHASSIS_TLV_START_OFFSET;
    if (idx >= area_data_end) {
        nv::logger::error(nv::logger::Event::FruInvalidData, {eeprom_addr});
        return Status::FruParseError;
    }

    auto copy_bytes = [](std::span<const uint8_t> src, std::span<uint8_t> dst) {
        if (dst.empty()) {
            return;
        }
        // FRU TLV fields are bounded by specification (max 63 bytes per field)
        // coverity[cert_int31_c_violation]
        const uint8_t n = std::min(static_cast<uint8_t>(src.size()),
                                   static_cast<uint8_t>(dst.size() - 1));
        std::memcpy(dst.data(), src.data(), n);
        dst[n] = '\0';  // NUL terminator
    };

    // Decode 6-bit ASCII packed data using frugen algorithm
    auto decode_6bit_ascii =
        [](const uint8_t* data, uint8_t tlv_len, std::span<uint8_t> dst) -> bool {
        // Clear destination buffer first
        std::fill(dst.begin(), dst.end(), 0);

        // Check minimum data requirement (need at least 1 byte)
        if (tlv_len == 0) {
            return false;
        }

        uint8_t i6 = 0;  // Index into 6-bit packed input data

        // Decode characters using frugen's 4-to-3 algorithm
        // FRU spec guarantees max 63 chars + null terminator = 64 bytes max
        // coverity[cert_int31_c_violation] - FRU spec limits dst.size() to 64 max
        const uint16_t dst_len = dst.size();  // Direct assignment from size_t to uint16_t
        // coverity[infinite_loop] - dst_len bounded by FRU spec (≤64)
        for (uint16_t i = 0; i < dst_len; i++) {
            const uint8_t byte_pos = i % SIXBIT_CHARS_PER_GROUP;  // Position within 4-character
                                                                  // group (0,1,2,3)
            uint8_t char_6bit = 0;

            switch (byte_pos) {
                case 0:  // First character: take low 6 bits of current byte
                    if (i6 >= tlv_len) {
                        return false;
                    }
                    char_6bit = data[i6] & SIXBIT_ASCII_MASK;
                    break;

                case 1:  // Second character: high 2 bits of current + low 4 bits of next
                    if (i6 + 1 >= tlv_len) {
                        return false;
                    }
                    // 6-bit ASCII bit operations are bounded by uint8_t input values
                    // coverity[cert_int31_c_violation]
                    char_6bit = (data[i6] >> FRU_TLV_TYPE_SHIFT)
                              | (data[i6 + 1] << SIXBIT_CASE1_SHIFT);
                    i6++;
                    break;

                case 2:  // Third character: high 4 bits of current + low 2 bits of next
                    if (i6 + 1 >= tlv_len) {
                        return false;
                    }
                    // 6-bit ASCII bit operations are bounded by uint8_t input values
                    // coverity[cert_int31_c_violation]
                    char_6bit = (data[i6] >> SIXBIT_CASE2_SHIFT)
                              | (data[i6 + 1] << SIXBIT_CASE2_SHIFT);
                    i6++;
                    break;

                case 3:  // Fourth character: high 6 bits of current byte
                    if (i6 >= tlv_len) {
                        return false;
                    }
                    char_6bit = data[i6] >> SIXBIT_CASE3_SHIFT;
                    i6++;
                    break;
                // Default case handles unexpected values for defensive programming
                // coverity[dead_error_begin]
                default: return false;
            }

            // Ensure only 6 bits and convert to ASCII
            char_6bit                &= SIXBIT_ASCII_MASK;
            const uint8_t ascii_char  = char_6bit + SIXBIT_ASCII_SPACE_OFFSET;  // Add space
                                                                                // offset

            dst[i] = ascii_char;
        }

        return true;  // Success
    };

    auto process_tlv =
        [&](std::span<const uint8_t> buf, uint16_t& pos, std::span<uint8_t> dst = {}) -> bool {
        if (pos >= area_data_end) {
            return false;
        }

        const uint8_t tl = buf[pos++];
        if (tl == FRU_TLV_EOF_MARKER) {
            return (dst.empty());
        }
        const uint8_t tlv_type = static_cast<uint8_t>(tl >> FRU_TLV_TYPE_SHIFT)
                               & FRU_TLV_TYPE_MASK;
        const auto tlv_len = static_cast<uint8_t>(tl & FRU_TLV_LENGTH_MASK);

        if (pos + tlv_len > area_data_end) {
            return false;
        }

        if (!dst.empty()) {
            const uint8_t* data = &buf[pos];
            if (tlv_type == FRU_TLV_TYPE_6BIT_ASCII) {
                if (!decode_6bit_ascii(data, tlv_len, dst)) {
                    nv::logger::error(nv::logger::Event::FruTlvDecodeError,
                                      {eeprom_addr, static_cast<uint8_t>(pos)});
                    return false;
                }
            }
            else {
                copy_bytes(std::span(data, tlv_len), dst);
            }
        }

        pos += tlv_len;
        return true;
    };

    // TLV1 = Part Number (skip)
    if (!process_tlv(std::span<const uint8_t>(chassis_data), idx)) {
        nv::logger::error(nv::logger::Event::FruPartNumberError, {eeprom_addr});
        return Status::FruPartNumberError;
    }

    // TLV2 = Serial Number
    if (!process_tlv(std::span<const uint8_t>(chassis_data), idx, std::span(out.serial))) {
        nv::logger::error(nv::logger::Event::FruSerialError, {eeprom_addr});
        return Status::FruSerialError;
    }

    // TLV3 = either EOF or first Custom Field
    out.num_custom_fields = 0;
    if (idx < area_data_end) {
        auto          chassis_span = std::span(chassis_data.data(), chassis_total_bytes);
        const uint8_t tl3          = chassis_span[idx];
        if (tl3 != FRU_TLV_EOF_MARKER) {
            if (!process_tlv(std::span<const uint8_t>(chassis_data),
                             idx,
                             std::span(out.custom_fields[0]))) {
                nv::logger::error(nv::logger::Event::FruCustomFieldError, {eeprom_addr});
                return Status::FruCustomFieldError;
            }
            out.num_custom_fields = 1;
        }
    }

    return Status::FruSuccess;
}

}  // namespace nv::fru
