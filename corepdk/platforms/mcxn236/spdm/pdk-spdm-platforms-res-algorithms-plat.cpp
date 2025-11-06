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
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-algorithms-plat.h"

namespace pdk::spdm::platforms::res::algorithms {

/**
 * @brief get the measurement specification supported by responder.
 *
 * This function is used to get the measurement specification supported by responder.
 *
 * @param provided_dmtf_meas_spec
 * @return void
 */
void negotiate_algo_measurement_specification_sel(bool& provided_dmtf_meas_spec)
{
    provided_dmtf_meas_spec = true;
    return;
}

/**
 * @brief get the measurement hash algorithm supported by responder.
 *
 * This function is used to get the measurement hash algorithm supported by responder.
 *
 * @param provided_raw_bit_streams_only
 * @param provided_tpm_alg_sha_256
 * @param provided_tpm_alg_sha_384
 * @param provided_tpm_alg_sha_512
 * @param provided_tpm_alg_sha3_256
 * @param provided_tpm_alg_sha3_384
 * @param provided_tpm_alg_sha3_512
 * @param provided_tpm_alg_sm3_256
 * @return void
 */
void negotiate_algo_measurement_hash_alg(bool& provided_raw_bit_streams_only,
                                         bool& provided_tpm_alg_sha_256,
                                         bool& provided_tpm_alg_sha_384,
                                         bool& provided_tpm_alg_sha_512,
                                         bool& provided_tpm_alg_sha3_256,
                                         bool& provided_tpm_alg_sha3_384,
                                         bool& provided_tpm_alg_sha3_512,
                                         bool& provided_tpm_alg_sm3_256)
{
    provided_raw_bit_streams_only = false;
    provided_tpm_alg_sha_256      = false;
    provided_tpm_alg_sha_384      = true;
    provided_tpm_alg_sha_512      = false;
    provided_tpm_alg_sha3_256     = false;
    provided_tpm_alg_sha3_384     = false;
    provided_tpm_alg_sha3_512     = false;
    provided_tpm_alg_sm3_256      = false;
    return;
}

/**
 * @brief get the asymmetric key signature algorithms for generation of digital signatures
 * supported by responder.
 *
 * This function is used to get the asymmetric key signature algorithms supported by responder.
 *
 * @param provided_tpm_alg_rsassa_2048
 * @param provided_tpm_alg_rsapss_2048
 * @param provided_tpm_alg_rsassa_3072
 * @param provided_tpm_alg_rsapss_3072
 * @param provided_tpm_alg_ecdsa_ecc_nist_p256
 * @param provided_tpm_alg_rsassa_4096
 * @param provided_tpm_alg_rsapss_4096
 * @param provided_tpm_alg_ecdsa_ecc_nist_p384
 * @param provided_tpm_alg_ecdsa_ecc_nist_p521
 * @param provided_tpm_alg_sm2_ecc_sm2_p256
 * @param provided_tpm_alg_eddsa_ed25519
 * @param provided_tpm_alg_eddsa_ed448
 * @return void
 */
void negotiate_algo_base_asym_alg(bool& provided_tpm_alg_rsassa_2048,
                                  bool& provided_tpm_alg_rsapss_2048,
                                  bool& provided_tpm_alg_rsassa_3072,
                                  bool& provided_tpm_alg_rsapss_3072,
                                  bool& provided_tpm_alg_ecdsa_ecc_nist_p256,
                                  bool& provided_tpm_alg_rsassa_4096,
                                  bool& provided_tpm_alg_rsapss_4096,
                                  bool& provided_tpm_alg_ecdsa_ecc_nist_p384,
                                  bool& provided_tpm_alg_ecdsa_ecc_nist_p521,
                                  bool& provided_tpm_alg_sm2_ecc_sm2_p256,
                                  bool& provided_tpm_alg_eddsa_ed25519,
                                  bool& provided_tpm_alg_eddsa_ed448)
{
    provided_tpm_alg_rsassa_2048         = false;
    provided_tpm_alg_rsapss_2048         = false;
    provided_tpm_alg_rsassa_3072         = false;
    provided_tpm_alg_rsapss_3072         = false;
    provided_tpm_alg_ecdsa_ecc_nist_p256 = false;
    provided_tpm_alg_rsassa_4096         = false;
    provided_tpm_alg_rsapss_4096         = false;
    provided_tpm_alg_ecdsa_ecc_nist_p384 = true;
    provided_tpm_alg_ecdsa_ecc_nist_p521 = false;
    provided_tpm_alg_sm2_ecc_sm2_p256    = false;
    provided_tpm_alg_eddsa_ed25519       = false;
    provided_tpm_alg_eddsa_ed448         = false;
    return;
}

/**
 * @brief get the cryptographic hashing algorithms supported by responder.
 *
 * This function is used to get the cryptographic hashing algorithms supported by responder.
 *
 * @param provided_tpm_alg_sha_256
 * @param provided_tpm_alg_sha_384
 * @param provided_tpm_alg_sha_512
 * @param provided_tpm_alg_sha3_256
 * @param provided_tpm_alg_sha3_384
 * @param provided_tpm_alg_sha3_512
 * @param provided_tpm_alg_sm3_256
 * @return void
 */
void negotiate_algo_base_hash_alg(bool& provided_tpm_alg_sha_256,
                                  bool& provided_tpm_alg_sha_384,
                                  bool& provided_tpm_alg_sha_512,
                                  bool& provided_tpm_alg_sha3_256,
                                  bool& provided_tpm_alg_sha3_384,
                                  bool& provided_tpm_alg_sha3_512,
                                  bool& provided_tpm_alg_sm3_256)
{
    provided_tpm_alg_sha_256  = false;
    provided_tpm_alg_sha_384  = true;
    provided_tpm_alg_sha_512  = false;
    provided_tpm_alg_sha3_256 = false;
    provided_tpm_alg_sha3_384 = false;
    provided_tpm_alg_sha3_512 = false;
    provided_tpm_alg_sm3_256  = false;
    return;
}

/**
 * @brief get the DHE(Diffie-Hellman Ephemeral) algorithm supported by responder.
 *
 * This function is used to get the DHE algorithm supported by responder.
 *
 * @param provided_secp521r1
 * @param provided_secp384r1
 * @param provided_secp256r1
 * @param provided_ffdhe4096
 * @param provided_ffdhe3072
 * @param provided_ffdhe2048
 * @param provided_sm2_p256
 * @return void
 */
void negotiate_algo_dhe(bool& provided_secp521r1,
                        bool& provided_secp384r1,
                        bool& provided_secp256r1,
                        bool& provided_ffdhe4096,
                        bool& provided_ffdhe3072,
                        bool& provided_ffdhe2048,
                        bool& provided_sm2_p256)
{
    provided_secp521r1 = false;
    provided_secp384r1 = true;
    provided_secp256r1 = false;
    provided_ffdhe4096 = false;
    provided_ffdhe3072 = false;
    provided_ffdhe2048 = false;
    provided_sm2_p256  = false;
    return;
}

/**
 * @brief get the AEADCipherSuite algorithm supported by responder.
 *
 * This function is used to get the AEADCipherSuite algorithm supported by responder.
 *
 * @param provided_aes_128_gcm
 * @param provided_aes_256_gcm
 * @param provided_chacha20_poly1305
 * @param provided_aead_sm4_gcm
 * @return void
 */
void negotiate_algo_aead(bool& provided_aes_128_gcm,
                         bool& provided_aes_256_gcm,
                         bool& provided_chacha20_poly1305,
                         bool& provided_aead_sm4_gcm)
{
    provided_aes_128_gcm       = false;
    provided_aes_256_gcm       = false;
    provided_chacha20_poly1305 = false;
    provided_aead_sm4_gcm      = false;
    return;
}

/**
 * @brief get the asymmetric key signature algorithms supported by responder.
 * verification by the responder.
 *
 * This function is used to get the asymmetric key signature algorithms for verification of
 * digital signatures supported by responder.
 *
 * @param provided_tpm_alg_rsassa_2048
 * @param provided_tpm_alg_rsapss_2048
 * @param provided_tpm_alg_rsassa_3072
 * @param provided_tpm_alg_rsapss_3072
 * @param provided_tpm_alg_ecdsa_ecc_nist_p256
 * @param provided_tpm_alg_rsassa_4096
 * @param provided_tpm_alg_rsapss_4096
 * @param provided_tpm_alg_ecdsa_ecc_nist_p384
 * @param provided_tpm_alg_ecdsa_ecc_nist_p521
 * @param provided_tpm_alg_sm2_ecc_sm2_p256
 * @param provided_tpm_alg_eddsa_ed25519
 * @param provided_tpm_alg_eddsa_ed448
 * @return void
 */
void negotiate_algo_rbaa(bool& provided_tpm_alg_rsassa_2048,
                         bool& provided_tpm_alg_rsapss_2048,
                         bool& provided_tpm_alg_rsassa_3072,
                         bool& provided_tpm_alg_rsapss_3072,
                         bool& provided_tpm_alg_ecdsa_ecc_nist_p256,
                         bool& provided_tpm_alg_rsassa_4096,
                         bool& provided_tpm_alg_rsapss_4096,
                         bool& provided_tpm_alg_ecdsa_ecc_nist_p384,
                         bool& provided_tpm_alg_ecdsa_ecc_nist_p521,
                         bool& provided_tpm_alg_sm2_ecc_sm2_p256,
                         bool& provided_tpm_alg_eddsa_ed25519,
                         bool& provided_tpm_alg_eddsa_ed448)
{
    provided_tpm_alg_rsassa_2048         = false;
    provided_tpm_alg_rsapss_2048         = false;
    provided_tpm_alg_rsassa_3072         = false;
    provided_tpm_alg_rsapss_3072         = false;
    provided_tpm_alg_ecdsa_ecc_nist_p256 = false;
    provided_tpm_alg_rsassa_4096         = false;
    provided_tpm_alg_rsapss_4096         = false;
    provided_tpm_alg_ecdsa_ecc_nist_p384 = true;
    provided_tpm_alg_ecdsa_ecc_nist_p521 = false;
    provided_tpm_alg_sm2_ecc_sm2_p256    = false;
    provided_tpm_alg_eddsa_ed25519       = false;
    provided_tpm_alg_eddsa_ed448         = false;
    return;
}

/**
 * @brief get the key schedule algorithm supported by responder.
 *
 * This function is used to get the key schedule algorithm supported by responder.
 *
 * @param provided_spdm_key_schedule
 * @return void
 */
void negotiate_algo_key_schedule(bool& provided_spdm_key_schedule)
{
    provided_spdm_key_schedule = false;
    return;
}

}  // namespace pdk::spdm::platforms::res::algorithms