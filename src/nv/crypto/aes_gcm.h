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
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "nv/spdm/crypto_status.h"

namespace nv::crypto {

inline constexpr size_t Aes256KeyBytes = 32u;
inline constexpr size_t AesGcmIvBytes  = 12u;
inline constexpr size_t AesGcmTagBytes = 16u;

using Aes256Key = std::array<uint8_t, Aes256KeyBytes>;
using AesGcmIv  = std::array<uint8_t, AesGcmIvBytes>;
using AesGcmTag = std::array<uint8_t, AesGcmTagBytes>;

// AES-256-GCM one-shot encrypt. The caller is responsible for nonce uniqueness
// for each key/IV pair. Encryption permits exact in-place operation
// (plaintext.data() == ciphertext.data()).
nv::spdm::crypto::CryptoStatus aes_256_gcm_encrypt(const Aes256Key&                        key,
                                                   std::span<const uint8_t, AesGcmIvBytes> iv,
                                                   std::span<const uint8_t>                aad,
                                                   std::span<const uint8_t> plaintext,
                                                   std::span<uint8_t>       ciphertext,
                                                   std::span<uint8_t, AesGcmTagBytes> tag);

}  // namespace nv::crypto
