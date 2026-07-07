/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/crypto/aes_gcm.h"

#include <array>
#include <cstdint>
#include <functional>
#include <span>

#include "mbedtls/gcm.h"
#include "mbedtls/memory_buffer_alloc.h"
#include "nv/crypto/key_clear_guard.h"

namespace nv::crypto {

using nv::spdm::crypto::CryptoStatus;

namespace {

constexpr uint32_t Aes256KeyBits      = 256u;
constexpr size_t   MbedtlsBufferBytes = 1024u;

bool ranges_overlap(std::span<const uint8_t> lhs, std::span<const uint8_t> rhs)
{
    if (lhs.empty() || rhs.empty()) {
        return false;
    }

    const auto* lhs_begin = lhs.data();
    const auto* lhs_end   = lhs_begin + lhs.size();
    const auto* rhs_begin = rhs.data();
    const auto* rhs_end   = rhs_begin + rhs.size();
    const auto  less      = std::less<const uint8_t*>{};

    return less(lhs_begin, rhs_end) && less(rhs_begin, lhs_end);
}

}  // namespace

CryptoStatus aes_256_gcm_encrypt(const Aes256Key&                        key,
                                 std::span<const uint8_t, AesGcmIvBytes> iv,
                                 std::span<const uint8_t>                aad,
                                 std::span<const uint8_t>                plaintext,
                                 std::span<uint8_t>                      ciphertext,
                                 std::span<uint8_t, AesGcmTagBytes>      tag)
{
    if (ciphertext.size() != plaintext.size()) {
        return CryptoStatus::FailAesGcmInvalidInput;
    }
    if (ranges_overlap(plaintext, ciphertext) && plaintext.data() != ciphertext.data()) {
        return CryptoStatus::FailAesGcmInvalidInput;
    }

    std::array<uint8_t, MbedtlsBufferBytes> buffer_for_mbedtls{};
    const KeyClearGuard buffer_guard{std::span<uint8_t>(buffer_for_mbedtls)};
    mbedtls_memory_buffer_alloc_init(buffer_for_mbedtls.data(), buffer_for_mbedtls.size());

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key.data(), Aes256KeyBits);
    if (ret != 0) {
        mbedtls_gcm_free(&ctx);
        mbedtls_memory_buffer_alloc_free();
        return CryptoStatus::FailAesGcmSetKey;
    }

    ret = mbedtls_gcm_crypt_and_tag(&ctx,
                                    MBEDTLS_GCM_ENCRYPT,
                                    plaintext.size(),
                                    iv.data(),
                                    iv.size(),
                                    aad.data(),
                                    aad.size(),
                                    plaintext.data(),
                                    ciphertext.data(),
                                    tag.size(),
                                    tag.data());
    mbedtls_gcm_free(&ctx);
    mbedtls_memory_buffer_alloc_free();
    if (ret != 0) {
        return CryptoStatus::FailAesGcmEncrypt;
    }
    return CryptoStatus::Success;
}

}  // namespace nv::crypto
