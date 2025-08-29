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

#include "nv/debugtoken/debugtoken.h"

#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/sha512.h"

#include "nv/common/build.h"
#include "nv/logger/log.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/spdm/spdm_get_measurement.h"

namespace nv::debugtoken {

using namespace nv::spdm::crypto;

/// local copy of debug token payload.
alignas(8) NV_SHARED_BSS std::array<uint8_t, TOKEN_SIZE> token_payload = {};  // NOLINT
/// Key to authenticate signed token binary.
// clang-format off
alignas(8) NV_SHARED_DATA const std::array<
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

/**
 *  This function checks whether background copy has been disabled.
 *
 *  @return boolean true if BG copy is disabled, false otherwise.
 */
bool is_background_update_disabled()
{
    // TODO:: is_background_update_disabled
    return true;
}

/**
 *  This function authenticates debug token payload once during
 *  token installation and once every boot during token read from spi flash.
 *
 *  @param[in] token_payload     Token payload.
 *  @param[in] pubkey            Token public key.
 *
 *  @return Status::Success on success, error code otherwise.
 */
Status auth_token(
    std::array<uint8_t, TOKEN_SIZE>& token_payload,
    const std::array<uint8_t, static_cast<uint8_t>(debugtoken::HashSize::Mcu384Pubkey)>& pubkey)
{
    // Validate public key is not empty
    const std::array<uint8_t, static_cast<uint8_t>(debugtoken::HashSize::Mcu384Pubkey)>
        empty_key{};
    if (pubkey == empty_key) {
        return Status::InternalError;
    }

    static_assert(TOKEN_SIZE == PayloadSize, "token_payload size does not match PayloadSize");

    // Validate payload size
    if (PayloadSize != token_payload.size()) {
        return Status::SizeMismatch;
    }

    // Calculate hash of token payload (excluding signature)
    std::array<uint8_t, static_cast<uint8_t>(debugtoken::HashSize::Sha384)> hash = {};
    auto crypto_status = spdm::crypto::spdm_hash_data(
        hash.data(),
        token_payload.data(),
        PayloadSize - static_cast<unsigned int>(debugtoken::HashSize::Mcu384Signature));

    if (crypto_status != spdm::crypto::CryptoStatus::Success) {
        return Status::InternalError;
    }

    // Extract signature components
    const size_t   signature_offset = offsetof(Token, signature);
    const uint8_t* signature_ptr    = token_payload.data() + signature_offset;

    // Use const std::span for read-only data
    std::span<const uint8_t> r_signature(
        signature_ptr, static_cast<unsigned int>(debugtoken::HashSize::Ecdsa384R));

    std::span<const uint8_t> s_signature(
        signature_ptr + static_cast<unsigned int>(debugtoken::HashSize::Ecdsa384R),
        static_cast<unsigned int>(debugtoken::HashSize::Ecdsa384L));

    // Verify signature
    crypto_status = spdm::crypto::spdm_ecdsa_verify(pubkey, r_signature, s_signature, hash);

    if (crypto_status != spdm::crypto::CryptoStatus::Success) {
        return Status::AuthFailed;
    }

    return Status::Success;
}

/**
 * @brief Updates the installation state of the debugtoken.
 *
 * This function performs necessary updates to track the installation status
 * of a debug token on the system.
 *
 * @return debugtoken::Status::Success on success, error code otherwise.
 */
Status update_token_install_state()
{
    // TODO:: update_token_install_state
    // 1. The OTP bit is used to detect whether debug token was ever installed on the system.
    // 2. Update the installation count in PDS
    return Status::Success;
}

bool is_dbg_token_in_flash()
{
    std::array<uint8_t, 4> tmp                = {};
    uint32_t               expected_magic_num = 0;
    auto                   spi_status         = flash::Flash::read(SpiOffset, tmp);

    if (spi_status != flash::Status::Ok) {
        return false;
    }

    for (int i = 4; i > 0; i--) {
        expected_magic_num <<= 8;
        expected_magic_num  |= tmp.at(i - 1);
    }

    return (expected_magic_num == MagicNum);
}

Status load_and_auth_dbg_token()
{
    auto spi_status = flash::Flash::read(SpiOffset, token_payload);
    if (spi_status != flash::Status::Ok) {
        return Status::InternalError;
    }

    Token token = {};
    std::memcpy(&token, &token_payload, sizeof(Token));

    auto status = Status::AuthFailed;

    if (token.type == Type::FlashDebugFw) {
        status = auth_token(token_payload, token_pubkey_der);
    }

    if (status != Status::Success) {
        logger::info(logger::Event::DtAuthTokenFail, {static_cast<uint8_t>(status)});

        // Clear local buffer.
        memset(&token_payload, 0, sizeof(token_payload));
        // Ignoring error to report generic error code.
        flash::Flash::erase(SpiOffset);
    }

    return status;
}

Status install_dbg_token(std::span<const uint8_t, debugtoken::PayloadSize> token_buffer)
{
    if (token_buffer.empty()) {
        return Status::InternalError;
    }

    // Check whether BG copy has been disabled.
    // If not then do not install token and return error code.
    // It is a requirement in debug token workflow to have background copy
    // disabled to support better recovery after token erase/expiry by
    // having prod signed image in the secondary slot
    if (is_background_update_disabled() == false) {
        return Status::InstallFailedBgCheck;
    }

    // copy input to token_payload
    std::copy(token_buffer.begin(), token_buffer.end(), token_payload.begin());

    Token token = {};
    std::memcpy(&token, token_buffer.data(), sizeof(Token));

    auto status = Status::Success;

    // check if token_type bits are present in token.type
    if (static_cast<uint32_t>(token.type) & DebugOptionsFlags) {
        status = auth_token(token_payload, token_pubkey_der);
    }
    else {
        status = Status::UnsupportedTokenType;
    }

    if (status != Status::Success) {
        logger::info(logger::Event::DtAuthTokenFail, {static_cast<uint8_t>(status)});
        return status;
    }

    // extract the sections from token_payload into std::array objects
    const size_t mcufw_version_offset = offsetof(Token, mcufw_version);
    if (mcufw_version_offset + DT_MCU_FW_VER_SIZE > token_payload.size()) {
        return Status::SizeMismatch;
    }
    std::array<uint8_t, DT_MCU_FW_VER_SIZE> mcufw_version = {};
    std::copy(token_payload.data() + mcufw_version_offset,
              token_payload.data() + mcufw_version_offset + DT_MCU_FW_VER_SIZE,
              mcufw_version.begin());

    const size_t serial_num_offset = offsetof(Token, serial_num);
    if (serial_num_offset + DT_DEV_SER_NUM_SIZE > token_payload.size()) {
        return Status::SizeMismatch;
    }
    std::array<uint8_t, DT_DEV_SER_NUM_SIZE> serial_num = {};
    std::copy(token_payload.data() + serial_num_offset,
              token_payload.data() + serial_num_offset + DT_DEV_SER_NUM_SIZE,
              serial_num.begin());

    const size_t nonce_offset = offsetof(Token, nonce);
    if (nonce_offset + DT_NONCE_SIZE > token_payload.size()) {
        return Status::SizeMismatch;
    }
    std::array<uint8_t, DT_NONCE_SIZE> nonce = {};
    std::copy(token_payload.data() + nonce_offset,
              token_payload.data() + nonce_offset + DT_NONCE_SIZE,
              nonce.begin());

    auto dt_status = verify_dbg_token_fields(mcufw_version,
                                             serial_num,
                                             nonce,
                                             spdm::measurement::MeasDebugTokenConfiguration,
                                             token.version,
                                             token.agent_ver);

    logger::info(logger::Event::DtVerifyStatus, {static_cast<uint8_t>(dt_status)});

    // map error codes to debug token error codes
    switch (dt_status) {
        case spdm::measurement::DtStatusSuccess              : break;
        case spdm::measurement::DtStatusMcuFwVerSizeMismatch :
        case spdm::measurement::DtStatusDevSerNumSizeMismatch:
        case spdm::measurement::DtStatusNonceSizeMismatch    : return Status::SizeMismatch;
        case spdm::measurement::DtStatusFailBadMcuFwVer      : return Status::McufwVersionCheckFailed;
        case spdm::measurement::DtStatusFailBadDevSerNum     : return Status::SerialNumInvalid;
        case spdm::measurement::DtStatusFailBadNonce         : return Status::NonceCheckFailed;
        case spdm::measurement::DtStatusFailNonceValidityUpdate:
            return Status::NonceUpdateFailed;
        case spdm::measurement::DtStatusFailBadAgentVer: return Status::AgentVersionCheckFailed;
        default                                        : return Status::InternalError;
    }

    // Token auth passed. Invalidate the nonce
    // and generate a new nonce to prevent replay of the same token.
    // Invalidate and generate a new nonce before install to mainly address the timed
    // reset attack after install
    if (spdm::measurement::gen_and_save_dbg_token_nonce() == false) {
        // failed to program a new nonce. Return failure to prevent any
        // possibility of replay after token install
        return Status::NonceUpdateFailed;
    }

    // erase the token offset in spi flash
    auto spi_status = flash::Flash::erase(SpiOffset);
    if (spi_status != flash::Status::Ok) {
        return Status::InternalError;
    }

    // Write token to flash.
    constexpr size_t ChunkSize     = 256;  // Size of each chunk to write (256 bytes)
    constexpr size_t TotalSize     = PayloadSize;
    constexpr size_t RemainingSize = TotalSize - ChunkSize;
    constexpr size_t PaddingSize   = 8;

    // Write the first 256 bytes
    spi_status = flash::Flash::write(SpiOffset, token_buffer.subspan(0, ChunkSize));
    if (spi_status != flash::Status::Ok) {
        // Ignoring error to report generic error code.
        flash::Flash::erase(SpiOffset);
        return Status::InternalError;
    }

    // Handle remaining bytes and pad to 16-byte boundary (16-byte alignment)
    if (RemainingSize > 0) {
        std::array<uint8_t, 16> padded_buffer = {};  // 16 bytes total size (8 bytes data +
                                                     // 8 bytes padding)

        // Copy the remaining data into the padded buffer
        std::memcpy(padded_buffer.data(), token_buffer.data() + ChunkSize, RemainingSize);

        spi_status = flash::Flash::write(
            SpiOffset + ChunkSize,
            std::span<const uint8_t>(padded_buffer.data(), RemainingSize + PaddingSize));
        if (spi_status != flash::Status::Ok) {
            // Ignoring error to report generic error code.
            flash::Flash::erase(SpiOffset);
            return Status::InternalError;
        }
    }

    // Token auth passed and is successfully installed in spi flash
    // Update token installation otp bit
    status = update_token_install_state();
    if (status != Status::Success) {
        // Ignoring error to report generic error code.
        flash::Flash::erase(SpiOffset);
        return status;
    }

    logger::info(logger::Event::DtInstallSuccess);

    return Status::Success;
}

Status erase_installed_dbg_token()
{
    if (is_dbg_token_in_flash()) {
        auto spi_status = flash::Flash::erase(SpiOffset);

        if (spi_status != flash::Status::Ok) {
            return Status::InternalError;
        }
    }

    logger::info(logger::Event::DtEraseSuccess);

    return Status::Success;
}

Type get_token_type()
{
    Type token_type = Type::Invalid;

    if (!is_dbg_token_in_flash()) {
        return token_type;
    }

    const size_t                      type_offset = offsetof(Token, type);
    std::array<uint8_t, sizeof(Type)> type_buffer = {};

    auto spi_status = flash::Flash::read(SpiOffset + type_offset, type_buffer);
    if (spi_status != flash::Status::Ok) {
        return token_type;
    }

    std::memcpy(&token_type, type_buffer.data(), sizeof(Type));

    return token_type;
}

}  // namespace nv::debugtoken
