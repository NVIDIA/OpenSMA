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

#pragma once

#include <cstdint>
#include <span>

namespace nv {
namespace debugtoken {

/* Debugtoken */
constexpr uint8_t DT_NONCE_SIZE       = 16;
constexpr uint8_t DT_MCU_FW_VER_SIZE  = 4;
constexpr uint8_t DT_DEV_SER_NUM_SIZE = 16;
constexpr uint8_t DT_RESERVED_SIZE    = 20;

constexpr uint16_t DT_AGENT_VER          = 0x0001;
constexpr uint16_t MCU_ENDPOINT_ID       = 0x0005;
constexpr uint32_t EC_CHECK_TOKEN_VER    = 0x00010000;  // MAJOR:1 MINOR:0
constexpr uint32_t AGENT_CHECK_TOKEN_VER = 0x00010001;  // MAJOR:1 MINOR:1

constexpr uint8_t DBG_TOKEN_REQ_FILE_MINOR_VER = 2;
constexpr uint8_t DBG_TOKEN_REQ_FILE_MAJOR_VER = 1;

constexpr uint32_t DBG_TOKEN_NONCE_VALID   = 0xAA;
constexpr uint32_t DBG_TOKEN_NONCE_INVALID = 0xFF;

typedef struct [[gnu::packed]]
{
    uint8_t was_installed;   // Byte 0: Was a Debug token ever installed? Bit 1: Is Debug token
                             // currently installed?
    uint32_t install_count;  // Byte 1~4: Debug token installation counter little endian
} DebugTokenStatsT;

// Debug Token Request Structure
typedef struct [[gnu::packed]]
{
    uint8_t                                  minor_version;   // Byte 0
    uint8_t                                  major_version;   // Byte 1
    uint16_t                                 struct_size;     // Byte 2~3
    std::array<uint8_t, DT_MCU_FW_VER_SIZE>  mcu_fw_version;  // Byte 4~7
    std::array<uint8_t, DT_NONCE_SIZE>       nonce;           // Byte 8~23
    std::array<uint8_t, DT_DEV_SER_NUM_SIZE> serial_number;   // Byte 24~39
    uint16_t                                 end_point_id;    // Byte 40~41
    uint16_t                                 agent_version;   // Byte 42~43
    std::array<uint8_t, DT_RESERVED_SIZE>    reserved;        // Btye 44-63
} DebugTokenConfigT;

/// General integer constants
enum Constants
{
    /// Address in internal SPI flash where token gets installed.
    SpiOffset = 0xEE000,
    MagicNum  = 0x5444434D,

    DebugTokenStatsTSize  = 5,
    DebugTokenConfigTSize = 64,
    PayloadSize           = 264,

    McufwDebugMode       = 0x1,
    McufwProdMode        = 0x2,
    DbgTokenAgentVersion = 0x0001,
};

/// Type of token.
enum class Type : uint32_t
{
    Invalid      = 0x00,  ///< UNDEFINED
    FlashDebugFw = 0x01,  ///< Debug firmware
    OtpDumpEn    = 0x02,  ///< Token to enable dumping of OTP data
    Max                   ///< Represents the maximum value for token types
};

/// Attributes of a token.
enum class Attr : uint16_t
{
    Invalid = 0,  ///< Invalid token attribute.
    Temporary,    ///< Token consumed in volatile memory RAM. One time use.
    Installable,  ///< Token installed in Flash.
    Max
};

/// Returned as error response to Debug token MCTP VDM messages.
enum class Status
{
    Success                  = 0,
    SizeMismatch             = 1,
    AuthFailed               = 2,
    NonceCheckFailed         = 3,
    SerialNumInvalid         = 4,
    McufwVersionCheckFailed  = 5,
    InstallFailedBgCheck     = 6,
    InternalError            = 7,
    NonceUpdateFailed        = 8,
    MaxInstallations         = 9,
    InstallCountReadFailed   = 10,
    InstallCountUpdateFailed = 11,
    OtpReadFailed            = 12,
    OtpWriteFailed           = 13,
    AgentVersionCheckFailed  = 14,
    UnsupportedTokenType     = 15,
    TokenTypeMismatch        = 16,
};

constexpr uint32_t DebugOptionsFlags = static_cast<uint32_t>(Type::FlashDebugFw)
                                     | static_cast<uint32_t>(Type::OtpDumpEn);

/// Valid hash sizes
enum class HashSize : uint16_t
{
    Sha256          = 32,
    Sha384          = 48,
    Ecdsa384R       = 48,
    Ecdsa384L       = 48,
    Mcu384Pubkey    = 96,
    Mcu384Signature = 96,
};

/// Debug token structure.
struct [[gnu::packed]] Token
{
    /// Magic number to identify the token in memory.
    uint32_t magic_num;
    /// Version of the token struct. [31:16] major version, [15:0] minor version.
    uint32_t version;

