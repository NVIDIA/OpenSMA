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

#include "mbedtls/aes.h"

namespace nv::crypto {

namespace {

// RFC 3394 / NIST SP 800-38F key-wrap constants.
constexpr size_t   SemiBlockBytes = 8u;
constexpr size_t   AesBlockBytes  = 16u;
constexpr size_t   IcvBytes       = SemiBlockBytes;
constexpr size_t   MinPlainBytes  = 2u * SemiBlockBytes;       // 16
constexpr size_t   MinCipherBytes = IcvBytes + MinPlainBytes;  // 24
constexpr uint32_t KwRoundCount   = 6u;
constexpr uint32_t Kek256Bits     = 256u;
constexpr uint32_t BitsPerByte    = 8u;
constexpr uint64_t ByteMask       = 0xFFu;
constexpr uint32_t TopByteShift   = static_cast<uint32_t>(sizeof(uint64_t) - 1u) * BitsPerByte;

// Upper bound on the semi-block count so the t = n*j + i computation in the
// main loops provably fits in uint64_t (n * (KwRoundCount + 1) << 2^64).
// 1<<20 semi-blocks = 8 MiB plaintext, far above any realistic key payload
// and small enough to fit in size_t on a 32-bit target.
constexpr size_t MaxSemiBlocks = static_cast<size_t>(1) << 20u;

constexpr std::array<uint8_t, IcvBytes> Rfc3394Iv = {
    0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6};

}  // namespace

Status nist_sp800_38f_kw_wrap(const Kek256&            kek,
                              std::span<const uint8_t> plain,
                              std::span<uint8_t>       out)
{
    if (plain.size() < MinPlainBytes || (plain.size() % SemiBlockBytes) != 0u) {
        return Status::FailKeyWrapInvalidInput;
    }
    if (out.size() != plain.size() + IcvBytes) {
        return Status::FailKeyWrapInvalidInput;
    }
    const size_t n = plain.size() / SemiBlockBytes;
    if (n > MaxSemiBlocks) {
        return Status::FailKeyWrapInvalidInput;
    }

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int ret = mbedtls_aes_setkey_enc(&ctx, kek.data(), Kek256Bits);
    if (ret != 0) {
        mbedtls_aes_free(&ctx);
        return Status::FailKeyWrapSetAesKey;
    }

    // Initialize working buffer: out = ICV || P[1..n]. Use memmove for the
    // plaintext copy so callers may pass plain.data() == out.data() (in-place
    // wrap); the ICV write must come *after* the move so it doesn't clobber
    // the first semi-block of the source before we read it.
    std::memmove(out.data() + IcvBytes, plain.data(), plain.size());
    std::memcpy(out.data(), Rfc3394Iv.data(), IcvBytes);

    std::array<uint8_t, AesBlockBytes> blk{};
    std::array<uint8_t, AesBlockBytes> b_buf{};
    for (uint32_t j = 0u; j < KwRoundCount; j++) {
        for (uint32_t i = 1u; i <= n; i++) {
            std::memcpy(blk.data(), out.data(), SemiBlockBytes);  // A
            std::memcpy(blk.data() + SemiBlockBytes,
                        out.data() + (i * SemiBlockBytes),
                        SemiBlockBytes);  // R[i]
            ret = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, blk.data(), b_buf.data());
            if (ret != 0) {
                mbedtls_aes_free(&ctx);
                return Status::FailKeyWrapAesEncrypt;
            }
            const uint64_t t = (static_cast<uint64_t>(n) * static_cast<uint64_t>(j))
                             + static_cast<uint64_t>(i);
            // k < 8; out (>= 24 B by checks above) and b_buf (16 B) both
            // cover [0, 8) — subscripts are in range, no tainted index.
            for (uint32_t k = 0u; k < SemiBlockBytes; k++) {
                const uint64_t t_byte = (t >> (TopByteShift - (k * BitsPerByte))) & ByteMask;
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                out[k] = static_cast<uint8_t>((static_cast<uint64_t>(b_buf[k]) ^ t_byte)
                                              & ByteMask);
            }
            std::memcpy(out.data() + (i * SemiBlockBytes),
                        b_buf.data() + SemiBlockBytes,
                        SemiBlockBytes);
        }
    }
    mbedtls_aes_free(&ctx);
    return Status::Success;
}

