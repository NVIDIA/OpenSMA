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

#include <cstddef>
#include <span>
#include <cstdint>

#include "nv/flash/common.h"

namespace nv::debugtoken {

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
    uint8_t currently_installed;  // Byte 0: Is Debug token currently installed?
} DebugTokenStatsT;

/// General integer constants
enum Constants
{
    DebugTokenStatsTSize = 5,
};

// Log throttling configuration to prevent SPI flash wear out
constexpr uint32_t MaxLogFailures = 10;  // Maximum consecutive failure logs before throttling

/// Type of token.
enum class Type : uint32_t
{
    Invalid      = 0x00,  ///< UNDEFINED
    FlashDebugFw = 0x01,  ///< Debug firmware
    McuDebug     = 0x02,  ///< MCU debug capability token
    CpldDebug    = 0x04,  ///< CPLD debug capability token
    Max                   ///< Represents the maximum value for token types
};

constexpr uint32_t DebugOptionsFlags = static_cast<uint32_t>(Type::FlashDebugFw)
                                     | static_cast<uint32_t>(Type::McuDebug)
                                     | static_cast<uint32_t>(Type::CpldDebug);

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

// TLV constants
constexpr uint32_t TlvMagicNumber       = 0x31564C54;  // "TLV1" in ASCII
constexpr uint32_t TlvVersion           = 0x00000001;  // 1.0
constexpr uint32_t TokenIdentifierMagic = 0x5444434D;  // "MCDT" in ASCII

// Device type constants
constexpr uint16_t McuDeviceTypeValue = 0x0005;  // MCU device type value

// TLV parsing constants
constexpr uint32_t TlvHeaderSize  = 4;     // type(2) + length(2)
constexpr uint32_t MaxTlvEntries  = 16;    // Maximum TLV entries in a token
constexpr uint32_t ReasonableSize = 1024;  // Reasonable size for TLV tokens

// Flash sector size constraint
constexpr uint32_t FlashSectorSize = 8192;  // 0x2000 bytes per sector

// Debug Token operation constants
constexpr uint32_t ERASE_ALL_TOKENS        = 0xFFFFFFFF;
constexpr uint32_t ERASE_ALL_AND_INCREMENT = 0xFFFFFFFE;

// TLV token version constants
constexpr uint32_t TLV_AGENT_CHECK_TOKEN_VER = 0x0001;  // Token version that requires agent
                                                        // version check

// TLV Header field offsets
constexpr uint32_t TlvHeaderIdentifierOffset = 0;   // offset to identifier field
constexpr uint32_t TlvHeaderVersionOffset    = 4;   // offset to version field
constexpr uint32_t TlvHeaderSizeOffset       = 8;   // offset to size field
constexpr uint32_t TlvHeaderReservedOffset   = 12;  // offset to reserved field

// TLV field lengths
constexpr uint16_t TlvDeviceTypeLength  = 2;
constexpr uint16_t TlvNonceLength       = 16;
constexpr uint16_t TlvSerialLength      = DT_DEV_SER_NUM_SIZE;
constexpr uint16_t TlvFwverLength       = 4;
constexpr uint16_t TlvAgentverLength    = 2;
constexpr uint16_t TlvLifecycleLength   = 1;
constexpr uint16_t TlvTokenTypeLength   = 4;
constexpr uint16_t TlvTokenIdLength     = 4;
constexpr uint16_t TlvTokenConfigLength = 2;

// TLV token request payload size calculation based on DebugTokenTLVConfig_t structure:
// Device Type TLV: 2+2+2 = 6 bytes (type + length + value)
// Nonce TLV: 2+2+16 = 20 bytes (type + length + value)
// Device Serial TLV: 2+2+16 = 20 bytes (type + length + value)
// Firmware Version TLV: 2+2+4 = 8 bytes (type + length + value)
// Agent Version TLV: 2+2+2 = 6 bytes (type + length + value)
// Lifecycle State TLV: 2+2+1 = 5 bytes (type + length + value)
// Total TLV payload: 6+20+20+8+6+5 = 65 bytes (excluding TLV header)
constexpr uint32_t TlvTokenRequestSize = 65;

