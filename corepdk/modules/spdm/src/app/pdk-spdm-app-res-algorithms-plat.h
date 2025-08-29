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

#include "pdk-spdm-app-res-algorithms.h"

namespace pdk::spdm::platforms::res::algorithms {

/**
 * @brief Select the measurement specification.
 *
 * This function is used to select the measurement specification from responder, the input
 * parameters are the measurement specification supported by requester, and the output is the
 * measurement specification selected by responder.
 *
 * @return pdk::spdm::app::res::algorithms::MeasurementSpecificationSel
 *
 * @note
 * Not supported in current implementation with ada responder library.
 *
 */
pdk::spdm::app::res::algorithms::MeasurementSpecificationSel
select_measurement_specification_sel(bool provided_dmtf_meas_spec);

/**
 * @brief Select the measurement hash algorithm.
 *
 * This function is used to select the measurement hash algorithm from responder, the input
 * parameters are the hash algorithm supported by requester, and the output is the hash
 * algorithm selected by responder.
 *
 * @return pdk::spdm::app::res::algorithms::MeasurementHashAlgo
 */
pdk::spdm::app::res::algorithms::MeasurementHashAlgo
select_measurement_hash_alg(bool provided_tpm_alg_sha_256,
                            bool provided_tpm_alg_sha_384,
                            bool provided_tpm_alg_sha_512,
                            bool provided_tpm_alg_sha3_256,
                            bool provided_tpm_alg_sha3_384,
                            bool provided_tpm_alg_sha3_512,
                            bool provided_raw_bit_streams_only);

/**
 * @brief Select the asymmetric key signature algorithms for the purpose of signature
 * verification by the responder.
 *
 * This function is used to select the asymmetric key signature algorithms algorithm from
 * responder, the input parameters are the asymmetric key signature algorithms supported by
 * requester, and the output is the asymmetric key signature algorithms selected by responder.
 *
 * @return pdk::spdm::app::res::algorithms::BaseAsymSel
 */
pdk::spdm::app::res::algorithms::BaseAsymSel
select_base_asym_alg(bool provided_tpm_alg_ecdsa_ecc_nist_p384,
                     bool provided_tpm_alg_rsapss_4096,
                     bool provided_tpm_alg_rsassa_4096,
                     bool provided_tpm_alg_ecdsa_ecc_nist_p256,
                     bool provided_tpm_alg_rsapss_3072,
                     bool provided_tpm_alg_rsassa_3072,
                     bool provided_tpm_alg_rsapss_2048,
                     bool provided_tpm_alg_rsassa_2048,
                     bool provided_tpm_alg_ecdsa_ecc_nist_p521);

/**
 * @brief Select the cryptographic hashing algorithms.
 *
 * This function is used to select the cryptographic hashing algorithms from responder, the
 * input parameters are the cryptographic hashing algorithms supported by requester, and the
 * output is the cryptographic hashing algorithms selected by responder.
 *
 * @return pdk::spdm::app::res::algorithms::BaseHashSel
 */
pdk::spdm::app::res::algorithms::BaseHashSel
select_base_hash_alg(bool provided_tpm_alg_sha_256,
                     bool provided_tpm_alg_sha_384,
                     bool provided_tpm_alg_sha_512,
                     bool provided_tpm_alg_sha3_256,
                     bool provided_tpm_alg_sha3_384,
                     bool provided_tpm_alg_sha3_512);

/**
 * @brief Select the DHE(Diffie-Hellman Ephemeral) algorithm.
 *
 * This function is used to select the DHE algorithm from responder, the input parameters are
 * the DHE algorithm supported by requester, and the output is the DHE algorithm selected by
 * responder.
 *
 * @return pdk::spdm::app::res::algorithms::DheAlgSupported
 */
pdk::spdm::app::res::algorithms::DheAlgSupported select_dhe_alg(bool provided_secp521r1,
                                                                bool provided_secp384r1,
                                                                bool provided_secp256r1,
                                                                bool provided_ffdhe4096,
                                                                bool provided_ffdhe3072,
                                                                bool provided_ffdhe2048);

/**
 * @brief Select the AEADCipherSuite algorithm.
 *
 * This function is used to select the AEADCipherSuite algorithm from responder, the input
 * parameters are the AEAD algorithm supported by requester, and the output is the AEAD
 * algorithm selected by responder.
 *
 * @return pdk::spdm::app::res::algorithms::AeadAlgSupported
 */
pdk::spdm::app::res::algorithms::AeadAlgSupported select_aead_alg(
    bool provided_chacha20_poly1305, bool provided_aes_256_gcm, bool provided_aes_128_gcm);

/**
 * @brief Select the asymmetric key signature algorithmm for the purpose of signature
 * verification by the responder.
 *
 * This function is used to select the request asymmetric key signature algorithm from
 * responder, the input parameters are the asymmetric key signature algorithm supported by
 * requester, and the output is the asymmetric key signature algorithm selected by responder.
 *
 * @return pdk::spdm::app::res::algorithms::ReqBaseAsymAlgSupported
 */
pdk::spdm::app::res::algorithms::ReqBaseAsymAlgSupported
select_req_base_asym_alg(bool provided_tpm_alg_ecdsa_ecc_nist_p384,
                         bool provided_tpm_alg_rsapss_4096,
                         bool provided_tpm_alg_rsassa_4096,
                         bool provided_tpm_alg_ecdsa_ecc_nist_p256,
                         bool provided_tpm_alg_rsapss_3072,
                         bool provided_tpm_alg_rsassa_3072,
                         bool provided_tpm_alg_rsapss_2048,
                         bool provided_tpm_alg_rsassa_2048,
                         bool provided_tpm_alg_ecdsa_ecc_nist_p521);

/**
 * @brief Select the key schedule algorithm.
 *
 * This function is used to select the key schedule algorithm from responder, the input
 * parameters are the key schedule algorithm supported by requester, and the output is the key
 * schedule algorithm selected by responder.
 *
 * @return pdk::spdm::app::res::algorithms::KeyScheduleAlgSupported
 *
 * @note
 * Not supported in current implementation with ada responder library.
 */
pdk::spdm::app::res::algorithms::KeyScheduleAlgSupported
select_key_schedule_alg(bool provided_spdm_key_schedule);

}  // namespace pdk::spdm::platforms::res::algorithms
