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
#include "nv/crypto/key_wrap.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <span>

#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;
using nv::crypto::Kek256;
using nv::crypto::Status;

namespace {

// RFC 3394 test KEK (shared by §4.3 and §4.6).
constexpr Kek256 kRfc3394Kek = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
                                0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                                0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};

// RFC 3394 §4.3 — wrap 128 bits of key data with a 256-bit KEK.
constexpr std::array<uint8_t, 16> kRfc3394_4_3_Plain = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
constexpr std::array<uint8_t, 24> kRfc3394_4_3_Cipher = {
    0x64, 0xE8, 0xC3, 0xF9, 0xCE, 0x0F, 0x5B, 0xA2, 0x63, 0xE9, 0x77, 0x79,
    0x05, 0x81, 0x8A, 0x2A, 0x93, 0xC8, 0x19, 0x1E, 0x7D, 0x6E, 0x8A, 0xE7};

// RFC 3394 §4.6 — wrap 256 bits of key data with a 256-bit KEK.
constexpr std::array<uint8_t, 32> kRfc3394_4_6_Plain = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
constexpr std::array<uint8_t, 40> kRfc3394_4_6_Cipher = {
    0x28, 0xC9, 0xF4, 0x04, 0xC4, 0xB8, 0x10, 0xF4, 0xCB, 0xCC, 0xB3, 0x5C, 0xFB, 0x87,
    0xF8, 0x26, 0x3F, 0x57, 0x86, 0xE2, 0xD8, 0x0E, 0xD3, 0x26, 0xCB, 0xC7, 0xF0, 0xE7,
    0x1A, 0x99, 0xF4, 0x3B, 0xFB, 0x98, 0x8B, 0x9B, 0x7A, 0x02, 0xDD, 0x21};

template<size_t N>
bool bytes_eq(std::span<const uint8_t> a, const std::array<uint8_t, N>& b)
{
    return a.size() == N && std::memcmp(a.data(), b.data(), N) == 0;
}

}  // namespace

TEST(KeyWrap, rfc3394_4_3_wrap_kat)
{
    std::array<uint8_t, 24> out{};
    const Status            s = crypto::nist_sp800_38f_kw_wrap(kRfc3394Kek, kRfc3394_4_3_Plain, out);
    ensure::is_eq(s, Status::Success);
    ensure::is_true(bytes_eq(out, kRfc3394_4_3_Cipher));
};

TEST(KeyWrap, rfc3394_4_3_unwrap_kat)
{
    std::array<uint8_t, 16> plain_out{};
    const Status            s =
        crypto::nist_sp800_38f_kw_unwrap(kRfc3394Kek, kRfc3394_4_3_Cipher, plain_out);
    ensure::is_eq(s, Status::Success);
    ensure::is_true(bytes_eq(plain_out, kRfc3394_4_3_Plain));
};

TEST(KeyWrap, rfc3394_4_6_wrap_kat)
{
    std::array<uint8_t, 40> out{};
    const Status            s = crypto::nist_sp800_38f_kw_wrap(kRfc3394Kek, kRfc3394_4_6_Plain, out);
    ensure::is_eq(s, Status::Success);
    ensure::is_true(bytes_eq(out, kRfc3394_4_6_Cipher));
};

TEST(KeyWrap, rfc3394_4_6_unwrap_kat)
{
    std::array<uint8_t, 32> plain_out{};
    const Status            s =
        crypto::nist_sp800_38f_kw_unwrap(kRfc3394Kek, kRfc3394_4_6_Cipher, plain_out);
    ensure::is_eq(s, Status::Success);
    ensure::is_true(bytes_eq(plain_out, kRfc3394_4_6_Plain));
};

TEST(KeyWrap, round_trip)
{
    std::array<uint8_t, 40> wrapped{};
    std::array<uint8_t, 32> recovered{};
    ensure::is_eq(crypto::nist_sp800_38f_kw_wrap(kRfc3394Kek, kRfc3394_4_6_Plain, wrapped),
                  Status::Success);
    ensure::is_eq(crypto::nist_sp800_38f_kw_unwrap(kRfc3394Kek, wrapped, recovered),
                  Status::Success);
    ensure::is_true(bytes_eq(recovered, kRfc3394_4_6_Plain));
};

