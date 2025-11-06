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
void spdm_library_measurement_specification_sel(bool& support_dmtf_meas_spec)
{
    platforms::res::algorithms::negotiate_algo_measurement_specification_sel(
        support_dmtf_meas_spec);
}

void spdm_library_negotiate_algo_measurement_hash_algo(bool& support_raw_bit_streams_only,
                                                       bool& support_tpm_alg_sha_256,
                                                       bool& support_tpm_alg_sha_384,
                                                       bool& support_tpm_alg_sha_512,
                                                       bool& support_tpm_alg_sha3_256,
                                                       bool& support_tpm_alg_sha3_384,
                                                       bool& support_tpm_alg_sha3_512,
                                                       bool& support_tpm_alg_sm3_256)
{
    platforms::res::algorithms::negotiate_algo_measurement_hash_alg(
        support_raw_bit_streams_only,
        support_tpm_alg_sha_256,
        support_tpm_alg_sha_384,
        support_tpm_alg_sha_512,
        support_tpm_alg_sha3_256,
        support_tpm_alg_sha3_384,
        support_tpm_alg_sha3_512,
        support_tpm_alg_sm3_256);
}

void spdm_library_negotiate_algo_base_asym_algo(bool& support_tpm_alg_rsassa_2048,
                                                bool& support_tpm_alg_rsapss_2048,
                                                bool& support_tpm_alg_rsassa_3072,
                                                bool& support_tpm_alg_rsapss_3072,
                                                bool& support_tpm_alg_ecdsa_ecc_nist_p256,
                                                bool& support_tpm_alg_rsassa_4096,
                                                bool& support_tpm_alg_rsapss_4096,
                                                bool& support_tpm_alg_ecdsa_ecc_nist_p384,
                                                bool& support_tpm_alg_ecdsa_ecc_nist_p521,
                                                bool& support_tpm_alg_sm2_ecc_sm2_p256,
                                                bool& support_tpm_alg_eddsa_ed25519,
                                                bool& support_tpm_alg_eddsa_ed448)
{
    platforms::res::algorithms::negotiate_algo_base_asym_alg(
        support_tpm_alg_rsassa_2048,
        support_tpm_alg_rsapss_2048,
        support_tpm_alg_rsassa_3072,
        support_tpm_alg_rsapss_3072,
        support_tpm_alg_ecdsa_ecc_nist_p256,
        support_tpm_alg_rsassa_4096,
        support_tpm_alg_rsapss_4096,
        support_tpm_alg_ecdsa_ecc_nist_p384,
        support_tpm_alg_ecdsa_ecc_nist_p521,
        support_tpm_alg_sm2_ecc_sm2_p256,
        support_tpm_alg_eddsa_ed25519,
        support_tpm_alg_eddsa_ed448);
}

void spdm_library_negotiate_algo_base_hash_algo(bool& support_tpm_alg_sha_256,
                                                bool& support_tpm_alg_sha_384,
                                                bool& support_tpm_alg_sha_512,
                                                bool& support_tpm_alg_sha3_256,
                                                bool& support_tpm_alg_sha3_384,
                                                bool& support_tpm_alg_sha3_512,
                                                bool& support_tpm_alg_sm3_256)
{
    platforms::res::algorithms::negotiate_algo_base_hash_alg(support_tpm_alg_sha_256,
                                                             support_tpm_alg_sha_384,
                                                             support_tpm_alg_sha_512,
                                                             support_tpm_alg_sha3_256,
                                                             support_tpm_alg_sha3_384,
                                                             support_tpm_alg_sha3_512,
                                                             support_tpm_alg_sm3_256);
}

void spdm_library_negotiate_algo_dhe(bool& support_secp521r1,
                                     bool& support_secp384r1,
                                     bool& support_secp256r1,
                                     bool& support_ffdhe4096,
                                     bool& support_ffdhe3072,
                                     bool& support_ffdhe2048,
                                     bool& support_sm2_p256)
{
    platforms::res::algorithms::negotiate_algo_dhe(support_secp521r1,
                                                   support_secp384r1,
                                                   support_secp256r1,
                                                   support_ffdhe4096,
                                                   support_ffdhe3072,
                                                   support_ffdhe2048,
                                                   support_sm2_p256);
}

void spdm_library_negotiate_algo_aead(bool& support_aes_128_gcm,
                                      bool& support_aes_256_gcm,
                                      bool& support_chacha20_poly1305,
                                      bool& support_aead_sm4_gcm)
{
    platforms::res::algorithms::negotiate_algo_aead(support_aes_128_gcm,
                                                    support_aes_256_gcm,
                                                    support_chacha20_poly1305,
                                                    support_aead_sm4_gcm);
}

void spdm_library_negotiate_algo_rbaa(bool& support_tpm_alg_rsassa_2048,
                                      bool& support_tpm_alg_rsapss_2048,
                                      bool& support_tpm_alg_rsassa_3072,
                                      bool& support_tpm_alg_rsapss_3072,
                                      bool& support_tpm_alg_ecdsa_ecc_nist_p256,
                                      bool& support_tpm_alg_rsassa_4096,
                                      bool& support_tpm_alg_rsapss_4096,
                                      bool& support_tpm_alg_ecdsa_ecc_nist_p384,
                                      bool& support_tpm_alg_ecdsa_ecc_nist_p521,
                                      bool& support_tpm_alg_sm2_ecc_sm2_p256,
                                      bool& support_tpm_alg_eddsa_ed25519,
                                      bool& support_tpm_alg_eddsa_ed448)
{
    platforms::res::algorithms::negotiate_algo_rbaa(support_tpm_alg_rsassa_2048,
                                                    support_tpm_alg_rsapss_2048,
                                                    support_tpm_alg_rsassa_3072,
                                                    support_tpm_alg_rsapss_3072,
                                                    support_tpm_alg_ecdsa_ecc_nist_p256,
                                                    support_tpm_alg_rsassa_4096,
                                                    support_tpm_alg_rsapss_4096,
                                                    support_tpm_alg_ecdsa_ecc_nist_p384,
                                                    support_tpm_alg_ecdsa_ecc_nist_p521,
                                                    support_tpm_alg_sm2_ecc_sm2_p256,
                                                    support_tpm_alg_eddsa_ed25519,
                                                    support_tpm_alg_eddsa_ed448);
}

void spdm_library_negotiate_algo_key_schedule(bool& support_spdm_key_schedule)
{
    platforms::res::algorithms::negotiate_algo_key_schedule(support_spdm_key_schedule);
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::algorithms