    uint16_t size;                               ///< Size of the struct.
    Attr     attr;                               ///< Token attributes.
    Type     type;                               ///< Type of token.
    uint8_t  mcufw_version[DT_MCU_FW_VER_SIZE];  ///< FW version of MCU.
    uint8_t  nonce[DT_NONCE_SIZE];               ///< Nonce generated during token request.
    uint8_t  serial_num[DT_DEV_SER_NUM_SIZE];    ///< Serial number of the MCU.
    uint16_t agent_ver;                          ///< Agent version
    uint8_t  reserved[18];                       ///< Reserved.

    /// Public key to authenticate debug signed apfw.
    uint8_t dbg_pubkey[static_cast<uint16_t>(debugtoken::HashSize::Mcu384Pubkey)];
    /// Signature of the token.
    uint8_t signature[static_cast<uint16_t>(debugtoken::HashSize::Mcu384Signature)];
};

constexpr std::size_t TOKEN_SIZE = sizeof(Token);

/**
 *  This function will read the first word in SPI flash at token offset
 *  and compare it against MagicNum to detect presence of valid token.
 *
 *  @return true if token is detected in flash (identifier matches), false otherwise.
 */
bool is_dbg_token_in_flash();

/**
 *  This function erases the token from ec internal spi flash.
 *  No-op if the token is not in spi flash.
 *
 *  @return debugtoken::Status::Success on success, error code otherwise.
 */
Status erase_installed_dbg_token();

/**
 *  This function installs debug token to spi flash upon reciept as mctp vdm
 *  message payload. Only after successful authentication will a token get installed
 *  in ecfw internal spi flash.
 *
 *  @param[in] token_buffer  Pointer to token payload buffer.
 *
 *  @return debugtoken::Status::Success on success, error code otherwise.
 */
Status install_dbg_token(std::span<const uint8_t, debugtoken::PayloadSize> token_buffer);

/**
 *  This function reads token from the spi flash, copies it to local buffer,
 *  authenticates the copied payload and upon successful authentication.
 *
 *  @param[in] token_type  Type of token.
 *
 *  @return debugtoken::Status::Success on success, error code otherwise.
 */
Status load_and_auth_dbg_token();

/**
 *  This function authenticates the token data structure.
 *
 *  @param[in] token_payload  Reference to token payload buffer.
 *  @param[in] pubkey        Pointer to public key for authentication
 *
 *  @return debugtoken::Status::Success on success, error code otherwise.
 */
Status auth_token(
    std::array<uint8_t, TOKEN_SIZE>& token_payload,
    const std::array<uint8_t, static_cast<uint8_t>(nv::debugtoken::HashSize::Mcu384Pubkey)>&
        pubkey);

/**
 * @brief Returns the type of the token.
 *
 * This function returns the type of the currently loaded debug token.
 *
 * @return The type of the token.
 */
Type get_token_type();

}  // namespace debugtoken
}  // namespace nv