// Debug Token Type and Subtype definitions for 0x0016 TLV
// Token Type/Subtype pairs are 2 bytes each: [Type|Subtype]
constexpr uint8_t DebugTokenTypeDebugFw   = 0x01;  // Debug FW token type
constexpr uint8_t DebugTokenTypeMcuDebug  = 0x02;  // MCU debug capability token type
constexpr uint8_t DebugTokenTypeCpldDebug = 0x04;  // CPLD debug capability token type

// Default subtypes for each token type
constexpr uint8_t DebugTokenSubtypeDebugFw   = 0x00;  // Default subtype for Debug FW
constexpr uint8_t DebugTokenSubtypeMcuDebug  = 0x00;  // Default subtype for MCU debug
constexpr uint8_t DebugTokenSubtypeCpldDebug = 0x00;  // Default subtype for CPLD debug

// Subtypes for FlashDebugFw (0x01) token type
constexpr uint8_t DebugTokenSubtypeNone   = 0x00;  // No specific subtype
constexpr uint8_t DebugTokenSubtypeMcuFw  = 0x01;  // MCU firmware debug
constexpr uint8_t DebugTokenSubtypeCpldFw = 0x02;  // CPLD firmware debug

// Subtypes for MCU debug capability (0x02) token type
constexpr uint8_t DebugTokenSubtypeErrorInjection  = 0x01;  // Ras Test
constexpr uint8_t DebugTokenSubtypePwrFailI2cDebug = 0x02;  // pwr_fail_i2c_debug

// Subtypes for CpldDebug (0x04) token type
constexpr uint8_t DebugTokenSubtypeCpldUnlockEn = 0x01;  // CPLD unlock enable

// Valid subtypes bitmask for each token type
constexpr uint32_t DebugTokenSubtypeValidMaskDebugFw = DebugTokenSubtypeMcuFw
                                                     | DebugTokenSubtypeCpldFw;
constexpr uint32_t DebugTokenSubtypeValidMaskMcuDebug = DebugTokenSubtypePwrFailI2cDebug
                                                      | DebugTokenSubtypeErrorInjection;
constexpr uint32_t DebugTokenSubtypeValidMaskCpldDebug = DebugTokenSubtypeCpldUnlockEn;

constexpr uint32_t get_subtype_valid_mask(uint32_t token_type)
{
    switch (token_type) {
        case DebugTokenTypeDebugFw  : return DebugTokenSubtypeValidMaskDebugFw;
        case DebugTokenTypeMcuDebug : return DebugTokenSubtypeValidMaskMcuDebug;
        case DebugTokenTypeCpldDebug: return DebugTokenSubtypeValidMaskCpldDebug;
        default                     : return 0;
    }
}

constexpr uint16_t TokenTypeSubtypePairSize   = 8;  // Size of each type/subtype pair
constexpr uint16_t MaxTokenTypeSubtypePairs   = 4;  // Maximum number of type/subtype pairs
constexpr uint16_t TokenTypeSubtypeListLength = MaxTokenTypeSubtypePairs
                                              * TokenTypeSubtypePairSize;

// Bit positions for each token type
constexpr uint32_t DebugTokenBitPosDebugFw   = 0;  // Bit 0 = 0x1
constexpr uint32_t DebugTokenBitPosMcuDebug  = 1;  // Bit 1 = 0x2
constexpr uint32_t DebugTokenBitPosCpldDebug = 2;  // Bit 2 = 0x4

// Maximum number of supported token types
constexpr uint32_t MaxTokenTypes = 3;  // FlashDebugFw, McuDebug, CpldDebug

// SKU Information values
constexpr uint8_t McuDebugMode = 0x1;  // Debug mode SKU
constexpr uint8_t McuProdMode  = 0x2;  // Production mode SKU

