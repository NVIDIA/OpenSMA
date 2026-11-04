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
#include "sys/crypto/crypto.h"

namespace sys::crypto {
#if defined(__cplusplus)
extern "C" {
#endif

NV_PRIVILEGED_FUNCTION bool Check_Receive_Sbfile_Capability_Priv()
{
    return true;
}

#if defined(__cplusplus)
}
#endif
nv::spdm::crypto::CryptoStatus authenticate_firmware(
    [[maybe_unused]] const nv::fw_parser::mcu::ParsingFwType InputParseingFwType)
{
    return nv::spdm::crypto::CryptoStatus::Success;
}

nv::spdm::crypto::CryptoStatus trng_generate([[maybe_unused]] std::span<uint8_t> output)
{
    return nv::spdm::crypto::CryptoStatus::FailRandomGen;
}

nv::spdm::crypto::CryptoStatus puf_wrap([[maybe_unused]] std::span<const uint8_t> key,
                                        [[maybe_unused]] std::span<uint8_t>       wrapped_key)
{
    return nv::spdm::crypto::CryptoStatus::FailPufFail;
}

nv::spdm::crypto::CryptoStatus puf_unwrap([[maybe_unused]] std::span<const uint8_t> key_code,
                                          [[maybe_unused]] std::span<uint8_t>       key)
{
    return nv::spdm::crypto::CryptoStatus::FailPufFail;
}

nv::spdm::crypto::CryptoStatus
aes_256_gcm_encrypt([[maybe_unused]] const nv::crypto::Aes256Key&                        key,
                    [[maybe_unused]] std::span<const uint8_t, nv::crypto::AesGcmIvBytes> iv,
                    [[maybe_unused]] std::span<const uint8_t>                            aad,
                    [[maybe_unused]] std::span<const uint8_t>                       plaintext,
                    [[maybe_unused]] std::span<uint8_t>                             ciphertext,
                    [[maybe_unused]] std::span<uint8_t, nv::crypto::AesGcmTagBytes> tag)
{
    return nv::spdm::crypto::CryptoStatus::FailAesGcmEncrypt;
}

nv::spdm::crypto::CryptoStatus
aes_256_gcm_decrypt([[maybe_unused]] const nv::crypto::Aes256Key&                        key,
                    [[maybe_unused]] std::span<const uint8_t, nv::crypto::AesGcmIvBytes> iv,
                    [[maybe_unused]] std::span<const uint8_t>                            aad,
                    [[maybe_unused]] std::span<const uint8_t> ciphertext,
                    [[maybe_unused]] std::span<const uint8_t, nv::crypto::AesGcmTagBytes> tag,
                    [[maybe_unused]] std::span<uint8_t> plaintext)
{
    return nv::spdm::crypto::CryptoStatus::FailAesGcmDecrypt;
}

nv::spdm::crypto::CryptoStatus
ecdh_p384_generate_key_pair([[maybe_unused]] nv::crypto::EcdhP384KeyPair& key_pair)
{
    return nv::spdm::crypto::CryptoStatus::FailEcdhKeyGeneration;
}

nv::spdm::crypto::CryptoStatus ecdh_p384_compute_shared_secret(
    [[maybe_unused]] const nv::crypto::EcdhP384PrivateKey& private_key,
    [[maybe_unused]] const nv::crypto::EcdhP384PublicKey&  peer_public_key,
    [[maybe_unused]] nv::crypto::EcdhP384SharedSecret&     shared_secret)
{
    return nv::spdm::crypto::CryptoStatus::FailEcdhKeyAgreement;
}

nv::spdm::crypto::CryptoStatus
hkdf_sha384([[maybe_unused]] std::span<const uint8_t> input_key_material,
            [[maybe_unused]] std::span<const uint8_t> salt,
            [[maybe_unused]] std::span<const uint8_t> info,
            [[maybe_unused]] std::span<uint8_t>       output)
{
    return nv::spdm::crypto::CryptoStatus::FailHkdf;
}

}  // namespace sys::crypto
