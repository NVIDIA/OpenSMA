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

#include "nv/debugtoken/debugtoken.h"
#include "nv/flash/flash.h"
#include "nv/mctp/nsm.h"

using namespace nv;
using namespace mctp;

namespace {
using namespace debugtoken;  // Access ReasonableSize, MaxTlvEntries, and debug token constants

// Convert object to byte span
template<typename T>
inline std::span<const uint8_t> to_bytes(const T& obj)
{
    auto byte_span = std::as_bytes(std::span{&obj, 1});
    return std::span<const uint8_t>{
        static_cast<const uint8_t*>(static_cast<const void*>(byte_span.data())),
        byte_span.size()};
}

// Helper function to copy data to buffer and advance offset
// Returns true on success, false if buffer overflow would occur
// @param buffer Destination buffer
// @param src Source data span to copy
// @param offset Current offset in buffer, will be updated on success
[[nodiscard]] static bool
copy_and_advance(std::span<uint8_t> buffer, std::span<const uint8_t> src, uint32_t& offset)
{
    const size_t size = src.size();
    // Check for overflow protection: ensure offset + size won't wrap and fits in buffer
    if (offset <= buffer.size() && size <= buffer.size() - offset) {
        memcpy(buffer.data() + offset, src.data(), size);
        // coverity[cert_int30_c_violation] - overflow protected by check above
        offset += static_cast<uint32_t>(size);
        return true;
    }
    return false;
}

// get SKU information based on key revocation status
static uint8_t get_sku_information()
{
    // Read key revocation status from OTP/CFPA
    uint32_t                key_revoke = 0;
    const nv::flash::Status status     = nv::flash::Flash::read_key_revoke(
        key_revoke, nv::flash::KeyRollbackSelect::Mcu);

    if (status == nv::flash::Status::Ok) {
        // Check if debug key (key0) is revoked
        const bool debug_key_revoked = (key_revoke & 0x1) != 0;

        if (debug_key_revoked) {
            return debugtoken::McuProdMode;  // 0x2 - Production SKU
        }
        else {
            return debugtoken::McuDebugMode;  // 0x1 - Debug SKU
        }
    }
    else {
        // If we can't read OTP, default to debug mode
        return debugtoken::McuDebugMode;
    }
}

// Helper function to get token types from installed token
static std::pair<uint32_t, uint32_t> getInstalledTokenTypes()
{
    uint32_t token_type_bitmask = 0;

    // Read TOKEN_TYPE TLV from installed token
    std::array<uint8_t, debugtoken::ReasonableSize> token_buffer       = {};
    constexpr uint32_t                              FlashReadChunkSize = nv::flash::BufferSize;
    constexpr uint32_t NumChunks    = debugtoken::ReasonableSize / FlashReadChunkSize;
    nv::flash::Status  flash_status = nv::flash::Status::Ok;

    for (uint32_t i = 0; i < NumChunks; i++) {
        const uint32_t offset     = i * FlashReadChunkSize;
        const uint32_t chunk_size = FlashReadChunkSize;
        flash_status              = nv::flash::Flash::read(debugtoken::SpiOffset + offset,
                                                           {token_buffer.data() + offset, chunk_size});

        if (flash_status != nv::flash::Status::Ok) {
            return {0, 0};  // Failed to read, assume no token types
        }
    }

    // Read TokenType TLV (0x0009) which contains the bitmask
    const std::span<const uint8_t> token_span(token_buffer.data(), token_buffer.size());
    std::span<uint8_t>             bitmask_span(
        static_cast<uint8_t*>(static_cast<void*>(&token_type_bitmask)),
        sizeof(token_type_bitmask));
    const debugtoken::TokenErrorCode tlv_result = debugtoken::read_tlv_field(
        token_span, debugtoken::TlvType::TokenType, bitmask_span);

    // Count number of known token types in bitmask
    uint32_t num_token_types = 0;
    if (tlv_result == debugtoken::TokenErrorCode::NoErrorCode) {
        // Check for token types based on bit positions
        if (token_type_bitmask & 0x01) {
            num_token_types++;  // FlashDebugFw (0x01)
        }
        if (token_type_bitmask & 0x02) {
            num_token_types++;  // OtpDumpEn (0x02)
        }
        if (token_type_bitmask & 0x04) {
            num_token_types++;  // RasDebug (0x04)
        }
    }

    return {num_token_types, token_type_bitmask};
}

static void map_bitpos_to_token(uint32_t  bit_pos,
                                uint32_t& token_type,
                                uint32_t& token_subtype,
                                bool&     valid)
{
    valid = true;
    switch (bit_pos) {
        case debugtoken::DebugTokenBitPosDebugFw:  // Debug FW (bit 0 = 0x1)
            token_type    = debugtoken::DebugTokenTypeDebugFw;
            token_subtype = debugtoken::DebugTokenSubtypeDebugFw;
            break;
        case debugtoken::DebugTokenBitPosOtpDump:  // OTP dump enable (bit 1 = 0x2)
            token_type    = debugtoken::DebugTokenTypeOtpDump;
            token_subtype = debugtoken::DebugTokenSubtypeOtpDump;
            break;
        case debugtoken::DebugTokenBitPosRasDebug:  // RAS Debug (bit 2 = 0x4)
            token_type    = debugtoken::DebugTokenTypeRasDebug;
            token_subtype = debugtoken::DebugTokenSubtypeRasDebug;
            break;
        default: valid = false; break;
    }
}

static uint32_t append_tlv_u8(std::span<uint8_t>  buffer,
                              uint32_t            offset,
                              debugtoken::TlvType type,
                              uint8_t             value)
{
    const uint32_t required_size = sizeof(debugtoken::TlvData) + sizeof(value);

    // Check if buffer has enough space with overflow protection
    if (offset > buffer.size() || required_size > buffer.size() - offset) {
        return offset;  // Return unchanged offset if not enough space
    }

    const uint32_t      original_offset = offset;
    debugtoken::TlvData tlv{};
    tlv.type   = type;
    tlv.length = 1;

    // Write TLV header
    if (!copy_and_advance(buffer, to_bytes(tlv), offset)) {
        return original_offset;  // Failed to write
    }

    // Write value
    if (!copy_and_advance(buffer, to_bytes(value), offset)) {
        return original_offset;  // Failed to write
    }

    return offset;
}

static void patch_tlv_header_total_size(std::span<uint8_t> tlv_buffer, uint32_t tlv_total_bytes)
{
    // Calculate the new size value
    uint32_t new_size = 0;
    if (tlv_total_bytes >= sizeof(debugtoken::TlvHeader)) {
        new_size = static_cast<uint32_t>(tlv_total_bytes - sizeof(debugtoken::TlvHeader));
    }
    else {
        new_size = 0;
    }

    // Ensure buffer is large enough for TlvHeader
    if (tlv_buffer.size() >= sizeof(debugtoken::TlvHeader)) {
        constexpr uint32_t size_offset = offsetof(debugtoken::TlvHeader, size);
        memcpy(tlv_buffer.data() + size_offset, &new_size, sizeof(new_size));
    }
}

}  // namespace

