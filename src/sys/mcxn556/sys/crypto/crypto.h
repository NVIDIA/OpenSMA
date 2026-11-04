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

#include <span>
#include <stdint.h>
#include "fsl_nboot.h"
#include "nv/crypto/aes_gcm.h"
#include "nv/crypto/ecdh_p384.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/fw_parser/fw_parser_mcu.h"
#include "sys/crypto/crypto.h"

namespace sys::crypto {

#ifdef CPU_MCXN547VDF
using auth_params_t = nboot_img_auth_ecdsa_parms_t;  // for 547
#else
using auth_params_t = nboot_img_auth_parms_t;  // for 556
#endif

nv::spdm::crypto::CryptoStatus
authenticate_firmware(const nv::fw_parser::mcu::ParsingFwType InputParseingFwType);

nv::spdm::crypto::CryptoStatus perform_image_auth(nboot_context_t& nbootCtx,
                                                  uint8_t*         imageAddress,
                                                  nboot_bool_t&    is_signature_verified,
                                                  auth_params_t&   parms);
nv::spdm::crypto::CryptoStatus perform_image_auth_svc(nboot_context_t& nbootCtx,
                                                      uint8_t*         imageAddress,
                                                      nboot_bool_t&    is_signature_verified,
                                                      auth_params_t&   parms);
nv::spdm::crypto::CryptoStatus perform_image_auth_impl(nboot_context_t& nbootCtx,
                                                       uint8_t*         imageAddress,
                                                       nboot_bool_t&    is_signature_verified,
                                                       auth_params_t&   parms);

nv::spdm::crypto::CryptoStatus trng_generate(std::span<uint8_t> output);
nv::spdm::crypto::CryptoStatus puf_wrap(std::span<const uint8_t> key,
                                        std::span<uint8_t>       wrapped_key);
nv::spdm::crypto::CryptoStatus puf_unwrap(std::span<const uint8_t> key_code,
                                          std::span<uint8_t>       key);
nv::spdm::crypto::CryptoStatus
aes_256_gcm_encrypt(const nv::crypto::Aes256Key&                        key,
                    std::span<const uint8_t, nv::crypto::AesGcmIvBytes> iv,
                    std::span<const uint8_t>                            aad,
                    std::span<const uint8_t>                            plaintext,
                    std::span<uint8_t>                                  ciphertext,
                    std::span<uint8_t, nv::crypto::AesGcmTagBytes>      tag);
nv::spdm::crypto::CryptoStatus
aes_256_gcm_decrypt(const nv::crypto::Aes256Key&                         key,
                    std::span<const uint8_t, nv::crypto::AesGcmIvBytes>  iv,
                    std::span<const uint8_t>                             aad,
                    std::span<const uint8_t>                             ciphertext,
                    std::span<const uint8_t, nv::crypto::AesGcmTagBytes> tag,
                    std::span<uint8_t>                                   plaintext);
nv::spdm::crypto::CryptoStatus
ecdh_p384_generate_key_pair(nv::crypto::EcdhP384KeyPair& key_pair);
nv::spdm::crypto::CryptoStatus
ecdh_p384_compute_shared_secret(const nv::crypto::EcdhP384PrivateKey& private_key,
                                const nv::crypto::EcdhP384PublicKey&  peer_public_key,
                                nv::crypto::EcdhP384SharedSecret&     shared_secret);
nv::spdm::crypto::CryptoStatus hkdf_sha384(std::span<const uint8_t> input_key_material,
                                           std::span<const uint8_t> salt,
                                           std::span<const uint8_t> info,
                                           std::span<uint8_t>       output);

}  // namespace sys::crypto
