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
#include "corepdk/modules/spdm/src/app/crypto/hash/pdk-spdm-app-res-crypto-hash-plat.h"
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-memory-allocate-plat.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "nv/logger/log.h"
#include <bit>
#include <cstring>
namespace pdk::spdm::platforms::res::crypto::hash {

void* sha384_new()
{
    void* ptr = pdk::spdm::platforms::res::memory_allocate::allocate(
        sizeof(mbedtls_sha512_context));
    if (ptr == nullptr) {
        return nullptr;
    }
    return ptr;
}
void sha384_free(void* ptr)
{
    if (ptr == nullptr) {
        return;
    }
    pdk::spdm::platforms::res::memory_allocate::deallocate(ptr);
}
bool sha384_init(void* ptr)
{
    constexpr int UsingSha384 = 1;
    if (ptr == nullptr) {
        return false;
    }
    auto                   ctx = std::bit_cast<mbedtls_sha512_context*>(ptr);
    mbedtls_sha512_context ctx_init{};
    std::memcpy(ctx, &ctx_init, sizeof(mbedtls_sha512_context));
    const int rc = mbedtls_sha512_starts_ret(ctx, UsingSha384);
    if (rc != 0) {
        return false;
    }
    return true;
}
bool sha384_duplicate(const void* ptr, void* new_ptr)
{
    if (ptr == nullptr || new_ptr == nullptr) {
        return false;
    }
    if (ptr == new_ptr) {
        return false;
    }
    auto ctx     = std::bit_cast<const mbedtls_sha512_context*>(ptr);
    auto new_ctx = std::bit_cast<mbedtls_sha512_context*>(new_ptr);
    mbedtls_sha512_clone(new_ctx, ctx);
    return true;
}
bool sha384_update(void* ptr, const void* data, size_t data_size)
{
    if (ptr == nullptr || data == nullptr) {
        return false;
    }
    auto      ctx = std::bit_cast<mbedtls_sha512_context*>(ptr);
    const int rc  = mbedtls_sha512_update_ret(
        ctx, std::bit_cast<const uint8_t*>(data), data_size);
    if (rc != 0) {
        return false;
    }
    return true;
}
bool sha384_final(void* ptr, uint8_t* hash_value)
{
    if (ptr == nullptr || hash_value == nullptr) {
        return false;
    }
    auto      ctx = std::bit_cast<mbedtls_sha512_context*>(ptr);
    const int rc  = mbedtls_sha512_finish_ret(ctx, hash_value);
    if (rc != 0) {
        return false;
    }
    return true;
}
bool sha384_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    if (data == nullptr || hash_value == nullptr) {
        return false;
    }
    constexpr int          UsingSha384 = 1;
    int                    rc          = 0;
    mbedtls_sha512_context ctx{};

    rc = mbedtls_sha512_starts_ret(&ctx, UsingSha384);
    if (rc != 0) {
        return false;
    }

    rc = mbedtls_sha512_update_ret(&ctx, std::bit_cast<const uint8_t*>(data), data_size);
    if (rc != 0) {
        return false;
    }

    rc = mbedtls_sha512_finish_ret(&ctx, hash_value);
    if (rc != 0) {
        return false;
    }
    return true;
}
}  // namespace pdk::spdm::platforms::res::crypto::hash

namespace pdk::spdm::platforms::res::crypto::hkdf {
bool hkdf_sha384_expand([[maybe_unused]] const uint8_t* prk,
                        [[maybe_unused]] size_t         prk_size,
                        [[maybe_unused]] const uint8_t* info,
                        [[maybe_unused]] size_t         info_size,
                        [[maybe_unused]] uint8_t*       out,
                        [[maybe_unused]] size_t         out_size)
{
    return false;
}
bool hkdf_sha384_extract([[maybe_unused]] const uint8_t* key,
                         [[maybe_unused]] size_t         key_size,
                         [[maybe_unused]] const uint8_t* salt,
                         [[maybe_unused]] size_t         salt_size,
                         [[maybe_unused]] uint8_t*       prk_out,
                         [[maybe_unused]] size_t         prk_out_size)
{
    return false;
}

}  // namespace pdk::spdm::platforms::res::crypto::hkdf