bool Nsm::process_debugtoken_diagnostics(const Packet& rx, Packet& tx)
{
    auto& nrx = NsmPktReq::from(rx);

    using type4_cmd = nv::mctp::NsmType4CmdCode;
    switch (nrx.get_type4_code()) {
        case type4_cmd::InstallToken  : on_install_token(rx, tx); return true;
        case type4_cmd::EraseToken    : on_erase_token(rx, tx); return true;
        case type4_cmd::QueryToken    : on_query_token(rx, tx); return true;
        case type4_cmd::QueryDeviceIds: on_query_device_ids(rx, tx); return true;
        default                       : break;
    }

    return false;
}

void Nsm::on_install_token(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx       = NsmPktResp::from(tx);
    auto& nrx       = NsmPktReq::from(rx);
    ntx.nv_msg_type = NsmMsgType::Diagnostics;
    ntx.set_type4_code(nrx.get_type4_code());

    // Get data size and data based on OCP version
    uint16_t       dataSize   = 0;
    const uint8_t* dataBuffer = nullptr;

    if (nrx.ocp_version >= 2) {
        // OCP Management IF Version 2 or higher - use NsmPktReqV2 format
        const auto& nrx_v2 = NsmPktReqV2::from(rx);
        dataSize           = nrx_v2.data_size;
        dataBuffer         = static_cast<const uint8_t*>(nrx_v2.data);
    }
    else {
        // OCP Management IF Version 1 - use NsmPktReq format
        dataSize   = static_cast<uint16_t>(nrx.data_size);
        dataBuffer = static_cast<const uint8_t*>(nrx.data);
    }

    if (dataSize == 0 || dataBuffer == nullptr) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Install the debug token using TLV version
    const std::span<const uint8_t>   token_span(dataBuffer, dataSize);
    const debugtoken::TokenErrorCode validation_result = debugtoken::install_dbg_token_tlv(
        token_span);

    if (validation_result == debugtoken::TokenErrorCode::NoErrorCode) {
        ntx.completion_code   = Ccode::Success;
        ntx.data_size         = 0;
        tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    }
    else {
        // Return error with TokenErrorCode in data payload
        ntx.completion_code         = Ccode::ErrorGeneral;
        const auto error_code_value = static_cast<uint16_t>(validation_result);
        memcpy(&ntx.data, &error_code_value, sizeof(error_code_value));
        ntx.data_size         = sizeof(error_code_value);
        tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + sizeof(error_code_value);
    }
}