// Lifecycle State values
enum class LifecycleState : uint8_t
{
    Manufacturing = 0x1,  ///< Manufacturing/Initial state (no keys revoked)
    Debug         = 0x2,  ///< Debug state (some prod keys revoked, debug key not revoked)
    Production    = 0x4,  ///< Production state (debug key revoked)
};

// TLV type definitions
// Reference: TLV Specification
// https://docs.google.com/document/d/1cSvvzHS_yPFEZB9UDV8l0gq4Hc7VwyPRAV3UsIgljL4/
enum class TlvType : uint16_t
{
    DeviceType              = 0x0001,  ///< Device type identifier
    ChallengeNonce          = 0x0002,  ///< Nonce data generated by device
    DeviceSerialNumber      = 0x0003,  ///< Unique serial number of the device
    DeviceSerialNumberArray = 0x0004,  ///< List of device serial numbers (multi-device
                                       ///< token)
    FirmwareVersion  = 0x0005,         ///< Firmware version requesting the token
    AgentVersion     = 0x0006,         ///< Token <-> device firmware lifecycle management
    LifecycleState   = 0x0007,         ///< Device lifecycle state
    TokenIdentifier  = 0x0008,         ///< Unique token identifier
    TokenType        = 0x0009,         ///< Type of token
    TokenConfig      = 0x000A,         ///< Token specific attributes
    NvidiaSignature  = 0x000B,         ///< NV signature over the entire token
    OemSignature     = 0x000C,         ///< Owner signature over the entire token
    InstallStatus    = 0x000D,         ///< A debug token is currently installed or not
    ProcessingStatus = 0x000E,         ///< Token processing status set by the end point
    SkuInformation   = 0x000F,         ///< Production sku or Debug sku
    NvidiaRatchet    = 0x0010,         ///< NV ratchet value
    OemRatchet       = 0x0011,         ///< Owner ratchet value
    ValidityCounter  = 0x0012,         ///< Counter which gets decremented upon each token
                                       ///< application
    CertificateChain      = 0x0013,    ///< Certificate chain for the signing authority
    MeasurementTranscript = 0x0014,    ///< Transcript for measurement index 50
    DeviceId              = 0x0015,    ///< Hardware fused identifier
    TokenTypeSubtypeList  = 0x0016,    ///< List of installed debug token type and subtype
                                       ///< pairs (8 bytes: [Type0|Subtype0][Type1|Subtype1]...)
    Payload = 0x0017,                  ///< Payload data in the token
};

// Debug Token NSM error codes
// Range 0x1000 to 0x1FFF is reserved exclusively for debug token feature
// https://docs.google.com/document/d/1hWlMRTTvou_KLZth6nbQR7zZ0MB-m-ZnW2iHb-2irHI/

enum class TokenErrorCode : uint16_t
{
    NoErrorCode        = 0x0000,  ///< Operation completed successfully
    TokenInternalError = 0x1000,  ///< An unexpected internal failure occurred while
                                  ///< processing the token request
    TokenInvalidFormat               = 0x1001,  ///< Token structure is invalid or malformed
    TokenSignatureVerificationFailed = 0x1002,  ///< Token signature or authentication
                                                ///< failed
    TokenInvalidNonce          = 0x1003,  ///< Nonce in the token is either expired or invalid
    TokenInvalidLifecycleState = 0x1004,  ///< The lifecycle state embedded in the
                                          ///< token does not match the device's
                                          ///< current lifecycle state
    TokenUnsupportedType    = 0x1005,     ///< Token type is not supported on this device
    TokenStorageError       = 0x1006,     ///< Failed to write token to device storage
    TokenRatchetCheckFailed = 0x1007,     ///< Token version is older than the ratchet
    ///< value
    TokenInternalErrorProcessing = 0x1008,  ///< Device internal error during token
    ///< command processing
    TokenFeatureDisabled         = 0x1009,  ///< Debug token feature is disabled on the device
    TokenFeatureDisabledByPolicy = 0x100A,  ///< Debug token feature disabled due to
    ///< policy restrictions
    TokenFwVersionMismatch   = 0x100B,  ///< TBD - Firmware version mismatch
    TokenInvalidSerialNumber = 0x100C,  ///< Device serial number does not match with
    ///< token serial number
    TokenInvalidPsid            = 0x100D,    ///< Invalid PSID
    TokenAlreadyInstalled       = 0x100E,    ///< A debug token is already installed
    TokenNotInstalled           = 0x100F,    ///< A debug token is not installed
    TokenHashVerificationFailed = 0x1010,    ///< Hash verification failed during token
                                             ///< processing
    TokenEraseRejectedNoProdImage = 0x1011,  ///< Erase rejected: no prod-signed
                                             ///< image to boot after erase
    // 0x1012 - 0x1013 reserved for future use
};

