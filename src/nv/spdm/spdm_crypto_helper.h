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

#include <algorithm>
#include <array>
#include <chrono>
#include <span>
#include <stdint.h>
#include <type_traits>
#include <variant>
#include "nv/crypto/aes_gcm.h"
#include "nv/crypto/ecdh_p384.h"
#include "nv/fw_parser/fw_parser_mcu.h"
#include "nv/ipc/task.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "corepdk/modules/spdm/src/app/crypto/hash/pdk-spdm-app-res-crypto-hash.h"
#include "nv/vrot/interface/types.h"
#include "nv/spdm/crypto_status.h"

#include "mbedtls/bignum.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"

namespace nv {
namespace spdm {

enum class Status;

namespace crypto {

typedef enum
: uint32_t
{
    Sha256HashSize = 256 / 8,
    Sha384HashSize = 384 / 8,
    Sha512HashSize = 512 / 8,
} HashSizeT;

using Sha384Digest = std::array<uint8_t, Sha384HashSize>;

typedef enum
: uint32_t
{
    Ecdsa256SignatureSize = 256 / 8 * 2,
    Ecdsa384SignatureSize = 384 / 8 * 2,
    Ecdsa512SignatureSize = 512 / 8 * 2,
} EcdsaSignatureSizeT;

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
    nv::vrot::ApId                     ap_id;
    // Assigned by spdm_crypto_helper when the async request is created.
    // Callers use the returned id to match the completion callback.
    uint8_t         auth_request_id;
    nv::ipc::TaskId request_task_id;
};

struct AuthenticateApFirmwareResponseParameter
{
    nv::spdm::crypto::CryptoStatus     auth_result;
    nv::fw_parser::ap::ParsingApFwType input_parsing_fw_type;
    nv::vrot::ApId                     ap_id;
    uint8_t                            auth_request_id;
    nv::ipc::TaskId                    request_task_id;
};

constexpr size_t UdsKeyBytes                      = 32;
constexpr size_t PufWrappedBytes                  = 84;
constexpr size_t PufWrapPrimitiveOutputBytes      = PufWrappedBytes;
constexpr size_t AesGcmPrimitiveMaxAadBytes       = 16;
constexpr size_t AesGcmPrimitiveMaxPlaintextBytes = 16;
constexpr size_t AesGcmPrimitiveTagBytes          = 16;
constexpr size_t AesGcmPrimitiveMaxOutputBytes    = AesGcmPrimitiveMaxPlaintextBytes
                                               + AesGcmPrimitiveTagBytes;
constexpr size_t TrngPrimitiveMaxOutputBytes   = 32;
constexpr size_t EcdhP384PrimitiveMaxSaltBytes = 32;
constexpr size_t EcdhP384PrimitiveMaxInfoBytes = 256;
constexpr size_t EcdhP384PrimitiveMaxAadBytes  = 256;
// This primitive unwraps one AES-256 model-weight key.
constexpr size_t EcdhP384PrimitiveMaxCiphertextBytes = nv::crypto::Aes256KeyBytes;

constexpr size_t CryptoPrimitiveOutputBytes = std::max(
    AesGcmPrimitiveMaxOutputBytes,
    std::max(PufWrapPrimitiveOutputBytes, TrngPrimitiveMaxOutputBytes));

enum class CryptoPrimitiveRequestId : uint32_t
{
    TrngGenerate                       = 1U,
    PufWrap                            = 2U,
    PufUnwrap                          = 3U,
    Aes256GcmEncrypt                   = 4U,
    EcdhP384GenerateKeyPair            = 5U,
    EcdhP384ClearKeyPair               = 6U,
    EcdhP384HkdfSha384Aes256GcmDecrypt = 7U,
    Ecdsa384Sha384Sign                 = 8U,
};

enum class CryptoPrimitiveQueueType : uint8_t
{
    Request,
    Response,
};

struct TrngGenerateRequestParameter
{
    nv::ipc::TaskId          request_task_id;
    nv::ipc::CoreId          request_core_id;
    uint32_t                 request_id;
    uint32_t                 output_size;
    std::array<uint32_t, 20> reserved;
};

struct PufWrapRequestParameter
{
    nv::ipc::TaskId request_task_id;
    nv::ipc::CoreId request_core_id;
    uint32_t        request_id;
    alignas(uint32_t) std::array<uint8_t, UdsKeyBytes> key;
    std::array<uint32_t, 13> reserved;
};

struct PufUnwrapRequestParameter
{
    nv::ipc::TaskId request_task_id;
    nv::ipc::CoreId request_core_id;
    uint32_t        request_id;
    alignas(uint32_t) std::array<uint8_t, PufWrappedBytes> wrapped;
};

struct Aes256GcmEncryptRequestParameter
{
    nv::ipc::TaskId request_task_id;
    nv::ipc::CoreId request_core_id;
    uint32_t        request_id;
    alignas(uint32_t) nv::crypto::Aes256Key key;
    alignas(uint32_t) nv::crypto::AesGcmIv iv;
    size_t aad_size;
    size_t plaintext_size;
    alignas(uint32_t) std::array<uint8_t, AesGcmPrimitiveMaxAadBytes> aad;
    alignas(uint32_t) std::array<uint8_t, AesGcmPrimitiveMaxPlaintextBytes> plaintext;
};

struct EcdhP384GenerateKeyPairRequestParameter
{
    nv::ipc::TaskId                request_task_id;
    nv::ipc::CoreId                request_core_id;
    uint32_t                       request_id;
    nv::crypto::EcdhP384PublicKey* public_key;
};

struct EcdhP384ClearKeyPairRequestParameter
{
    nv::ipc::TaskId request_task_id;
    nv::ipc::CoreId request_core_id;
    uint32_t        request_id;
};

struct EcdhP384HkdfSha384Aes256GcmDecryptRequestParameter
{
    nv::ipc::TaskId                      request_task_id;
    nv::ipc::CoreId                      request_core_id;
    uint32_t                             request_id;
    const nv::crypto::EcdhP384PublicKey* peer_public_key;
    const uint8_t*                       salt;
    uint32_t                             salt_size;
    const uint8_t*                       info;
    uint32_t                             info_size;
    const uint8_t*                       aad;
    uint32_t                             aad_size;
    const uint8_t*                       iv;
    const uint8_t*                       ciphertext;
    uint32_t                             ciphertext_size;
    const uint8_t*                       tag;
    uint8_t*                             plaintext_out;
    uint32_t                             plaintext_out_size;
};

struct Ecdsa384Sha384SignRequestParameter
{
    nv::ipc::TaskId                             request_task_id;
    nv::ipc::CoreId                             request_core_id;
    uint32_t                                    request_id;
    const Sha384Digest*                         hash;
    std::array<uint8_t, Ecdsa384SignatureSize>* signature;
};

struct CryptoPrimitiveResponseParameters
{
    nv::ipc::TaskId request_task_id;
    uint32_t        request_id;
    CryptoStatus    status;
    alignas(uint32_t) std::array<uint8_t, CryptoPrimitiveOutputBytes> output;
};

using CryptoReqResParameters = std::variant<AuthenticateMcuFirmwareRequestParameter,
                                            AuthenticateMcuFirmwareResponseParameter,
                                            AuthenticateApFirmwareRequestParameter,
                                            AuthenticateApFirmwareResponseParameter>;
static_assert(sizeof(CryptoReqResParameters) == nv::ipc::SpdmCryptoHelperQueueSize,
              "the SpdmCryptoHelperQueueSize value shopuld equal to structure size");

using CryptoPrimitiveRequestParameters = std::variant<
    TrngGenerateRequestParameter,
    PufWrapRequestParameter,
    PufUnwrapRequestParameter,
    Aes256GcmEncryptRequestParameter,
    EcdhP384GenerateKeyPairRequestParameter,
    EcdhP384ClearKeyPairRequestParameter,
    EcdhP384HkdfSha384Aes256GcmDecryptRequestParameter,
    Ecdsa384Sha384SignRequestParameter>;

CryptoStatus spdm_hash_data(uint8_t* ret, const uint8_t* data, uint32_t data_len);
CryptoStatus spdm_hash_sha1(uint8_t* ret, const uint8_t* data, uint32_t data_len);
bool         hmac_sha384(std::span<const uint8_t> key,
                         std::span<const uint8_t> message,
                         Sha384Digest&            hmac);
bool         hkdf_sha384(std::span<const uint8_t> ikm,
                         std::span<const uint8_t> salt,
                         std::span<const uint8_t> info,
                         Sha384Digest&            output);
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
send_authenticate_firmware_request(const nv::vrot::ApInfo&            ap,
                                   nv::fw_parser::ap::ParsingApFwType InputParseingApFwType,
                                   uint8_t&                           auth_request_id,
                                   nv::ipc::TaskId request_task_id = nv::ipc::TaskId::Begin);

// SecureBoot supplies a storage slot. SPDM resolves it to the logical
// authentication target used by the firmware parser and persistence paths.
CryptoStatus
send_authenticate_firmware_request(const nv::vrot::ApInfo& ap,
                                   uint8_t                 slot,
                                   uint8_t&                auth_request_id,
                                   nv::ipc::TaskId request_task_id = nv::ipc::TaskId::Begin);

nv::vrot::ApOpErrCode request_runtime_consistency();

CryptoStatus authenticate_ap_firmware(const nv::vrot::ApInfo&            ap,
                                      nv::fw_parser::ap::ParsingApFwType InputParseingApFwType,
                                      uint8_t&                           auth_request_id,
                                      nv::ipc::TaskId request_task_id = nv::ipc::TaskId::Begin);

// SPDM task execution path for a request already created by this helper.
// Requesters should call authenticate_ap_firmware() or
// send_authenticate_firmware_request() so the helper can assign and return the
// request id.
CryptoStatus authenticate_ap_firmware_with_request_id(
    const nv::vrot::ApInfo&            ap,
    nv::fw_parser::ap::ParsingApFwType InputParseingApFwType,
    nv::ipc::TaskId                    request_task_id,
    uint8_t                            auth_request_id);

nv::spdm::Status
                 submit_primitive_request(CryptoPrimitiveRequestParameters&  request,
                                          CryptoPrimitiveResponseParameters& response,
                                          std::chrono::milliseconds timeout = std::chrono::milliseconds{500});
void             process_primitive_request_queue();
bool             is_primitive_request_queue_enabled();
nv::ipc::QueueId get_primitive_queue_id(CryptoPrimitiveQueueType queue_type);
CryptoStatus     trng_generate(std::span<uint8_t> output);
CryptoStatus     puf_wrap(std::span<const uint8_t> key, std::span<uint8_t> wrapped);
CryptoStatus     puf_unwrap(std::span<const uint8_t> wrapped, std::span<uint8_t> key);
CryptoStatus     aes_256_gcm_encrypt(const nv::crypto::Aes256Key&                        key,
                                     std::span<const uint8_t, nv::crypto::AesGcmIvBytes> iv,
                                     std::span<const uint8_t>                            aad,
                                     std::span<const uint8_t>                            plaintext,
                                     std::span<uint8_t>                                  ciphertext,
                                     std::span<uint8_t, nv::crypto::AesGcmTagBytes>      tag);
CryptoStatus     ecdh_p384_generate_key_pair(nv::crypto::EcdhP384PublicKey& public_key);
CryptoStatus     ecdh_p384_clear_key_pair();
CryptoStatus     ecdh_p384_hkdf_sha384_aes_256_gcm_decrypt(
        const nv::crypto::EcdhP384PublicKey&                 peer_public_key,
        std::span<const uint8_t>                             salt,
        std::span<const uint8_t>                             info,
        std::span<const uint8_t>                             aad,
        std::span<const uint8_t, nv::crypto::AesGcmIvBytes>  iv,
        std::span<const uint8_t>                             ciphertext,
        std::span<const uint8_t, nv::crypto::AesGcmTagBytes> tag,
        std::span<uint8_t>                                   plaintext_out);

template<nv::ipc::TaskId task_id>
requires(task_id == nv::ipc::TaskId::Pldm)
void send_authenticate_mcu_firmware_result(AuthenticateMcuFirmwareResponseParameter result);

template<nv::ipc::TaskId task_id>
requires(task_id == nv::ipc::TaskId::Pldm)
void send_authenticate_ap_firmware_result(AuthenticateApFirmwareResponseParameter result);

void send_response_back(const CryptoReqResParameters& result);
// key for strata CPLD
constexpr std::array<std::array<uint8_t, Ecdsa384PublicKeySize>, 2>
    ApFwPublicKeys = nv::ipc::ApFwPublicKeys;

}  // namespace crypto
}  // namespace spdm
}  // namespace nv