Status nist_sp800_38f_kw_unwrap(const Kek256&            kek,
                                std::span<const uint8_t> in,
                                std::span<uint8_t>       plain_out)
{
    if (in.size() < MinCipherBytes || (in.size() % SemiBlockBytes) != 0u) {
        return Status::FailKeyWrapInvalidInput;
    }
    if (plain_out.size() != in.size() - IcvBytes) {
        return Status::FailKeyWrapInvalidInput;
    }
    const size_t n = (in.size() / SemiBlockBytes) - 1u;
    if (n > MaxSemiBlocks) {
        return Status::FailKeyWrapInvalidInput;
    }

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int ret = mbedtls_aes_setkey_dec(&ctx, kek.data(), Kek256Bits);
    if (ret != 0) {
        mbedtls_aes_free(&ctx);
        return Status::FailKeyWrapSetAesKey;
    }

    // a_buf is the 8-byte register; R[1..n] lives in plain_out throughout.
    // Save A first (so an in.data() == plain_out.data() caller can't clobber
    // the IV via the bulk move below), then memmove the ciphertext blocks
    // into place.
    std::array<uint8_t, SemiBlockBytes> a_buf{};
    std::memcpy(a_buf.data(), in.data(), SemiBlockBytes);
    std::memmove(plain_out.data(), in.data() + SemiBlockBytes, in.size() - SemiBlockBytes);

    std::array<uint8_t, AesBlockBytes> blk{};
    std::array<uint8_t, AesBlockBytes> b_buf{};
    // Reverse-iterate j over [0, KwRoundCount) using the unsigned post-decrement
    // idiom so all loop arithmetic stays in size_t (avoids signed/unsigned mix
    // and the int-to-size_t cast Coverity flagged).
    for (size_t j = KwRoundCount; j-- > 0u;) {
        for (size_t i = n; i > 0u; i--) {
            // All factors fit in uint32; cast both to uint64_t so the
            // multiply-then-add is performed in 64-bit unsigned end-to-end
            // and Coverity can see it can't wrap.
            // n is bounded by MaxSemiBlocks above; t = n*j + i provably fits
            // in uint64_t with all-uint64 operands.
            const uint64_t t = (static_cast<uint64_t>(n) * static_cast<uint64_t>(j))
                             + static_cast<uint64_t>(i);
            // k < 8; a_buf (8 B) and blk (16 B) both cover [0, 8) —
            // subscripts are in range, no tainted index.
            for (uint32_t k = 0u; k < SemiBlockBytes; k++) {
                const uint64_t t_byte = (t >> (TopByteShift - (k * BitsPerByte))) & ByteMask;
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                blk[k] = static_cast<uint8_t>((static_cast<uint64_t>(a_buf[k]) ^ t_byte)
                                              & ByteMask);
            }
            std::memcpy(blk.data() + SemiBlockBytes,
                        plain_out.data() + ((i - 1u) * SemiBlockBytes),
                        SemiBlockBytes);
            ret = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, blk.data(), b_buf.data());
            if (ret != 0) {
                std::memset(plain_out.data(), 0, plain_out.size());
                mbedtls_aes_free(&ctx);
                return Status::FailKeyUnwrapAesDecrypt;
            }
            std::memcpy(a_buf.data(), b_buf.data(), SemiBlockBytes);
            std::memcpy(plain_out.data() + ((i - 1u) * SemiBlockBytes),
                        b_buf.data() + SemiBlockBytes,
                        SemiBlockBytes);
        }
    }
    mbedtls_aes_free(&ctx);

    if (std::memcmp(a_buf.data(), Rfc3394Iv.data(), SemiBlockBytes) != 0) {
        std::memset(plain_out.data(), 0, plain_out.size());
        return Status::FailKeyUnwrapIntegrity;
    }
    return Status::Success;
}

}  // namespace nv::crypto