// TLV header structure
struct [[gnu::packed]] TlvHeader
{
    /// Identifier (0x544C5631 - "TLV1" in ASCII, as per spec)
    uint32_t identifier;
    /// Version field [31:16] major version, [15:0] minor version
    uint32_t version;
    /// Size of the TLV payload excluding the header
    uint32_t size;
    /// Reserved for future use
    uint8_t reserved[20];
};

// Specific TLV structures for fixed-size data
struct [[gnu::packed]] TlvDeviceType
{
    TlvType  type   = TlvType::DeviceType;  ///< Type: 0x0001
    uint16_t length = TlvDeviceTypeLength;  ///< Length: 2
    uint16_t value  = McuDeviceTypeValue;   ///< Device type value (MCU = 0x0005)
};

struct [[gnu::packed]] TlvNonce
{
    TlvType  type      = TlvType::ChallengeNonce;  ///< Type: 0x0002
    uint16_t length    = TlvNonceLength;           ///< Length: 16
    uint8_t  value[16] = {};                       ///< Nonce data (initialized to zeros)
};

struct [[gnu::packed]] TlvDeviceSerialNumber
{
    TlvType  type                       = TlvType::DeviceSerialNumber;  ///< Type: 0x0003
    uint16_t length                     = TlvSerialLength;  ///< Length: DT_DEV_SER_NUM_SIZE
    uint8_t  value[DT_DEV_SER_NUM_SIZE] = {};  ///< Device serial number (initialized to zeros)
};

struct [[gnu::packed]] TlvFirmwareVersion
{
    TlvType  type     = TlvType::FirmwareVersion;  ///< Type: 0x0005
    uint16_t length   = TlvFwverLength;            ///< Length: 4
    uint8_t  value[4] = {};                        ///< Firmware version (initialized to zeros)
};

struct [[gnu::packed]] TlvAgentVersion
{
    TlvType  type   = TlvType::AgentVersion;  ///< Type: 0x0006
    uint16_t length = TlvAgentverLength;      ///< Length: 2
    uint16_t value  = 0;                      ///< Agent version (initialized to 0)
};

struct [[gnu::packed]] TlvLifecycleState
{
    TlvType  type   = TlvType::LifecycleState;  ///< Type: 0x0007
    uint16_t length = TlvLifecycleLength;       ///< Length: 1
    uint8_t  value  = 0;                        ///< Lifecycle state value (initialized to 0)
};

struct [[gnu::packed]] TlvTokenIdentifier
{
    TlvType  type     = TlvType::TokenIdentifier;  ///< Type: 0x0008
    uint16_t length   = TlvTokenIdLength;          ///< Length: 4
    uint8_t  value[4] = {};  ///< Token identifier (initialized to zeros, typically
                             ///< "MCDT")
};

struct [[gnu::packed]] TlvTokenType
{
    TlvType  type   = TlvType::TokenType;  ///< Type: 0x0009
    uint16_t length = TlvTokenTypeLength;  ///< Length: 4
    uint32_t value  = 0;                   ///< Token type (initialized to 0)
};

struct [[gnu::packed]] TlvTokenConfig
{
    TlvType  type   = TlvType::TokenConfig;  ///< Type: 0x000A
    uint16_t length = TlvTokenConfigLength;  ///< Length: 2
    uint16_t value  = 0;                     ///< Token configuration (initialized to 0)
};

