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
#include <cstring>
#include <span>

#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;
using nv::crypto::Aes256Key;
using nv::crypto::AesGcmIv;
using nv::crypto::AesGcmTag;
using nv::spdm::crypto::CryptoStatus;

namespace {

constexpr Aes256Key               kZeroKey{};
constexpr AesGcmIv                kZeroIv{};
constexpr AesGcmTag               kAes256ZeroBlockTag = {0xd0,
                                                         0xd1,
                                                         0xc8,
                                                         0xa7,
                                                         0x99,
                                                         0x99,
                                                         0x6b,
                                                         0xf0,
                                                         0x26,
                                                         0x5b,
                                                         0x98,
                                                         0xb5,
                                                         0xd4,
                                                         0x8a,
                                                         0xb9,
                                                         0x19};
constexpr std::array<uint8_t, 16> kZeroPlain{};
constexpr std::array<uint8_t, 16> kAes256ZeroBlockCipher = {0xce,
                                                            0xa7,
                                                            0x40,
                                                            0x3d,
                                                            0x4d,
                                                            0x60,
                                                            0x6b,
                                                            0x6e,
                                                            0x07,
                                                            0x4e,
                                                            0xc5,
                                                            0xd3,
                                                            0xba,
                                                            0xf3,
                                                            0x9d,
                                                            0x18};

constexpr Aes256Key kRoundTripKey = {0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
                                     0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
                                     0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
                                     0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4};
constexpr AesGcmIv  kRoundTripIv  = {
    0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad, 0xde, 0xca, 0xf8, 0x88};
constexpr std::array<uint8_t, 20> kRoundTripAad   = {0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe,
                                                     0xef, 0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad,
                                                     0xbe, 0xef, 0xab, 0xad, 0xda, 0xd2};
constexpr std::array<uint8_t, 32> kRoundTripPlain = {
    0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5, 0xa5, 0x59, 0x09,
    0xc5, 0xaf, 0xf5, 0x26, 0x9a, 0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34,
    0xf7, 0xda, 0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72};
constexpr std::array<uint8_t, 32> kRoundTripCipher = {
    0x7e, 0x16, 0xda, 0x55, 0x17, 0xc2, 0xd7, 0x9e, 0x33, 0xc7, 0x73,
    0xbe, 0x78, 0x0d, 0xe9, 0x5a, 0x81, 0x4d, 0x89, 0x9d, 0x00, 0x78,
    0xcd, 0xcd, 0x1e, 0x78, 0x95, 0x76, 0xe7, 0x5e, 0x4b, 0xaf};
constexpr AesGcmTag kRoundTripTag = {0xa5,
                                     0x73,
                                     0xff,
                                     0x04,
                                     0x0f,
                                     0x42,
                                     0x59,
                                     0xed,
                                     0x36,
                                     0x5b,
                                     0x4f,
                                     0x7f,
                                     0x5c,
                                     0x50,
                                     0x25,
                                     0xfe};

template<size_t N>
bool bytes_eq(std::span<const uint8_t> a, const std::array<uint8_t, N>& b)
{
    return a.size() == N && std::memcmp(a.data(), b.data(), N) == 0;
}

}  // namespace

TEST(AesGcm, aes256_zero_block_encrypt_kat)
{
    std::array<uint8_t, 16> cipher{};
    AesGcmTag               tag{};

    const CryptoStatus s = crypto::aes_256_gcm_encrypt(
        kZeroKey, kZeroIv, {}, kZeroPlain, cipher, tag);

    ensure::is_eq(s, CryptoStatus::Success);
    ensure::is_true(bytes_eq(cipher, kAes256ZeroBlockCipher));
    ensure::is_true(bytes_eq(tag, kAes256ZeroBlockTag));
};

TEST(AesGcm, encrypt_with_aad)
{
    std::array<uint8_t, 32> cipher{};
    AesGcmTag               tag{};

    ensure::is_eq(crypto::aes_256_gcm_encrypt(
                      kRoundTripKey, kRoundTripIv, kRoundTripAad, kRoundTripPlain, cipher, tag),
                  CryptoStatus::Success);
    ensure::is_true(bytes_eq(cipher, kRoundTripCipher));
    ensure::is_true(bytes_eq(tag, kRoundTripTag));
};

TEST(AesGcm, in_place_encrypt)
{
    std::array<uint8_t, 16>        buf = kZeroPlain;
    AesGcmTag                      tag{};
    const std::span<const uint8_t> plain{buf};
    const std::span<uint8_t>       cipher{buf};

    ensure::is_eq(crypto::aes_256_gcm_encrypt(kZeroKey, kZeroIv, {}, plain, cipher, tag),
                  CryptoStatus::Success);
    ensure::is_true(bytes_eq(buf, kAes256ZeroBlockCipher));
    ensure::is_true(bytes_eq(tag, kAes256ZeroBlockTag));
};

TEST(AesGcm, rejects_mismatched_output_sizes)
{
    std::array<uint8_t, 15> short_cipher{};
    std::array<uint8_t, 16> cipher{};
    AesGcmTag               tag{};

    ensure::is_eq(
        crypto::aes_256_gcm_encrypt(kZeroKey, kZeroIv, {}, kZeroPlain, short_cipher, tag),
        CryptoStatus::FailAesGcmInvalidInput);

    const std::span<const uint8_t> partial_plain{kZeroPlain.data(), kZeroPlain.size()};
    const std::span<uint8_t>       partial_cipher{cipher.data() + 1u, cipher.size() - 1u};
    ensure::is_eq(
        crypto::aes_256_gcm_encrypt(kZeroKey, kZeroIv, {}, partial_plain, partial_cipher, tag),
        CryptoStatus::FailAesGcmInvalidInput);
};
