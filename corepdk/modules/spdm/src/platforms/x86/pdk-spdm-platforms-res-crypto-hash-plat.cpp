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
#include <array>
#include <cstdlib>
#include <cstring>

#include "crypto/hash/pdk-spdm-app-res-crypto-hash-plat.h"
#include "pdk-spdm-app-res-memory-allocate-plat.h"
namespace pdk::spdm::platforms::res::crypto::hash {
using SHA384_CTX = std::array<uint8_t, 64>;

// NOLINTBEGIN
void* sha384_new()
{
    SHA384_CTX* ptr = (SHA384_CTX*)malloc(sizeof(SHA384_CTX));
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
    free(ptr);
}
bool sha384_init(void* ptr)
{
    if (ptr == nullptr) {
        return false;
    }
    SHA384_CTX* ctx = (SHA384_CTX*)ptr;
    memset(ctx, 0, sizeof(SHA384_CTX));
    return true;
}
bool sha384_duplicate(const void* ptr, void* new_ptr)
{
    if (ptr == nullptr || new_ptr == nullptr) {
        return false;
    }
    const SHA384_CTX* ctx     = (const SHA384_CTX*)ptr;
    SHA384_CTX*       new_ctx = (SHA384_CTX*)new_ptr;
    memcpy(new_ctx, ctx, sizeof(SHA384_CTX));
    return true;
}
bool sha384_update(void* ptr, const void* data, size_t data_size)
{
    // TODO: implement
    return true;
}
bool sha384_final(void* ptr, uint8_t* hash_value)
{
    // TODO: implement
    return true;
}
bool sha384_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    // TODO: implement
    return true;
}
}  // namespace pdk::spdm::platforms::res::crypto::hash

namespace pdk::spdm::platforms::res::crypto::hkdf {
bool hkdf_sha384_expand(const uint8_t* prk,
                        size_t         prk_size,
                        const uint8_t* info,
                        size_t         info_size,
                        uint8_t*       out,
                        size_t         out_size)
{
    // TODO: implement
    return false;
}
bool hkdf_sha384_extract(const uint8_t* key,
                         size_t         key_size,
                         const uint8_t* salt,
                         size_t         salt_size,
                         uint8_t*       prk_out,
                         size_t         prk_out_size)
{
    // TODO: implement
    return false;
}
// NOLINTEND

}  // namespace pdk::spdm::platforms::res::crypto::hkdf