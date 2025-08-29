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
#include <array>
#include <variant>
#include <stdint.h>

#include "pdk-spdm-app-res-ada-library.h"
#include "pdk-spdm-app-res-algorithms-enum.h"
#include "pdk-spdm-app-res-code.h"
namespace pdk::spdm::app::res::algorithms {

#ifdef __cplusplus
extern "C" {
#endif
uint8_t spdm_platform_select_measurement_hash_algo(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t                                                   tpm_alg_sha_256,
    uint8_t                                                   tpm_alg_sha_384,
    uint8_t                                                   tpm_alg_sha_512,
    uint8_t                                                   tpm_alg_sha3_256,
    uint8_t                                                   tpm_alg_sha3_384,
    uint8_t                                                   tpm_alg_sha3_512,
    uint8_t                                                   raw_bit_streams_only);

int64_t spdm_platform_select_base_asym_algo(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t                                                   tpm_alg_ecdsa_ecc_nist_p384,
    uint8_t                                                   tpm_alg_rsapss_4096,
    uint8_t                                                   tpm_alg_rsassa_4096,
    uint8_t                                                   tpm_alg_ecdsa_ecc_nist_p256,
    uint8_t                                                   tpm_alg_rsapss_3072,
    uint8_t                                                   tpm_alg_rsassa_3072,
    uint8_t                                                   tpm_alg_rsapss_2048,
    uint8_t                                                   tpm_alg_rsassa_2048,
    uint8_t                                                   tpm_alg_ecdsa_ecc_nist_p521);

uint8_t spdm_platform_select_base_hash_algo(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t                                                   tpm_alg_sha_256,
    uint8_t                                                   tpm_alg_sha_384,
    uint8_t                                                   tpm_alg_sha_512,
    uint8_t                                                   tpm_alg_sha3_256,
    uint8_t                                                   tpm_alg_sha3_384,
    uint8_t                                                   tpm_alg_sha3_512);

uint8_t
spdm_platform_select_dhe(pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
                         uint8_t                                                   secp521r1,
                         uint8_t                                                   secp384r1,
                         uint8_t                                                   secp256r1,
                         uint8_t                                                   ffdhe4096,
                         uint8_t                                                   ffdhe3072,
                         uint8_t                                                   ffdhe2048);

uint8_t
spdm_platform_select_aead(pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
                          uint8_t chacha20_poly1305,
                          uint8_t aes_256_gcm,
                          uint8_t aes_128_gcm);

int64_t
spdm_platform_select_rbaa(pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
                          uint8_t ra_tpm_alg_ecdsa_ecc_nist_p384,
                          uint8_t ra_tpm_alg_rsapss_4096,
                          uint8_t ra_tpm_alg_rsassa_4096,
                          uint8_t ra_tpm_alg_ecdsa_ecc_nist_p256,
                          uint8_t ra_tpm_alg_rsapss_3072,
                          uint8_t ra_tpm_alg_rsassa_3072,
                          uint8_t ra_tpm_alg_rsapss_2048,
                          uint8_t ra_tpm_alg_rsassa_2048,
                          uint8_t ra_tpm_alg_ecdsa_ecc_nist_p521);

#ifdef __cplusplus
}
#endif

using SPDMVersion = uint8_t;
using Param1      = uint8_t;
using Param2      = uint8_t;
using Length      = uint16_t;

enum class MeasurementSpecificationSel : uint8_t
{
    NotSupport   = 0,
    DmtfMeasSpec = 1
};

using Reserved = uint8_t;

// MeasurementHashAlgo, BaseAsymSel, BaseHashSel declare in
// "pdk-spdm-app-res-algorithms-enum.h".

using Reserved12Bytes = std::array<uint8_t, 12>;
static_assert(sizeof(Reserved12Bytes) == 12);

enum class ExtAsymSelCount : uint8_t
{
    NotSelect = 0,
    Select    = 1
};
enum class ExtHashSelCount : uint8_t
{
    NotSelect = 0,
    Select    = 1
};
using Reserved2Bytes = std::array<uint8_t, 2>;

struct [[gnu::packed]] ExtendAlgorithmField
{
    uint8_t  registry_id;
    uint8_t  reserved;
    uint16_t algorithm_id;
};

using ExtAsymSel = ExtendAlgorithmField;

using ExtHashSel = ExtendAlgorithmField;
enum class AlgType : uint8_t
{
    Reserved_0      = 0,
    Reserved_1      = 1,
    DHE             = 2,
    AEADCipherSuite = 3,
    ReqBaseAsymAlg  = 4,
    KeySchedule     = 5
};
struct [[gnu::packed]] AlgCount
{
    uint8_t fixed_alg_count : 4;
    uint8_t ext_alg_count   : 4;
};

enum class DheAlgSupported : uint16_t
{
    NotSupport = 0,
    Ffdhe2048  = 0b1 << 0,
    Ffdhe3072  = 0b1 << 1,
    Ffdhe4096  = 0b1 << 2,
    Secp256r1  = 0b1 << 3,
    Secp384r1  = 0b1 << 4,
    Secp521r1  = 0b1 << 5
};

enum class AeadAlgSupported : uint16_t
{
    NotSupport       = 0,
    Aes128Gcm        = 0b1 << 0,
    Aes256Gcm        = 0b1 << 1,
    Chacha20Poly1305 = 0b1 << 2
};

enum class ReqBaseAsymAlgSupported : uint16_t
{
    NotSupport             = 0,
    TpmAlgRsassa2048       = 1 << 0,
    TpmAlgRsapss2048       = 1 << 1,
    TpmAlgRsassa3072       = 1 << 2,
    TpmAlgRsapss_3072      = 1 << 3,
    TpmAlgEcdsaEccNistP256 = 1 << 4,
    TpmAlgRsassa_4096      = 1 << 5,
    TpmAlgRsapss_4096      = 1 << 6,
    TpmAlgEcdsaEccNistP384 = 1 << 7,
    TpmAlgEcdsaEccNistP521 = 1 << 8,
};

enum class KeyScheduleAlgSupported : uint16_t
{
    NotSupport      = 0,
    SpdmKeySchedule = 1 << 0,
};

template<AlgType use_alg_type, bool use_alg_external>
requires(use_alg_type >= AlgType::DHE && use_alg_type <= AlgType::KeySchedule)
struct [[gnu::packed]] RespAlgStruct
{
    AlgType  alg_type;
    AlgCount alg_count;
};

template<AlgType use_alg_type>
requires(use_alg_type >= AlgType::DHE && use_alg_type <= AlgType::KeySchedule)
using AlgSupported = std::conditional_t<
    use_alg_type == AlgType::DHE,
    DheAlgSupported,
    std::conditional_t<
        use_alg_type == AlgType::AEADCipherSuite,
        AeadAlgSupported,
        std::conditional_t<use_alg_type == AlgType::ReqBaseAsymAlg,
                           ReqBaseAsymAlgSupported,
                           std::conditional_t<use_alg_type == AlgType::KeySchedule,
                                              KeyScheduleAlgSupported,
                                              std::monostate>>>>;

template<AlgType use_alg_type>
struct [[gnu::packed]] RespAlgStruct<use_alg_type, true>
{
    AlgType                    alg_type;
    AlgCount                   alg_count;
    AlgSupported<use_alg_type> alg_supported;
    ExtendAlgorithmField       extend_algorithm_field;
};

template<AlgType use_alg_type>
struct [[gnu::packed]] RespAlgStruct<use_alg_type, false>
{
    AlgType                    alg_type;
    AlgCount                   alg_count;
    AlgSupported<use_alg_type> alg_supported;
};

static_assert(sizeof(RespAlgStruct<AlgType::DHE, true>) == 8);
static_assert(sizeof(RespAlgStruct<AlgType::DHE, false>) == 4);

static_assert(sizeof(RespAlgStruct<AlgType::AEADCipherSuite, true>) == 8);
static_assert(sizeof(RespAlgStruct<AlgType::AEADCipherSuite, false>) == 4);

static_assert(sizeof(RespAlgStruct<AlgType::ReqBaseAsymAlg, true>) == 8);
static_assert(sizeof(RespAlgStruct<AlgType::ReqBaseAsymAlg, false>) == 4);

static_assert(sizeof(RespAlgStruct<AlgType::KeySchedule, true>) == 8);
static_assert(sizeof(RespAlgStruct<AlgType::KeySchedule, false>) == 4);

struct [[gnu::packed]] SpdmResAlgorithmsPayload
{
    SPDMVersion                 spdm_version;
    code::SpdmResponseCode      RequestResponseCode;
    Param1                      param1;
    Param2                      param2;
    Length                      length;
    MeasurementSpecificationSel measurement_specification_sel;
    Reserved                    reserved;
    MeasurementHashAlgo         measurement_hash_algo;
    BaseAsymSel                 base_asym_sel;
    BaseHashSel                 base_hash_sel;
    Reserved12Bytes             reserved_12_bytes;
    ExtAsymSelCount             ext_asym_count;
    ExtHashSelCount             ext_hash_sel_count;
    Reserved2Bytes              reserved_2_bytes;
    // [[todo]]
    // need to be template to append the following fields, note the sequence of the fields is
    // fixed, but each field is optional
    // 1. extend_asym_sel
    // 2. extend_hash_sel
    // 3. DHE
    // 4. AEADCipherSuite
    // 5. ReqBaseAsymAlg
    // 6. KeySchedule
};
static_assert(sizeof(SpdmResAlgorithmsPayload) == 36);

}  // namespace pdk::spdm::app::res::algorithms