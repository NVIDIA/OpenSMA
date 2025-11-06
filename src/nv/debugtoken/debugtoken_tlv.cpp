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

#include <cstdint>
#include <cstring>
#include <span>

#include "mbedtls/sha512.h"

#include "nv/common/build.h"
#include "nv/debugtoken/debugtoken.h"
#include "nv/flash/datastore.h"
#include "nv/flash/flash.h"
#include "nv/logger/log.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/spdm/spdm_get_measurement.h"

namespace nv::debugtoken {

using namespace nv::spdm::crypto;
// clang-format off
constexpr std::array<
    uint8_t, static_cast<uint8_t>(nv::debugtoken::HashSize::Mcu384Pubkey)> token_pubkey_der =
    (common::build::Mode == common::build::Modes::Dev)
        ? std::array<uint8_t, static_cast<uint8_t>(nv::debugtoken::HashSize::Mcu384Pubkey)>{
        0x4b, 0xb4, 0x4f, 0x73, 0x20, 0x04, 0x56, 0x52, 0x23, 0x56, 0x89, 0x5d, 0xb6, 0x5a,
        0xce, 0x06, 0x1d, 0x6a, 0xc7, 0xeb, 0x26, 0xf1, 0xd7, 0xc1, 0x44, 0x25, 0xde, 0xbc,
        0x87, 0xc7, 0x28, 0xaa, 0x64, 0xb6, 0xc4, 0x6d, 0x18, 0x17, 0xca, 0x8c, 0xd8, 0xfd,
        0xeb, 0xd8, 0xd3, 0x59, 0x45, 0x71, 0xe5, 0x24, 0xd4, 0x8d, 0xd5, 0x80, 0x19, 0x9f,
        0x45, 0x3a, 0x29, 0x8f, 0x71, 0xfc, 0x09, 0xad, 0xa9, 0xf4, 0xb4, 0xf0, 0x54, 0x8b,
        0x58, 0x21, 0x4d, 0x38, 0x63, 0xb8, 0x8d, 0xe2, 0x12, 0x84, 0xa7, 0xd4, 0xf4, 0x2f,
        0x25, 0x65, 0x44, 0x8f, 0x7c, 0xd4, 0xc0, 0xa1, 0x60, 0x9f, 0x0d, 0x6e}
        : std::array<uint8_t, static_cast<uint8_t>(nv::debugtoken::HashSize::Mcu384Pubkey)>{
        0x1b, 0xf8, 0xa8, 0x13, 0x99, 0x5f, 0x8a, 0x41, 0x11, 0xf6, 0xa2, 0x95, 0x47, 0xc4,
        0x89, 0xff, 0x34, 0x0e, 0x1b, 0x8c, 0xcd, 0xfb, 0x56, 0xef, 0x20, 0x33, 0xb1, 0x4a,
        0x57, 0xb8, 0x08, 0x4b, 0xef, 0xea, 0x22, 0xb0, 0xfb, 0x87, 0x04, 0x01, 0x2a, 0x13,
        0x42, 0xec, 0x09, 0x85, 0xe5, 0xf6, 0xae, 0xc6, 0x1e, 0xa1, 0xf6, 0xfb, 0xc4, 0xba,
        0x59, 0xc8, 0x83, 0x69, 0xbc, 0xf6, 0x3a, 0x78, 0x62, 0xa2, 0x41, 0x1e, 0x95, 0x84,
        0x9f, 0x2c, 0x47, 0xdb, 0xaf, 0x6c, 0x90, 0x5c, 0x5c, 0x61, 0xce, 0x34, 0xdc, 0x69,
        0xb3, 0xa1, 0xd3, 0x74, 0xae, 0x3f, 0xed, 0x55, 0x80, 0xe3, 0x9a, 0x05};
// clang-format on

namespace {

// Helper function to copy data from buffer and advance offset
// @param src Source buffer to read from
// @param offset Current offset in buffer, will be updated on success
// @param dest Destination object to copy into
// @return true if successful, false if overflow would occur
template<typename T>
[[nodiscard]] bool
copy_and_advance(const std::span<const uint8_t>& src, uint32_t& offset, T& dest)
{
    // Check for overflow before reading
    if (offset > UINT32_MAX - sizeof(T)) {
        return false;
    }
    memcpy(&dest, src.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

// Helper function to read TLV header and skip entire TLV entry (header + data)
// @param src Source buffer to read from
// @param offset Current offset in buffer, will be updated to point after entire TLV
// @param tlv_data TLV header structure to populate
// @return true if successful, false if overflow would occur
[[nodiscard]] bool
read_and_skip_tlv(const std::span<const uint8_t>& src, uint32_t& offset, TlvData& tlv_data)
{
    // Read TLV header
    if (!copy_and_advance(src, offset, tlv_data)) {
        return false;  // Overflow in header read
    }

    // Check for overflow before skipping data portion
    if (offset > UINT32_MAX - tlv_data.length) {
        return false;
    }
    offset += tlv_data.length;
    return true;
}

namespace {
// Log failure with throttling
void log_failure_throttled(logger::EventStructItem event, uint8_t error_code)
{
    nv::flash::Data fail_count = 0;
    (void)nv::flash::Flash::get_data(nv::flash::Key::NpdsDebugTokenFailCount, fail_count);

    if (fail_count < nv::debugtoken::MaxLogFailures) {
        logger::info(event, {error_code});
    }

    if (fail_count < UINT32_MAX) {
        (void)nv::flash::Flash::set_data(nv::flash::Key::NpdsDebugTokenFailCount,
                                         fail_count + 1);
    }
}

// Reset failure counter on success
void reset_log_throttle()
{
    (void)nv::flash::Flash::set_data(nv::flash::Key::NpdsDebugTokenFailCount, 0);
}
}  // anonymous namespace

// TLV index cache structure
struct TlvIndexCache
{
    std::array<TlvEntry, MaxTlvEntries> entries{};
    uint32_t                            count = 0;
    bool                                valid = false;
};

// Build index of all TLV entries in the token (scan once)
bool build_tlv_index(const std::span<const uint8_t>& token_span, TlvIndexCache& cache)
{
    cache.count = 0;
    cache.valid = false;

    if (token_span.empty()) {
        return false;
    }

    // Read the header
    TlvHeader tlv_header{};
    memcpy(&tlv_header, token_span.data(), sizeof(TlvHeader));

    if (tlv_header.identifier != TlvMagicNumber) {
        return false;
    }

    const uint32_t header_size   = sizeof(TlvHeader);
    const uint32_t tlv_data_size = tlv_header.size;

    // Check for overflow in addition
    if (tlv_data_size > UINT32_MAX - header_size) {
        return false;  // Overflow would occur
    }

    uint32_t       offset     = header_size;
    const uint32_t end_offset = header_size + tlv_data_size;

    // Scan all TLV entries and build index
    while (offset < end_offset && cache.count < MaxTlvEntries) {
        // Check if we have enough space for TLV header
        if (offset + TlvHeaderSize > token_span.size()) {
            break;
        }

        // Read TLV header and skip to next entry
        TlvData        tlv_data{};
        const uint32_t entry_start = offset;  // Save offset before advancing

        // Read header and skip entire TLV entry
        if (!read_and_skip_tlv(token_span, offset, tlv_data)) {
            break;  // Overflow would occur
        }

        // Check if entry exceeds token boundaries
        if (offset > end_offset) {
            break;
        }

        // Store entry in cache
        cache.entries.at(cache.count).type        = static_cast<uint16_t>(tlv_data.type);
        cache.entries.at(cache.count).length      = tlv_data.length;
        cache.entries.at(cache.count).data_offset = entry_start + TlvHeaderSize;
        cache.count++;
    }

    cache.valid = (cache.count > 0);
    return cache.valid;
}

// Find TLV entry from cache
bool find_tlv_in_cache(const TlvIndexCache& cache, TlvType target_type, TlvEntry& entry)
{
    if (!cache.valid) {
        return false;
    }

    for (uint32_t i = 0; i < cache.count; i++) {
        if (cache.entries.at(i).type == static_cast<uint16_t>(target_type)) {
            entry = cache.entries.at(i);
            return true;
        }
    }

    return false;
}

// Helper function to find specific TLV entry by type
bool find_tlv_entry(const std::span<const uint8_t>& token_span,
                    TlvType                         target_type,
                    TlvEntry&                       entry)
{
    // For single lookups, build temporary cache
    TlvIndexCache cache{};
    if (!build_tlv_index(token_span, cache)) {
        return false;
    }

    return find_tlv_in_cache(cache, target_type, entry);
}
}  // anonymous namespace

// Generic TLV reading function with unified error handling
TokenErrorCode read_tlv_field(const std::span<const uint8_t>& token_span,
                              TlvType                         target_type,
                              std::span<uint8_t>&             output_buffer)
{
    if (token_span.empty() || output_buffer.empty()) {
        return TokenErrorCode::TokenInternalError;
    }

    TlvEntry entry{};
    if (!find_tlv_entry(token_span, target_type, entry)) {
        return TokenErrorCode::TokenInternalError;  // Required TLV not found
    }

    if (entry.length != output_buffer.size()) {
        return TokenErrorCode::TokenInvalidFormat;  // Length mismatch
    }

    memcpy(output_buffer.data(), token_span.data() + entry.data_offset, entry.length);
    return TokenErrorCode::NoErrorCode;
}

// Specialized function for TLV validation
TokenErrorCode validate_tlv_field(const std::span<const uint8_t>& token_span,
                                  TlvType                         target_type,
                                  const std::span<const uint8_t>& expected_data)
{
    if (token_span.empty() || expected_data.empty()) {
        return TokenErrorCode::TokenInternalError;
    }

    TlvEntry entry{};
    if (!find_tlv_entry(token_span, target_type, entry)) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    if (entry.length != expected_data.size()) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    if (memcmp(token_span.data() + entry.data_offset, expected_data.data(), entry.length)
        != 0) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    return TokenErrorCode::NoErrorCode;
}

TokenErrorCode auth_token_tlv(
    const std::span<const uint8_t>& token_payload,
    const std::array<uint8_t, static_cast<uint8_t>(debugtoken::HashSize::Mcu384Pubkey)>& pubkey)
{
    TlvHeader header{};
    if (token_payload.size() >= sizeof(TlvHeader)) {
        memcpy(&header, token_payload.data(), sizeof(TlvHeader));
    }

    // Use local hash buffer (48 bytes is reasonable for stack)
    std::array<uint8_t, Sha384HashSize> hash{};  // Initialized to zeros

    // Scan all TLVs and collect signature TLV positions
    // Then calculate hash over everything except signature TLVs
    const uint32_t header_size   = sizeof(TlvHeader);
    const uint32_t tlv_data_size = header.size;

    // Use local TLV list (384 bytes is acceptable for stack)
    std::array<TlvInfo, MaxTlvEntries> tlv_list{};  // Initialized to zeros
    uint32_t                           tlv_count           = 0;
    uint32_t                           signature_tlv_index = UINT32_MAX;

    // Validate TLV data section bounds before creating span
    // Check for overflow in size calculation
    if (tlv_data_size > UINT32_MAX - header_size) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    // Check if TLV data fits within token payload
    if (header_size + tlv_data_size > token_payload.size()) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    // Create span for TLV data section (excluding header)
    const std::span<const uint8_t> tlv_data_span{token_payload.data() + header_size,
                                                 tlv_data_size};

    // First pass: collect all TLV positions
    uint32_t current_offset = 0;  // Offset from TLV data start
    while (current_offset < tlv_data_size && tlv_count < MaxTlvEntries) {
        // Safe bounds checking using offset arithmetic
        if (current_offset + TlvHeaderSize > tlv_data_size) {
            break;
        }

        // Save entry start before advancing
        const uint32_t entry_start = current_offset;

        // Read TLV header and skip to next entry
        TlvData tlv_data{};
        if (!read_and_skip_tlv(tlv_data_span, current_offset, tlv_data)) {
            break;  // Overflow would occur
        }

        // Validate offset hasn't exceeded data size
        if (current_offset > tlv_data_size) {
            return TokenErrorCode::TokenInvalidFormat;
        }

        // Store TLV information (convert to absolute offset from token start)
        tlv_list.at(tlv_count).start_offset = header_size + entry_start;
        tlv_list.at(tlv_count).type         = static_cast<uint16_t>(tlv_data.type);
        tlv_list.at(tlv_count).length       = tlv_data.length;

        if (tlv_data.type == TlvType::NvidiaSignature) {
            if (signature_tlv_index == UINT32_MAX) {
                signature_tlv_index = tlv_count;
            }
            else {
                return TokenErrorCode::TokenInvalidFormat;
            }
        }

        tlv_count++;
    }

    if (signature_tlv_index == UINT32_MAX) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Calculate hash of the token payload excluding the signature using incremental hashing
    constexpr int          UsingSha384 = 1;
    int                    rc          = 0;
    mbedtls_sha512_context ctx;

    // Initialize hash context
    rc = mbedtls_sha512_starts_ret(&ctx, UsingSha384);
    if (rc != 0) {
        return TokenErrorCode::TokenHashVerificationFailed;
    }

    // Hash TLV header first
    rc = mbedtls_sha512_update_ret(&ctx, token_payload.data(), sizeof(TlvHeader));
    if (rc != 0) {
        return TokenErrorCode::TokenHashVerificationFailed;
    }

    // Hash all TLVs except signature TLVs (type 0x0B) directly from original data
    for (uint32_t i = 0; i < tlv_count; i++) {
        if (signature_tlv_index != UINT32_MAX && i == signature_tlv_index) {
            continue;  // Skip signature TLV (0x0B)
        }

        const uint32_t tlv_total_size = TlvHeaderSize + tlv_list.at(i).length;  // TLV header +
                                                                                // data

        // Hash this TLV directly from the original token data
        rc = mbedtls_sha512_update_ret(
            &ctx, token_payload.data() + tlv_list.at(i).start_offset, tlv_total_size);
        if (rc != 0) {
            return TokenErrorCode::TokenHashVerificationFailed;
        }
    }

    // Finalize hash calculation
    rc                         = mbedtls_sha512_finish_ret(&ctx, hash.data());
    CryptoStatus crypto_status = (rc == 0) ? CryptoStatus::Success : CryptoStatus::FailHashCalc;

    if (crypto_status != CryptoStatus::Success) {
        return TokenErrorCode::TokenHashVerificationFailed;
    }

    // Additional bounds check for signature_tlv_index
    if (signature_tlv_index >= tlv_count) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    const TlvInfo& sig_tlv = tlv_list.at(signature_tlv_index);

    if (sig_tlv.length != static_cast<uint16_t>(debugtoken::HashSize::Mcu384Signature)) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // CERT ARR36-C compliant bounds checking using pure offset arithmetic
    if (header.size > ReasonableSize) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    const uint32_t buffer_total_size = sizeof(TlvHeader) + header.size;
    const uint32_t sig_tlv_offset    = sig_tlv.start_offset;

    // Check if signature TLV offset is within buffer bounds
    if (sig_tlv_offset >= buffer_total_size) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Check if complete signature TLV (header + data) fits within buffer
    const uint32_t sig_tlv_end_offset = sig_tlv_offset + TlvHeaderSize + sig_tlv.length;
    if (sig_tlv_end_offset > buffer_total_size) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Use signature data directly from token buffer
    const uint32_t signature_offset = sig_tlv_offset + TlvHeaderSize;

    // Create spans - use constructor for compatibility
    std::span<const uint8_t> r_signature{token_payload.data() + signature_offset,
                                         static_cast<size_t>(debugtoken::HashSize::Ecdsa384R)};

    std::span<const uint8_t> s_signature{
        token_payload.data() + signature_offset
            + static_cast<size_t>(debugtoken::HashSize::Ecdsa384R),
        static_cast<size_t>(debugtoken::HashSize::Ecdsa384L)};

    // Verify signature using SPDM crypto with proper namespace
    crypto_status = spdm::crypto::spdm_ecdsa_verify(pubkey, r_signature, s_signature, hash);

    if (crypto_status != CryptoStatus::Success) {
        return TokenErrorCode::TokenSignatureVerificationFailed;
    }

    return TokenErrorCode::NoErrorCode;
}

TokenErrorCode check_debug_token_type_enabled(Type token_type)
{
    // Check NPDS cache first
    // Value 0 = not authenticated, non-zero = authenticated (value is token_type_bitmap)
    nv::flash::Data cached_token_type_bitmap = 0;
    const auto      npds_status              = nv::flash::Flash::get_data(
        nv::flash::Key::NpdsDebugTokenAuthenticated, cached_token_type_bitmap);

    // If already authenticated this boot, check cached token type
    if (npds_status == nv::flash::Status::Ok && cached_token_type_bitmap != 0) {
        // If specific token_type requested, verify it matches cached type
        if (token_type != Type::Invalid) {
            const auto token_bitmask = static_cast<uint32_t>(token_type);
            if ((cached_token_type_bitmap & token_bitmask) == 0) {
                return TokenErrorCode::TokenUnsupportedType;
            }
        }
        // Token is authenticated and type is valid (NoErrorCode = success) so return success
        return TokenErrorCode::NoErrorCode;
    }

    // check if token is installed in flash
    if (!is_dbg_token_tlv_in_flash()) {
        return TokenErrorCode::TokenNotInstalled;
    }

    // Read TLV token from flash
    std::array<uint8_t, ReasonableSize> token_buffer = {};

    // Read token from flash in chunks
    constexpr uint32_t FlashReadChunkSize = nv::flash::BufferSize;
    constexpr uint32_t NumChunks          = ReasonableSize / FlashReadChunkSize;

    for (uint32_t i = 0; i < NumChunks; i++) {
        const uint32_t offset     = i * FlashReadChunkSize;
        const uint32_t chunk_size = FlashReadChunkSize;

        auto flash_status = nv::flash::Flash::read(
            SpiOffset + offset, std::span<uint8_t>{token_buffer.data() + offset, chunk_size});

        if (flash_status != nv::flash::Status::Ok) {
            return TokenErrorCode::TokenStorageError;
        }
    }

    // Read TLV header for size validation
    TlvHeader header{};
    std::memcpy(&header, token_buffer.data(), sizeof(TlvHeader));

    // Verify TLV size doesn't exceed buffer
    if (header.size > ReasonableSize - sizeof(TlvHeader)) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Calculate total token size
    const uint32_t                 total_token_size = sizeof(TlvHeader) + header.size;
    const std::span<const uint8_t> token_span{token_buffer.data(), total_token_size};

    // Build TLV index once
    TlvIndexCache tlv_cache{};
    if (!build_tlv_index(token_span, tlv_cache)) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Extract and verify token type from TLV data (0x0009)
    uint32_t extracted_token_type = 0;
    TlvEntry entry{};
    if (find_tlv_in_cache(tlv_cache, TlvType::TokenType, entry)) {
        if (entry.length != sizeof(uint32_t)) {
            return TokenErrorCode::TokenInvalidFormat;
        }

        memcpy(&extracted_token_type, token_span.data() + entry.data_offset, entry.length);

        // Verify extracted token type is valid
        if ((extracted_token_type & DebugOptionsFlags) == 0) {
            return TokenErrorCode::TokenUnsupportedType;
        }

        // If specific token_type requested
        if (token_type != Type::Invalid) {
            const auto token_bitmask = static_cast<uint32_t>(token_type);
            // Check if requested type is present in the token
            if ((extracted_token_type & token_bitmask) == 0) {
                return TokenErrorCode::TokenUnsupportedType;
            }
        }
    }
    // If token type TLV not found, continue with authentication

    // Authenticate token using the public key
    const TokenErrorCode auth_result = auth_token_tlv(token_span, token_pubkey_der);

    if (auth_result != TokenErrorCode::NoErrorCode) {
        return auth_result;
    }

    (void)nv::flash::Flash::set_data(nv::flash::Key::NpdsDebugTokenAuthenticated,
                                     extracted_token_type);

    return TokenErrorCode::NoErrorCode;
}

TokenErrorCode install_dbg_token_tlv(const std::span<const uint8_t>& token_span)
{
    if (token_span.empty()) {
        return TokenErrorCode::TokenInternalError;
    }

    // Verify TLV header first
    TlvHeader tlv_header{};
    memcpy(&tlv_header, token_span.data(), sizeof(TlvHeader));

    if (tlv_header.identifier != TlvMagicNumber) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Verify TLV size is reasonable
    if (tlv_header.size > ReasonableSize - sizeof(TlvHeader)) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Check if total token size exceeds flash sector size
    const uint32_t total_token_size = sizeof(TlvHeader) + tlv_header.size;
    if (total_token_size > FlashSectorSize) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Build TLV index once for all lookups
    TlvIndexCache tlv_cache{};
    if (!build_tlv_index(token_span, tlv_cache)) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Extract token fields first to determine the correct public key
    std::array<uint8_t, DT_MCU_FW_VER_SIZE>  mcufw_version        = {};
    std::array<uint8_t, DT_DEV_SER_NUM_SIZE> serial_num           = {};
    std::array<uint8_t, DT_NONCE_SIZE>       nonce                = {};
    uint16_t                                 agent_ver            = 0;
    uint32_t                                 extracted_token_type = 0;

    // Parse TLV data to extract these fields using cached index
    TlvEntry entry{};

    // Validate "MCDT" using header constant (CERT STR30-C compliant)
    const std::span<const uint8_t> expected_magic{
        static_cast<const uint8_t*>(static_cast<const void*>(&TokenIdentifierMagic)),
        static_cast<size_t>(TlvTokenIdLength)};
    if (!find_tlv_in_cache(tlv_cache, TlvType::TokenIdentifier, entry)
        || entry.length != expected_magic.size()) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    if (memcmp(token_span.data() + entry.data_offset, expected_magic.data(), entry.length)
        != 0) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Read nonce (16 bytes)
    if (!find_tlv_in_cache(tlv_cache, TlvType::ChallengeNonce, entry)
        || entry.length != nonce.size()) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    memcpy(nonce.data(), token_span.data() + entry.data_offset, entry.length);

    // Read device serial number
    if (!find_tlv_in_cache(tlv_cache, TlvType::DeviceSerialNumber, entry)
        || entry.length != serial_num.size()) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    memcpy(serial_num.data(), token_span.data() + entry.data_offset, entry.length);

    // Read firmware version (4 bytes)
    if (!find_tlv_in_cache(tlv_cache, TlvType::FirmwareVersion, entry)
        || entry.length != mcufw_version.size()) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    memcpy(mcufw_version.data(), token_span.data() + entry.data_offset, entry.length);

    // Read agent version (2 bytes)
    if (!find_tlv_in_cache(tlv_cache, TlvType::AgentVersion, entry)
        || entry.length != sizeof(agent_ver)) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    memcpy(&agent_ver, token_span.data() + entry.data_offset, entry.length);

    // Read token type (4 bytes)
    if (!find_tlv_in_cache(tlv_cache, TlvType::TokenType, entry)
        || entry.length != sizeof(extracted_token_type)) {
        return TokenErrorCode::TokenInvalidFormat;
    }
    memcpy(&extracted_token_type, token_span.data() + entry.data_offset, entry.length);

    // Validate extracted token type (supports bitmask combinations)
    if (extracted_token_type == 0 || (extracted_token_type & ~DebugOptionsFlags) != 0) {
        return TokenErrorCode::TokenUnsupportedType;
    }

    // Authenticate token using dev/prod public key
    const uint32_t token_size = sizeof(TlvHeader) + tlv_header.size;
    if (token_size > ReasonableSize) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Use the same public key for both token types (dev/prod mode dependent)
    const std::span<const uint8_t> token_subspan{token_span.data(),
                                                 static_cast<size_t>(token_size)};
    const TokenErrorCode auth_result = auth_token_tlv(token_subspan, token_pubkey_der);

    if (auth_result != TokenErrorCode::NoErrorCode) {
        // Log with throttling to prevent SPI wear out from repeated failures
        log_failure_throttled(logger::Event::DtAuthTokenFail,
                              static_cast<uint8_t>(auth_result));
        return auth_result;
    }

    // Verify debug token fields using SPDM measurement (TLV format)
    auto dt_status = spdm::measurement::verify_dbg_token_fields(
        mcufw_version,
        serial_num,
        nonce,
        spdm::measurement::MeasDebugTokenTlvConfiguration,
        tlv_header.version,
        agent_ver);

    // Only log if verification failed
    if (dt_status != spdm::measurement::DtStatusSuccess) {
        // Log with throttling to prevent SPI wear out from repeated failures
        log_failure_throttled(logger::Event::DtVerifyStatus, static_cast<uint8_t>(dt_status));
    }

    // Map verification result to TokenErrorCode
    switch (dt_status) {
        case spdm::measurement::DtStatusSuccess: break;

        // Format/size errors
        case spdm::measurement::DtStatusMcuFwVerSizeMismatch:
        case spdm::measurement::DtStatusDevSerNumSizeMismatch:
        case spdm::measurement::DtStatusNonceSizeMismatch:
        case spdm::measurement::DtStatusFailBadTokenVer:
            return TokenErrorCode::TokenInvalidFormat;

        // Version mismatch errors
        case spdm::measurement::DtStatusFailBadMcuFwVer:
        case spdm::measurement::DtStatusFailBadAgentVer:
            return TokenErrorCode::TokenFwVersionMismatch;

        // Field verification errors
        case spdm::measurement::DtStatusFailBadDevSerNum:
            return TokenErrorCode::TokenInvalidSerialNumber;
        case spdm::measurement::DtStatusFailBadNonce: return TokenErrorCode::TokenInvalidNonce;

        // Internal/other errors
        case spdm::measurement::DtStatusFailNonceValidityUpdate:
        case spdm::measurement::DtStatusMax:
        default                                                : return TokenErrorCode::TokenInternalError;
    }

    // Token auth passed. Invalidate the nonce and generate a new nonce to prevent replay of the
    // same token. Invalidate and generate a new nonce before install to mainly address the
    // timed reset attack after install
    if (spdm::measurement::gen_and_save_dbg_token_nonce() == false) {
        // Failed to program a new nonce. Return failure to prevent any
        // possibility of replay after token install
        return TokenErrorCode::TokenInternalError;
    }

    // Erase the token area in SPI flash
    auto flash_status = flash::Flash::erase(SpiOffset);
    if (flash_status != flash::Status::Ok) {
        return TokenErrorCode::TokenStorageError;
    }

    // Write complete token to flash in one operation
    if (token_size > ReasonableSize || token_size > token_span.size()) {
        return TokenErrorCode::TokenInvalidFormat;
    }

    // Write token to flash with 16-byte alignment
    constexpr uint32_t PhraseSize = nv::flash::PhraseSize;  // Use system-defined alignment

    // Calculate aligned size and remaining bytes
    const uint32_t aligned_size = (token_size / PhraseSize) * PhraseSize;
    // Prevent unsigned integer wrap by ensuring aligned_size <= token_size
    const uint32_t remaining_bytes = (aligned_size <= token_size) ? (token_size - aligned_size)
                                                                  : 0;

    // First write: aligned portion (if any)
    if (aligned_size > 0) {
        const std::span<const uint8_t> aligned_chunk{token_span.data(),
                                                     static_cast<size_t>(aligned_size)};
        flash_status = flash::Flash::write(SpiOffset, aligned_chunk);
        if (flash_status != flash::Status::Ok) {
            (void)flash::Flash::erase(SpiOffset);
            return TokenErrorCode::TokenStorageError;
        }
    }

    // Second write: remaining bytes with padding (if any)
    if (remaining_bytes > 0) {
        std::array<uint8_t, PhraseSize> padded_buffer = {};  // Zero-initialized
        memcpy(padded_buffer.data(), token_span.data() + aligned_size, remaining_bytes);

        const std::span<const uint8_t> padded_chunk{padded_buffer.data(),
                                                    static_cast<size_t>(PhraseSize)};
        flash_status = flash::Flash::write(SpiOffset + aligned_size, padded_chunk);
        if (flash_status != flash::Status::Ok) {
            (void)flash::Flash::erase(SpiOffset);
            return TokenErrorCode::TokenStorageError;
        }
    }

    logger::info(logger::Event::DtInstallSuccess);

    // Installation succeeded - reset failure counter
    reset_log_throttle();

    (void)nv::flash::Flash::set_data(nv::flash::Key::NpdsDebugTokenAuthenticated,
                                     extracted_token_type);

    return TokenErrorCode::NoErrorCode;
}

TokenErrorCode erase_installed_dbg_token_tlv()
{
    // Check if TLV token exists in flash before erasing
    if (is_dbg_token_tlv_in_flash()) {
        auto flash_status = flash::Flash::erase(SpiOffset);

        if (flash_status != flash::Status::Ok) {
            return TokenErrorCode::TokenStorageError;
        }
    }

    logger::info(logger::Event::DtEraseSuccess);

    // Clear NPDS cache since token is erased
    (void)nv::flash::Flash::set_data(nv::flash::Key::NpdsDebugTokenAuthenticated, 0);

    // Reset failure counter to allow fresh attempts
    reset_log_throttle();

    return TokenErrorCode::NoErrorCode;
}

bool is_dbg_token_tlv_in_flash()
{
    std::array<uint8_t, 64> tmp                = {};
    uint32_t                expected_magic_num = 0;
    auto                    flash_status       = flash::Flash::read(SpiOffset, tmp);

    if (flash_status != flash::Status::Ok) {
        return false;
    }

    // First check for TLV magic number ("TLV1" in ASCII)
    for (int i = 4; i > 0; i--) {
        expected_magic_num <<= 8;
        expected_magic_num  |= tmp.at(i - 1);
    }

    if (expected_magic_num != TlvMagicNumber) {
        return false;  // not a valid TLV format
    }

    // Then check for debug token identifier ("MCDT" in ASCII)
    const std::span<const uint8_t> tmp_span{tmp.data(), static_cast<size_t>(tmp.size())};
    const std::span<const uint8_t> expected_magic_check{
        static_cast<const uint8_t*>(static_cast<const void*>(&TokenIdentifierMagic)),
        static_cast<size_t>(TlvTokenIdLength)};
    const TokenErrorCode result = validate_tlv_field(
        tmp_span, TlvType::TokenIdentifier, expected_magic_check);
    return (result == TokenErrorCode::NoErrorCode);
}

}  // namespace nv::debugtoken