void Nsm::on_erase_token(const Packet& rx, Packet& tx)
{
    auto& nrx = NsmPktReq::from(rx);

    // Get data size and data based on OCP version
    uint16_t       dataSize   = 0;
    const uint8_t* dataBuffer = nullptr;

    if (nrx.ocp_version >= 2) {
        // OCP Management IF Version 2 or higher - use NsmPktReqV2 format
        const auto& nrx_v2 = NsmPktReqV2::from(rx);
        dataSize           = nrx_v2.data_size;
        dataBuffer         = static_cast<const uint8_t*>(nrx_v2.data);
    }
    else {
        // OCP Management IF Version 1 - use NsmPktReq format
        dataSize   = static_cast<uint16_t>(nrx.data_size);
        dataBuffer = static_cast<const uint8_t*>(nrx.data);
    }

    // Extract token type from request data
    uint32_t token_type = 0;
    if (dataSize >= sizeof(token_type) && dataBuffer != nullptr) {
        memcpy(&token_type, dataBuffer, sizeof(token_type));
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Get installed token information at the beginning
    auto [num_types, installed_bitmask] = getInstalledTokenTypes();

    debugtoken::TokenErrorCode error_code = debugtoken::TokenErrorCode::NoErrorCode;

    if (token_type == ERASE_ALL_TOKENS) {
        // Check if any token is installed before erasing
        if (installed_bitmask == 0) {
            error_code = debugtoken::TokenErrorCode::TokenNotInstalled;
        }
        else {
            // Erase all installed tokens
            error_code = debugtoken::erase_installed_dbg_token_tlv();
        }
    }
    else if (token_type == ERASE_ALL_AND_INCREMENT) {
        // Check if any token is installed before erasing
        if (installed_bitmask == 0) {
            error_code = debugtoken::TokenErrorCode::TokenNotInstalled;
        }
        else {
            // Erase all installed tokens and increment ratchet counter
            error_code = debugtoken::erase_installed_dbg_token_tlv();

            if (error_code == debugtoken::TokenErrorCode::NoErrorCode) {
                // TODO: Implement ratchet counter increment functionality
                // This should increment the ratchet counter in OTP/persistent storage
            }
        }
    }
    else {
        // Erase specific token type
        // Note: erasing any individual token type will erase the entire token
        // Validate request: must contain only valid bits and at least one valid bit
        const bool has_invalid_bits = (token_type & ~debugtoken::DebugOptionsFlags) != 0;
        const bool has_valid_bits   = (token_type & debugtoken::DebugOptionsFlags) != 0;

        if (has_invalid_bits || !has_valid_bits) {
            error_code = debugtoken::TokenErrorCode::TokenUnsupportedType;
        }
        else {
            // Check if installed token matches any requested type
            const bool type_match = (installed_bitmask & token_type) != 0;
            if (type_match) {
                // Erase entire composite token
                error_code = debugtoken::erase_installed_dbg_token_tlv();
            }
            else {
                error_code = debugtoken::TokenErrorCode::TokenNotInstalled;
            }
        }
    }

    // Build response
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx       = NsmPktResp::from(tx);
    ntx.nv_msg_type = NsmMsgType::Diagnostics;
    ntx.set_type4_code(NsmType4CmdCode::EraseToken);

    if (error_code != debugtoken::TokenErrorCode::NoErrorCode) {
        // Return error with TokenErrorCode in data payload
        ntx.completion_code         = Ccode::ErrorGeneral;
        const auto error_code_value = static_cast<uint16_t>(error_code);
        memcpy(&ntx.data, &error_code_value, sizeof(error_code_value));
        ntx.data_size         = sizeof(error_code_value);
        tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + sizeof(error_code_value);
    }
    else {
        // Success response has no data payload
        ntx.completion_code   = Ccode::Success;
        ntx.data_size         = 0;
        tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    }
}

void Nsm::on_query_token(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx       = NsmPktResp::from(tx);
    ntx.nv_msg_type = NsmMsgType::Diagnostics;
    ntx.set_type4_code(NsmType4CmdCode::QueryToken);

    const bool token_installed = debugtoken::is_dbg_token_tlv_in_flash();

    // Get token type information once
    uint32_t token_type_bitmask = 0;
    uint32_t num_token_types    = 0;

    if (token_installed) {
        auto [types_count, bitmask] = getInstalledTokenTypes();
        num_token_types             = types_count;
        token_type_bitmask          = bitmask;
    }

    // Validate token types count
    if (num_token_types > debugtoken::MaxTokenTypes) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    const uint32_t token_type_subtype_length = num_token_types * 8;  // 8 bytes per entry

    // Calculate actual TLV data size needed - COMPLETE VERSION
    const uint32_t tlv_header_size        = sizeof(debugtoken::TlvHeader);
    const uint32_t install_status_size    = sizeof(debugtoken::TlvData) + 1;
    const uint32_t processing_status_size = sizeof(debugtoken::TlvData) + 1;
    const uint32_t sku_info_size          = sizeof(debugtoken::TlvData) + 1;
    const uint32_t token_list_size = sizeof(debugtoken::TlvData) + token_type_subtype_length;

    const uint32_t tlv_data_size_calc = tlv_header_size + install_status_size
                                      + processing_status_size + sku_info_size
                                      + token_list_size;
    uint16_t tlv_data_size = 0;
    if (tlv_data_size_calc <= UINT16_MAX) {
        tlv_data_size = static_cast<uint16_t>(tlv_data_size_calc);
    }
    else {
        tlv_data_size = UINT16_MAX;  // Cap at maximum value
    }

    // Calculate maximum response size from buffer size
    // coverity[cert_int31_c_violation] - NsmPktBufDataLen is constexpr 256, fits in uint16_t
    constexpr uint16_t MAX_NSM_RESPONSE_SIZE = static_cast<uint16_t>(NsmPktBufDataLen)
                                             - sizeof(Header) - HeaderResponseSize;
    const uint16_t total_response_size = tlv_data_size;

    if (total_response_size > MAX_NSM_RESPONSE_SIZE) {
        // Response too large, return error
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Build TLV response using span and offset approach
    const std::span<uint8_t> response_buffer(static_cast<uint8_t*>(ntx.data),
                                             MAX_NSM_RESPONSE_SIZE);
    uint32_t                 current_offset = 0;

    // TLV Header (Fixed)
    debugtoken::TlvHeader tlv_header = {};
    tlv_header.identifier            = debugtoken::TlvMagicNumber;
    tlv_header.version               = debugtoken::TlvVersion;

    if (!copy_and_advance(response_buffer, to_bytes(tlv_header), current_offset)) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Install/Processing/SKU (1 byte each)
    current_offset = append_tlv_u8(response_buffer,
                                   current_offset,
                                   debugtoken::TlvType::InstallStatus,
                                   token_installed ? static_cast<uint8_t>(1)
                                                   : static_cast<uint8_t>(0));
    current_offset = append_tlv_u8(response_buffer,
                                   current_offset,
                                   debugtoken::TlvType::ProcessingStatus,
                                   token_installed ? static_cast<uint8_t>(1)
                                                   : static_cast<uint8_t>(0));

    // SKU Information - check if debug key is revoked
    const uint8_t sku_value = get_sku_information();
    current_offset          = append_tlv_u8(
        response_buffer, current_offset, debugtoken::TlvType::SkuInformation, sku_value);

    // Token type list (0x0016) - Only write if we have tokens
    uint16_t list_len = 0;

    if (num_token_types > 0) {
        list_len = static_cast<uint16_t>(num_token_types * 8);  // 8 bytes per token

        // Write TLV header
        debugtoken::TlvData list_hdr{};
        list_hdr.type   = debugtoken::TlvType::TokenTypeSubtypeList;
        list_hdr.length = list_len;

        if (!copy_and_advance(response_buffer, to_bytes(list_hdr), current_offset)) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }

        // Write tokens using map_bitpos_to_token for correct values
        for (uint32_t bit_pos = 0; bit_pos < 32; bit_pos++) {
            if (token_type_bitmask & (1U << bit_pos)) {
                uint32_t token_type    = 0;
                uint32_t token_subtype = 0;
                bool     valid         = false;

                map_bitpos_to_token(bit_pos, token_type, token_subtype, valid);

                if (valid) {
                    if (!copy_and_advance(response_buffer, to_bytes(token_type), current_offset)
                        || !copy_and_advance(
                            response_buffer, to_bytes(token_subtype), current_offset)) {
                        // Buffer overflow would occur
                        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                        return;
                    }
                }
            }
        }
    }
    else {
        // No tokens - write empty token type list
        debugtoken::TlvData list_hdr{};
        list_hdr.type   = debugtoken::TlvType::TokenTypeSubtypeList;
        list_hdr.length = 0;  // Empty list

        if (!copy_and_advance(response_buffer, to_bytes(list_hdr), current_offset)) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
    }

    // Set final response size using offset
    const uint32_t tlv_data_bytes = current_offset;

    // Patch TLV header size to the actual value now that we know final bytes
    patch_tlv_header_total_size(response_buffer, tlv_data_bytes);

    // Check for overflow in packet length calculation (CERT INT31-C compliance)
    constexpr uint16_t header_total = sizeof(Header) + HeaderResponseSize;
    if (tlv_data_bytes > UINT16_MAX - header_total) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Safe to cast after bounds check above
    const auto actual_response_size = static_cast<uint16_t>(tlv_data_bytes);
    tx.priv.packet_length           = header_total + actual_response_size;

    ntx.completion_code = Ccode::Success;
    ntx.data_size       = actual_response_size;
}

void Nsm::on_query_device_ids(const Packet& rx, Packet& tx)
{
    // Fill basic response header
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx       = NsmPktResp::from(tx);
    auto& nrx       = NsmPktReq::from(rx);
    ntx.nv_msg_type = NsmMsgType::Diagnostics;
    ntx.set_type4_code(nrx.get_type4_code());

    // Response data: 16 bytes device unique identifier
    constexpr uint16_t SERIAL_NUMBER_SIZE = debugtoken::TlvSerialLength;

    // Get device serial number using span approach
    const std::span<uint8_t> response_buffer(static_cast<uint8_t*>(ntx.data),
                                             SERIAL_NUMBER_SIZE);

    // Get device UUID/serial number
    const auto& uuid = _ctl.router().ec.uuid;

    // UUID size is always 16 bytes, same as SERIAL_NUMBER_SIZE
    static_assert(sizeof(uuid) == SERIAL_NUMBER_SIZE,
                  "UUID size must match serial number size");

    // Copy UUID to response buffer
    memcpy(response_buffer.data(), &uuid, SERIAL_NUMBER_SIZE);

    // Set response parameters
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = SERIAL_NUMBER_SIZE;

    // Calculate packet length (safe to use uint16_t for this small size)
    const uint16_t calculated_length = sizeof(Header) + HeaderResponseSize + SERIAL_NUMBER_SIZE;
    tx.priv.packet_length            = calculated_length;
}
