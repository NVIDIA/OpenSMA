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
#include "pdk-spdm-app-res-algorithms.h"

#include <limits>
#include <utility>

#include "pdk-spdm-app-res-algorithms-plat.h"

namespace pdk::spdm::app::res::algorithms {

namespace {
size_t find_hash_size_according_to_selected_measurement(
    const MeasurementHashAlgo measurement_hash_algo)
{
    size_t ret_len = 0;
    switch (measurement_hash_algo) {
        case MeasurementHashAlgo::NotSupport      : ret_len = 0; break;
        case MeasurementHashAlgo::RawBitStreamOnly: ret_len = 0; break;
        case MeasurementHashAlgo::TpmAlgSha256:
            ret_len = HashAlgoDigestSize::TpmAlgSha256Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha384:
            ret_len = HashAlgoDigestSize::TpmAlgSha384Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha512:
            ret_len = HashAlgoDigestSize::TpmAlgSha512Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha3_256:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_256Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha3_384:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_384Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha3_512:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_512Size;
            break;
        default: ret_len = 0; break;
    }
    return ret_len;
}
size_t find_signature_size_according_to_base_hash_algo(const BaseHashSel base_hash_algo)
{
    size_t ret_len = 0;
    switch (base_hash_algo) {
        case BaseHashSel::NotSupport  : ret_len = 0; break;
        case BaseHashSel::TpmAlgSha256: ret_len = HashAlgoDigestSize::TpmAlgSha256Size; break;
        case BaseHashSel::TpmAlgSha384: ret_len = HashAlgoDigestSize::TpmAlgSha384Size; break;
        case BaseHashSel::TpmAlgSha512: ret_len = HashAlgoDigestSize::TpmAlgSha512Size; break;
        case BaseHashSel::TpmAlgSha3_256:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_256Size;
            break;
        case BaseHashSel::TpmAlgSha3_384:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_384Size;
            break;
        case BaseHashSel::TpmAlgSha3_512:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_512Size;
            break;
        default: ret_len = 0; break;
    }
    return ret_len;
}
size_t find_signature_size_according_to_base_asym_algo(const BaseAsymSel base_asym_algo)
{
    size_t ret_len = 0;
    switch (base_asym_algo) {
        case BaseAsymSel::NotSupport: ret_len = 0; break;
        case BaseAsymSel::TpmAlgRsassa2048:
            ret_len = AsymAlgoSignatureSize::TpmAlgRsassa2048Size;
            break;
        case BaseAsymSel::TpmAlgRsapss2048:
            ret_len = AsymAlgoSignatureSize::TpmAlgRsapss2048Size;
            break;
        case BaseAsymSel::TpmAlgRsassa3072:
            ret_len = AsymAlgoSignatureSize::TpmAlgRsassa3072Size;
            break;
        case BaseAsymSel::TpmAlgRsapss3072:
            ret_len = AsymAlgoSignatureSize::TpmAlgRsapss3072Size;
            break;
        case BaseAsymSel::TpmAlgEcdsaEccNistP256:
            ret_len = AsymAlgoSignatureSize::TpmAlgEcdsaEccNistP256Size;
            break;
        case BaseAsymSel::TpmAlgRsassa4096:
            ret_len = AsymAlgoSignatureSize::TpmAlgRsassa4096Size;
            break;
        case BaseAsymSel::TpmAlgRsapss4096:
            ret_len = AsymAlgoSignatureSize::TpmAlgRsapss4096Size;
            break;
        case BaseAsymSel::TpmAlgEcdsaEccNistP384:
            ret_len = AsymAlgoSignatureSize::TpmAlgEcdsaEccNistP384Size;
            break;
        case BaseAsymSel::TpmAlgEcdsaEccNistP521:
            ret_len = AsymAlgoSignatureSize::TpmAlgEcdsaEccNistP521Size;
            break;
        default: ret_len = 0; break;
    }
    return ret_len;
}

}  // namespace

#ifdef __cplusplus
extern "C" {
#endif
uint8_t spdm_platform_select_measurement_hash_algo(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t                                                                    tpm_alg_sha_256,
    uint8_t                                                                    tpm_alg_sha_384,
    uint8_t                                                                    tpm_alg_sha_512,
    uint8_t                                                                    tpm_alg_sha3_256,
    uint8_t                                                                    tpm_alg_sha3_384,
    uint8_t                                                                    tpm_alg_sha3_512,
    uint8_t raw_bit_streams_only

)
{
    instance->measurement_hash_alg = platforms::res::algorithms::select_measurement_hash_alg(
        1 == tpm_alg_sha_256,
        1 == tpm_alg_sha_384,
        1 == tpm_alg_sha_512,
        1 == tpm_alg_sha3_256,
        1 == tpm_alg_sha3_384,
        1 == tpm_alg_sha3_512,
        1 == raw_bit_streams_only);
    instance->measurement_hash_alg_length = find_hash_size_according_to_selected_measurement(
        instance->measurement_hash_alg);

    auto const select_result = std::to_underlying(instance->measurement_hash_alg);

    if (select_result > std::numeric_limits<uint8_t>::max()) {
        // should not reach since the max value in the MeasurementHashAlgo is 1 << 6
        // [TpmAlgSha3_512]
        return 0;
    }
    // return the selected measurement hash value
    return static_cast<uint8_t>(select_result);
}

int64_t spdm_platform_select_base_asym_algo(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t tpm_alg_ecdsa_ecc_nist_p384,
    uint8_t tpm_alg_rsapss_4096,
    uint8_t tpm_alg_rsassa_4096,
    uint8_t tpm_alg_ecdsa_ecc_nist_p256,
    uint8_t tpm_alg_rsapss_3072,
    uint8_t tpm_alg_rsassa_3072,
    uint8_t tpm_alg_rsapss_2048,
    uint8_t tpm_alg_rsassa_2048,
    uint8_t tpm_alg_ecdsa_ecc_nist_p521)
{
    instance->base_asym_algo = platforms::res::algorithms::select_base_asym_alg(
        1 == tpm_alg_ecdsa_ecc_nist_p384,
        1 == tpm_alg_rsapss_4096,
        1 == tpm_alg_rsassa_4096,
        1 == tpm_alg_ecdsa_ecc_nist_p256,
        1 == tpm_alg_rsapss_3072,
        1 == tpm_alg_rsassa_3072,
        1 == tpm_alg_rsapss_2048,
        1 == tpm_alg_rsassa_2048,
        1 == tpm_alg_ecdsa_ecc_nist_p521);
    instance->base_asym_algo_length = find_signature_size_according_to_base_asym_algo(
        instance->base_asym_algo);

    auto const select_result = std::to_underlying(instance->base_asym_algo);

    // return the selected bash asym algo
    return static_cast<int64_t>(select_result);
}

uint8_t spdm_platform_select_base_hash_algo(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t                                                                    tpm_alg_sha_256,
    uint8_t                                                                    tpm_alg_sha_384,
    uint8_t                                                                    tpm_alg_sha_512,
    uint8_t                                                                    tpm_alg_sha3_256,
    uint8_t                                                                    tpm_alg_sha3_384,
    uint8_t                                                                    tpm_alg_sha3_512)
{
    instance->base_hash_algo = platforms::res::algorithms::select_base_hash_alg(
        1 == tpm_alg_sha_256,
        1 == tpm_alg_sha_384,
        1 == tpm_alg_sha_512,
        1 == tpm_alg_sha3_256,
        1 == tpm_alg_sha3_384,
        1 == tpm_alg_sha3_512);
    instance->base_hash_algo_length = find_signature_size_according_to_base_hash_algo(
        instance->base_hash_algo);

    auto const select_result = std::to_underlying(instance->base_hash_algo);

    if (select_result > std::numeric_limits<uint8_t>::max()) {
        // should not reach since the max value in the BaseHashSel is 1 << 5 [TpmAlgSha3_512]
        return 0;
    }
    return static_cast<uint8_t>(select_result);
}

uint8_t spdm_platform_select_dhe(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t                                                                    secp521r1,
    uint8_t                                                                    secp384r1,
    uint8_t                                                                    secp256r1,
    uint8_t                                                                    ffdhe4096,
    uint8_t                                                                    ffdhe3072,
    uint8_t                                                                    ffdhe2048)
{
    const auto select_result = std::to_underlying(
        platforms::res::algorithms::select_dhe_alg(1 == secp521r1,
                                                   1 == secp384r1,
                                                   1 == secp256r1,
                                                   1 == ffdhe4096,
                                                   1 == ffdhe3072,
                                                   1 == ffdhe2048));
    if (select_result > std::numeric_limits<uint8_t>::max()) {
        // should not reach since the max value in the DheAlgSupported is 1 << 5 [Secp521r1]
        return 0;
    }
    return static_cast<uint8_t>(select_result);
}

uint8_t spdm_platform_select_aead(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t chacha20_poly1305,
    uint8_t aes_256_gcm,
    uint8_t aes_128_gcm)
{
    const auto select_result = std::to_underlying(platforms::res::algorithms::select_aead_alg(
        1 == chacha20_poly1305, 1 == aes_256_gcm, 1 == aes_128_gcm));

    if (select_result > std::numeric_limits<uint8_t>::max()) {
        // should not reach since the max value in the AeadAlgSupported is 1 << 2
        // [Chacha20Poly1305]
        return 0;
    }
    return static_cast<uint8_t>(select_result);
}

int64_t spdm_platform_select_rbaa(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t ra_tpm_alg_ecdsa_ecc_nist_p384,
    uint8_t ra_tpm_alg_rsapss_4096,
    uint8_t ra_tpm_alg_rsassa_4096,
    uint8_t ra_tpm_alg_ecdsa_ecc_nist_p256,
    uint8_t ra_tpm_alg_rsapss_3072,
    uint8_t ra_tpm_alg_rsassa_3072,
    uint8_t ra_tpm_alg_rsapss_2048,
    uint8_t ra_tpm_alg_rsassa_2048,
    uint8_t ra_tpm_alg_ecdsa_ecc_nist_p521)
{
    const auto select_result = std::to_underlying(
        platforms::res::algorithms::select_req_base_asym_alg(
            1 == ra_tpm_alg_ecdsa_ecc_nist_p384,
            1 == ra_tpm_alg_rsapss_4096,
            1 == ra_tpm_alg_rsassa_4096,
            1 == ra_tpm_alg_ecdsa_ecc_nist_p256,
            1 == ra_tpm_alg_rsapss_3072,
            1 == ra_tpm_alg_rsassa_3072,
            1 == ra_tpm_alg_rsapss_2048,
            1 == ra_tpm_alg_rsassa_2048,
            1 == ra_tpm_alg_ecdsa_ecc_nist_p521));

    return static_cast<int64_t>(select_result);
}

int64_t spdm_platform_select_key_schedule(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t spdm_key_schedule)
{
    const auto select_result = std::to_underlying(
        platforms::res::algorithms::select_key_schedule_alg(1 == spdm_key_schedule));
    return static_cast<int64_t>(select_result);
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::algorithms