TEST(KeyWrap, in_place_wrap)
{
    // Buffer is sized for the wrapped output (plain.size() + 8). The first
    // plain.size() bytes hold the plaintext input; after wrap the whole
    // buffer holds the ciphertext.
    std::array<uint8_t, 40> buf{};
    std::memcpy(buf.data(), kRfc3394_4_6_Plain.data(), kRfc3394_4_6_Plain.size());
    const std::span<const uint8_t> plain{buf.data(), kRfc3394_4_6_Plain.size()};
    const std::span<uint8_t>       out{buf};
    ensure::is_eq(crypto::nist_sp800_38f_kw_wrap(kRfc3394Kek, plain, out), Status::Success);
    ensure::is_true(bytes_eq(buf, kRfc3394_4_6_Cipher));
};

TEST(KeyWrap, in_place_unwrap)
{
    std::array<uint8_t, 40> buf{};
    std::memcpy(buf.data(), kRfc3394_4_6_Cipher.data(), kRfc3394_4_6_Cipher.size());
    const std::span<const uint8_t> in{buf};
    const std::span<uint8_t>       plain_out{buf.data(), kRfc3394_4_6_Plain.size()};
    ensure::is_eq(crypto::nist_sp800_38f_kw_unwrap(kRfc3394Kek, in, plain_out), Status::Success);
    ensure::is_true(bytes_eq(std::span<const uint8_t>{buf.data(), kRfc3394_4_6_Plain.size()},
                             kRfc3394_4_6_Plain));
};

TEST(KeyWrap, unwrap_tampered_icv_zeroizes_output)
{
    // Flip a bit in the first ciphertext byte (the wrapped IV) so the
    // integrity check at the end of unwrap fails.
    std::array<uint8_t, 40> tampered = kRfc3394_4_6_Cipher;
    tampered[0] ^= 0x01;

    std::array<uint8_t, 32> plain_out{};
    std::memset(plain_out.data(), 0xCC, plain_out.size());  // sentinel

    const Status s = crypto::nist_sp800_38f_kw_unwrap(kRfc3394Kek, tampered, plain_out);
    ensure::is_eq(s, Status::FailKeyUnwrapIntegrity);
    // Output buffer must be zeroized on integrity failure (no plaintext leak).
    for (uint8_t b : plain_out) {
        ensure::is_eq(b, static_cast<uint8_t>(0));
    }
};

TEST(KeyWrap, wrap_rejects_invalid_sizes)
{
    std::array<uint8_t, 40> out{};

    // Plaintext too short (must be >= 16).
    std::array<uint8_t, 8> too_short{};
    ensure::is_eq(crypto::nist_sp800_38f_kw_wrap(kRfc3394Kek, too_short, out),
                  Status::FailKeyWrapInvalidInput);

    // Plaintext not a multiple of 8.
    std::array<uint8_t, 17> unaligned{};
    ensure::is_eq(crypto::nist_sp800_38f_kw_wrap(kRfc3394Kek, unaligned, out),
                  Status::FailKeyWrapInvalidInput);

    // Output size != plain.size() + 8.
    std::array<uint8_t, 39> undersized_out{};
    ensure::is_eq(crypto::nist_sp800_38f_kw_wrap(kRfc3394Kek, kRfc3394_4_6_Plain, undersized_out),
                  Status::FailKeyWrapInvalidInput);
};

TEST(KeyWrap, unwrap_rejects_invalid_sizes)
{
    std::array<uint8_t, 32> plain_out{};

    // Ciphertext too short (must be >= 24).
    std::array<uint8_t, 16> too_short{};
    ensure::is_eq(crypto::nist_sp800_38f_kw_unwrap(kRfc3394Kek, too_short, plain_out),
                  Status::FailKeyWrapInvalidInput);

    // Ciphertext not a multiple of 8.
    std::array<uint8_t, 25> unaligned{};
    ensure::is_eq(crypto::nist_sp800_38f_kw_unwrap(kRfc3394Kek, unaligned, plain_out),
                  Status::FailKeyWrapInvalidInput);

    // Output size != in.size() - 8.
    std::array<uint8_t, 31> undersized_out{};
    ensure::is_eq(crypto::nist_sp800_38f_kw_unwrap(kRfc3394Kek, kRfc3394_4_6_Cipher, undersized_out),
                  Status::FailKeyWrapInvalidInput);
};
