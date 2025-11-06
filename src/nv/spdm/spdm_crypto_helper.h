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

#include <stdint.h>
#include <span>
#include <chrono>
#include <array>
#include <type_traits>
#include <variant>
#include "nv/fw_parser/fw_parser_mcu.h"
#include "nv/ipc/task.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "corepdk/modules/spdm/src/app/crypto/hash/pdk-spdm-app-res-crypto-hash.h"

#include "mbedtls/bignum.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"

namespace nv {
namespace spdm {
namespace crypto {

typedef enum
: uint32_t
{
    Sha256HashSize = 256 / 8,
    Sha384HashSize = 384 / 8,
    Sha512HashSize = 512 / 8,
} HashSizeT;

typedef enum
: uint32_t
{
    Ecdsa256SignatureSize = 256 / 8 * 2,
    Ecdsa384SignatureSize = 384 / 8 * 2,
    Ecdsa512SignatureSize = 512 / 8 * 2,
} EcdsaSignatureSizeT;

enum class CryptoStatus : uint8_t
{
    Success = 0,
    FailEcdsaSign,
    FailGetPriKey,
    FailLoadEcdsaContext,
    FailHashStart,
    FailHashUpdate,
    FailHashCalc,
    FailRandomGen,
    FailSignatureBufferLength,
    FailSignatureVerify,
    FailCfpaAccess,
    FailCmpaAccess,
    FailNbootContextInit,
    FailNbootImageAuthenticate,
    FailSecurityVersionRollBack,
    FailImageSigningKeyRevoke,
    FailEfuseAccess,
    FailSendToSpdm,
    FailParsingFirmware,
    FailApMetadataRead,
    FailApImageRead,
    FailApImageHashMismatch,
    FailApPublicKeyMismatch,
    FailUnknown,  // this FailUnknown should be the last value.
    End,
};

constexpr uint32_t Ecdsa384PublicKeySize = 96;

constexpr uint32_t CryptoEngineBufferSize = 1024;

struct Sha384Context  // utilize the RAII to manage the context
{
    void* ctx;
    Sha384Context() : ctx(nullptr)
    {
        ctx = pdk::spdm::app::res::crypto::hash::libspdm_sha384_new();
    }
    bool init()
    {
        if (ctx == nullptr) {
            return false;
        }
        return pdk::spdm::app::res::crypto::hash::libspdm_sha384_init(ctx);
    }
    bool update(const uint8_t* data, size_t size)
    {
        return pdk::spdm::app::res::crypto::hash::libspdm_sha384_update(ctx, data, size);
    }
    bool finish(uint8_t* hash)
    {
        return pdk::spdm::app::res::crypto::hash::libspdm_sha384_final(ctx, hash);
    }
    ~Sha384Context()
    {
        if (ctx != nullptr) {
            pdk::spdm::app::res::crypto::hash::libspdm_sha384_free(ctx);
        }
    }
};

struct AuthenticateMcuFirmwareRequestParameter
{
    nv::ipc::CoreId                   request_core_id;
    nv::fw_parser::mcu::ParsingFwType input_parsing_fw_type;
    nv::ipc::TaskId                   request_task_id;
};

struct AuthenticateMcuFirmwareResponseParameter
{
    nv::spdm::crypto::CryptoStatus    auth_result;
    nv::fw_parser::mcu::ParsingFwType input_parsing_fw_type;
    nv::ipc::TaskId                   request_task_id;
};

struct AuthenticateApFirmwareRequestParameter
{
    nv::ipc::CoreId                    request_core_id;
    nv::fw_parser::ap::ParsingApFwType input_parsing_fw_type;
    nv::ipc::TaskId                    request_task_id;
};

struct AuthenticateApFirmwareResponseParameter
{
    nv::spdm::crypto::CryptoStatus     auth_result;
    nv::fw_parser::ap::ParsingApFwType input_parsing_fw_type;
    nv::ipc::TaskId                    request_task_id;
};

using CryptoReqResParameters = std::variant<AuthenticateMcuFirmwareRequestParameter,
                                            AuthenticateMcuFirmwareResponseParameter,
                                            AuthenticateApFirmwareRequestParameter,
                                            AuthenticateApFirmwareResponseParameter>;
static_assert(sizeof(CryptoReqResParameters) == nv::ipc::SpdmCryptoHelperQueueSize,
              "the SpdmCryptoHelperQueueSize value shopuld equal to structure size");
CryptoStatus spdm_hash_data(uint8_t* ret, const uint8_t* data, uint32_t data_len);
CryptoStatus spdm_hash_sha1(uint8_t* ret, const uint8_t* data, uint32_t data_len);
CryptoStatus spdm_random_data(uint8_t* data, size_t data_len);
CryptoStatus
spdm_ecdsa_sign(uint8_t* signature, size_t sig_size, const uint8_t* hash, size_t hash_size);
CryptoStatus spdm_ecdsa_verify(const std::array<uint8_t, Ecdsa384PublicKeySize>& pub_key,
                               std::span<const uint8_t>&                         r_signature,
                               std::span<const uint8_t>&                         s_signature,
                               std::array<uint8_t, Sha384HashSize>&              hash);

CryptoStatus
authenticate_mcu_firmware(const nv::fw_parser::mcu::ParsingFwType InputParseingFwType);
CryptoStatus
authenticate_ap_firmware(const nv::fw_parser::ap::ParsingApFwType InputParseingApFwType);

template<nv::ipc::TaskId task_id>
requires(task_id == nv::ipc::TaskId::Pldm)
void send_authenticate_mcu_firmware_result(AuthenticateMcuFirmwareResponseParameter result);

template<nv::ipc::TaskId task_id>
requires(task_id == nv::ipc::TaskId::Pldm)
void send_authenticate_ap_firmware_result(AuthenticateApFirmwareResponseParameter result);

void send_response_back(const CryptoReqResParameters& result);
// key for strata CPLD
constexpr std::array<std::array<uint8_t, Ecdsa384PublicKeySize>, 2> ApFwPublicKeys{
    {{0xE0, 0xE5, 0x9F, 0x88, 0x24, 0x87, 0x16, 0x8F, 0xAA, 0x2E, 0x02, 0x4C, 0x4D, 0x29,
      0x5F, 0x80, 0x67, 0xBE, 0xCF, 0xED, 0xC3, 0x7C, 0xC3, 0x1F, 0x99, 0x42, 0xBF, 0x44,
      0x3B, 0x09, 0xE7, 0x69, 0x65, 0x55, 0x55, 0xF7, 0xF2, 0x6F, 0xCC, 0x3B, 0x6E, 0xCB,
      0xA1, 0x44, 0x62, 0x01, 0xA4, 0x20, 0x71, 0x3F, 0x1F, 0x88, 0x2C, 0x29, 0xF0, 0xCB,
      0x0E, 0xE6, 0xC1, 0x02, 0x93, 0x51, 0x09, 0x33, 0xC5, 0xAB, 0x96, 0x98, 0x37, 0x7C,
      0xEF, 0x20, 0xB3, 0xA6, 0x26, 0xD2, 0xFC, 0x9B, 0xA6, 0x1E, 0x3B, 0xBA, 0x66, 0x76,
      0x7B, 0x60, 0x7E, 0xB3, 0x34, 0x40, 0xA0, 0xB1, 0x46, 0x4C, 0x1D, 0x0A},
     {0xEF, 0x6A, 0x1F, 0x27, 0xED, 0x6E, 0xD1, 0xA6, 0x29, 0x22, 0x45, 0x81, 0x37, 0x1F,
      0xB9, 0x52, 0x9B, 0xB0, 0x48, 0xEE, 0x77, 0x8F, 0xB2, 0x83, 0x45, 0xA7, 0x82, 0x8F,
      0x6B, 0xC0, 0x73, 0x96, 0x64, 0x4E, 0x6C, 0x26, 0x5D, 0x9A, 0xE0, 0x01, 0x62, 0xF2,
      0x84, 0xEE, 0x00, 0x36, 0x6E, 0x13, 0xA2, 0x13, 0x57, 0x83, 0x05, 0x42, 0x24, 0x9B,
      0x12, 0x47, 0x56, 0x79, 0x08, 0xB5, 0x24, 0xB3, 0xCD, 0x44, 0x9B, 0x05, 0x76, 0x6F,
      0x2B, 0x72, 0x80, 0xC1, 0x38, 0x40, 0xA3, 0xB9, 0x34, 0x72, 0x90, 0x7E, 0x09, 0x8E,
      0x9B, 0x46, 0x4C, 0x3B, 0x17, 0x1B, 0x55, 0x5E, 0x42, 0xD2, 0x3F, 0x99}}
};

}  // namespace crypto
}  // namespace spdm
}  // namespace nv