// Token Type/Subtype pair structure
struct [[gnu::packed]] TokenTypeSubtypePair
{
    uint32_t type    = 0;  ///< Token type
    uint32_t subtype = 0;  ///< Token subtype bitmap
};

struct [[gnu::packed]] TlvTokenTypeSubtypeList
{
    TlvType              type   = TlvType::TokenTypeSubtypeList;  ///< Type: 0x0016
    uint16_t             length = TokenTypeSubtypeListLength;  ///< Length of type/subtype pairs
    TokenTypeSubtypePair pairs[MaxTokenTypeSubtypePairs] = {};  ///< Type/Subtype pairs
};

// Generic TLV structure for variable-length data
struct [[gnu::packed]] TlvData
{
    TlvType  type;    ///< Type of TLV data
    uint16_t length;  ///< Length of value field
    // Note: value field is accessed via pointer arithmetic
    // The actual value data follows this structure in memory
};

// Generic TLV entry structure for internal processing
struct TlvEntry
{
    uint16_t type;         ///< Type of TLV data
    uint16_t length;       ///< Length of value field
    uint32_t data_offset;  ///< Offset to actual data in the token buffer
};

// TLV information structure for token parsing (internal use)
struct TlvInfo
{
    uint32_t start_offset;  ///< Offset from token_data base pointer (CERT ARR36-C compliant)
    uint16_t type;          ///< Type of TLV data
    uint16_t length;        ///< Length of value field
};

// Debug Token TLV Configuration structure for SPDM measurements
struct [[gnu::packed]] DebugTokenTlvConfig
{
    // Standard TLV Header (32 bytes: identifier + version + size + reserved)
    TlvHeader header;

    // TLV Data section using specialized TLV structures
    TlvDeviceType         device_type;       // Device Type TLV (bytes 33-32 to 37-36)
    TlvNonce              nonce;             // Nonce TLV (bytes 39-38 to 57-42)
    TlvDeviceSerialNumber serial;            // Device Serial Number TLV (bytes 59-58 to 77-62)
    TlvFirmwareVersion    firmware_version;  // Firmware Version TLV (bytes 79-78 to 85-82)
    TlvAgentVersion       agent_version;     // Agent Version TLV (bytes 87-86 to 91-90)
    TlvLifecycleState     lifecycle_state;   // Lifecycle State TLV (bytes 92-96)
};

// Type alias for backward compatibility
using DebugTokenTLVConfig_t = DebugTokenTlvConfig;

// Token Request structure
struct [[gnu::packed]] TokenRequest
{
    TlvHeader header;  ///< Fixed header

    // Measurement transcript TLV
    TlvData measurement_transcript_tlv;  ///< Type: 0x0015, Length: variable

    // Certificate chain TLV
    TlvData certificate_chain_tlv;  ///< Type: 0x0014, Length: variable
};

/// Token Main structure
struct [[gnu::packed]] TokenMain
{
    TlvHeader header;  ///< Fixed header

    // Magic number TLV
    TlvTokenIdentifier magic_num_tlv;  ///< Type: 0x0008, Length: 4, Value: "MCDT"

    // Nonce TLV
    TlvNonce nonce_tlv;  ///< Type: 0x0002, Length: 16

    // Device serial number TLV
    TlvDeviceSerialNumber serial_num_tlv;  ///< Type: 0x0003, Length: 16

    // Firmware version TLV
    TlvFirmwareVersion fw_version_tlv;  ///< Type: 0x0005, Length: 4

    // Agent version TLV
    TlvAgentVersion agent_version_tlv;  ///< Type: 0x0006, Length: 2

    // Token type TLV
    TlvTokenType token_type_tlv;  ///< Type: 0x0009, Length: 4

    // Token configuration TLV
    TlvTokenConfig token_config_tlv;  ///< Type: 0x000A, Length: 2

    // NV signature TLV
    TlvData nv_signature_tlv;  ///< Type: 0x000B, Length: variable
};

/**
 *  Get the remapped flash address for debug token based on the current boot slot.
 *  Handles dual-slot boot by applying flash address remapping via get_flash_address().
 *
 *  @return Remapped flash address for the debug token region.
 */
