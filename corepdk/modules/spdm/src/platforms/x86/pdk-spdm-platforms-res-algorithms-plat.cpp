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
#include "app/pdk-spdm-app-res-algorithms-plat.h"

namespace pdk::spdm::platforms::res::algorithms {

pdk::spdm::app::res::algorithms::MeasurementHashAlgo
select_measurement_hash_alg([[maybe_unused]] bool provided_tpm_alg_sha_256,
                            bool                  provided_tpm_alg_sha_384,
                            [[maybe_unused]] bool provided_tpm_alg_sha_512,
                            [[maybe_unused]] bool provided_tpm_alg_sha3_256,
                            [[maybe_unused]] bool provided_tpm_alg_sha3_384,
                            [[maybe_unused]] bool provided_tpm_alg_sha3_512,
                            [[maybe_unused]] bool provided_raw_bit_streams_only)
{
    // we only support SHA384
    if (provided_tpm_alg_sha_384) {
        return pdk::spdm::app::res::algorithms::MeasurementHashAlgo::TpmAlgSha384;
    }
    // No mode set, other is not unsupported
    return pdk::spdm::app::res::algorithms::MeasurementHashAlgo::NotSupport;
}

pdk::spdm::app::res::algorithms::BaseAsymSel
select_base_asym_alg(bool                  provided_tpm_alg_ecdsa_ecc_nist_p384,
                     [[maybe_unused]] bool provided_tpm_alg_rsapss_4096,
                     [[maybe_unused]] bool provided_tpm_alg_rsassa_4096,
                     [[maybe_unused]] bool provided_tpm_alg_ecdsa_ecc_nist_p256,
                     [[maybe_unused]] bool provided_tpm_alg_rsapss_3072,
                     [[maybe_unused]] bool provided_tpm_alg_rsassa_3072,
                     [[maybe_unused]] bool provided_tpm_alg_rsapss_2048,
                     [[maybe_unused]] bool provided_tpm_alg_rsassa_2048,
                     [[maybe_unused]] bool provided_tpm_alg_ecdsa_ecc_nist_p521)
{
    // Only support ECC_NIST_P384
    if (provided_tpm_alg_ecdsa_ecc_nist_p384) {
        return pdk::spdm::app::res::algorithms::BaseAsymSel::TpmAlgEcdsaEccNistP384;
    }
    // No mode set, unsupported
    return pdk::spdm::app::res::algorithms::BaseAsymSel::NotSupport;
}

pdk::spdm::app::res::algorithms::BaseHashSel
select_base_hash_alg([[maybe_unused]] bool provided_tpm_alg_sha_256,
                     bool                  provided_tpm_alg_sha_384,
                     [[maybe_unused]] bool provided_tpm_alg_sha_512,
                     [[maybe_unused]] bool provided_tpm_alg_sha3_256,
                     [[maybe_unused]] bool provided_tpm_alg_sha3_384,
                     [[maybe_unused]] bool provided_tpm_alg_sha3_512)
{
    // Only support SHA384
    if (provided_tpm_alg_sha_384) {
        return pdk::spdm::app::res::algorithms::BaseHashSel::TpmAlgSha384;
    }
    // No mode set, unsupported
    return pdk::spdm::app::res::algorithms::BaseHashSel::NotSupport;
}

pdk::spdm::app::res::algorithms::DheAlgSupported
select_dhe_alg([[maybe_unused]] bool provided_secp521r1,
               [[maybe_unused]] bool provided_secp384r1,
               [[maybe_unused]] bool provided_secp256r1,
               [[maybe_unused]] bool provided_ffdhe4096,
               [[maybe_unused]] bool provided_ffdhe3072,
               [[maybe_unused]] bool provided_ffdhe2048)
{
    // don't support key exchange
    return pdk::spdm::app::res::algorithms::DheAlgSupported::NotSupport;
}

pdk::spdm::app::res::algorithms::AeadAlgSupported
select_aead_alg([[maybe_unused]] bool provided_chacha20_poly1305,
                [[maybe_unused]] bool provided_aes_256_gcm,
                [[maybe_unused]] bool provided_aes_128_gcm)
{
    //  doesn't support key exchange
    return pdk::spdm::app::res::algorithms::AeadAlgSupported::NotSupport;
}

pdk::spdm::app::res::algorithms::ReqBaseAsymAlgSupported
select_req_base_asym_alg([[maybe_unused]] bool provided_tpm_alg_ecdsa_ecc_nist_p384,
                         [[maybe_unused]] bool provided_tpm_alg_rsapss_4096,
                         [[maybe_unused]] bool provided_tpm_alg_rsassa_4096,
                         [[maybe_unused]] bool provided_tpm_alg_ecdsa_ecc_nist_p256,
                         [[maybe_unused]] bool provided_tpm_alg_rsapss_3072,
                         [[maybe_unused]] bool provided_tpm_alg_rsassa_3072,
                         [[maybe_unused]] bool provided_tpm_alg_rsapss_2048,
                         [[maybe_unused]] bool provided_tpm_alg_rsassa_2048,
                         [[maybe_unused]] bool provided_tpm_alg_ecdsa_ecc_nist_p521)
{
    // doesn't support key exchange
    return pdk::spdm::app::res::algorithms::ReqBaseAsymAlgSupported::NotSupport;
}

pdk::spdm::app::res::algorithms::KeyScheduleAlgSupported
select_key_schedule_alg([[maybe_unused]] bool provided_spdm_key_schedule)
{
    // doesn't support key exchange
    return pdk::spdm::app::res::algorithms::KeyScheduleAlgSupported::NotSupport;
}

}  // namespace pdk::spdm::platforms::res::algorithms