nv::flash::Address get_token_flash_address();

/**
 *  TLV version: This function will read the first word in SPI flash at token offset
 *  and compare it against MagicNum to detect presence of valid token.
 *
 *  @return true if token is detected in flash (identifier matches), false otherwise.
 */
bool is_dbg_token_tlv_in_flash();

/**
 * Read data from a specific TLV field in a token buffer
 *
 * @param[in] token_span      Span of token buffer
 * @param[in] target_type     Type of TLV to read
 * @param[out] output_buffer  Span of output buffer to store the read data
 * @return TokenErrorCode::NoErrorCode if successful, error code otherwise
 */
TokenErrorCode read_tlv_field(const std::span<const uint8_t>& token_span,
                              TlvType                         target_type,
                              std::span<uint8_t>&             output_buffer);

/**
 * Validate data in a specific TLV field against expected values
 *
 * @param[in] token_span    Span of token buffer
 * @param[in] target_type   Type of TLV to validate
 * @param[in] expected_data Span of expected data to compare against
 * @return TokenErrorCode::NoErrorCode if validation passes, error code otherwise
 */
TokenErrorCode validate_tlv_field(const std::span<const uint8_t>& token_span,
                                  TlvType                         target_type,
                                  const std::span<const uint8_t>& expected_data);

/**
 *  TLV version: This function erases the token from ec internal spi flash.
 *  No-op if the token is not in spi flash.
 *
 *  @return TokenErrorCode for standardized error reporting
 */
TokenErrorCode erase_installed_dbg_token_tlv();

/**
 *  TLV version: This function checks if debug token is enabled for the specified type.
 *  Verifies authentication from NPDS cache or loads and authenticates from flash.
 *
 *  @param[in] token_type  Type of token to verify
 *  @return TokenErrorCode for standardized error reporting
 */
TokenErrorCode check_debug_token_type_enabled(Type token_type);

/**
 *  Check if debug token with specific subtype is enabled.
 *
 *  @param[in] token_type    Type of token
 *  @param[in] token_subtype Subtype to verify
 *  @return TokenErrorCode for standardized error reporting
 */
TokenErrorCode check_debug_token_subtype_enabled(Type token_type, uint32_t token_subtype);

/**
 *  Fast path: true if token type and subtype bits are set in NPDS cache (no full auth).
 *
 *  @param[in] token_type       Debug token type (FlashDebugFw / McuDebug / CpldDebug)
 *  @param[in] token_subtype_bit Single subtype bit, e.g. DebugTokenSubtypePwrFailI2cDebug
 */
bool is_debug_token_subtype_enabled_cached(Type token_type, uint32_t token_subtype_bit);

/**
 *  TLV version: This function installs debug token to spi flash upon reciept as mctp vdm
 *  message payload. Only after successful authentication will a token get installed
 *  in mcufw internal spi flash.
 *
 *  @param[in] token_buffer  Pointer to token payload buffer.
 *
 *  @return TokenErrorCode for standardized error reporting, or 0x0000 on success
 */
TokenErrorCode install_dbg_token_tlv(const std::span<const uint8_t>& token_span);

/**
 *  TLV version: This function authenticates the token data structure.
 *
 *  @param[in] token_buffer  Reference to token payload buffer.
 *  @param[in] pubkey        Pointer to public key for authentication
 *
 *  @return TokenErrorCode for standardized error reporting
 */
TokenErrorCode auth_token_tlv(
    const std::span<const uint8_t>& token_payload,
    const std::array<uint8_t, static_cast<uint8_t>(debugtoken::HashSize::Mcu384Pubkey)>&
        pubkey);

/**
 * Synchronize debug-token feature state after AP boot/authentication.
 *
 * Covers:
 *   - Token already installed before this MCU boot -> AP feature is restored.
 *   - Token erased during MCU recovery -> AP feature stays locked.
 */
void sync_debug_token_features_on_boot();

}  // namespace nv::debugtoken
