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

namespace nv::crypto {

enum class Status : uint8_t
{
    Success = 0,
    FailKeyWrapInvalidInput,
    FailKeyWrapSetAesKey,
    FailKeyWrapAesEncrypt,
    FailKeyUnwrapAesDecrypt,
    FailKeyUnwrapIntegrity,
};

using Kek256 = std::array<uint8_t, 32>;

// NIST SP800-38F KW (RFC 3394) wrap/unwrap with a 256-bit AES KEK.
// Same algorithm on every platform; on mcxn556 the underlying mbedtls AES
// calls are ELS-accelerated via aes_alt, on x86 they fall through to stock
// software / AES-NI. Byte output is identical on both.
//   - wrap:   plain.size() bytes in, out must be plain.size() + 8 bytes
//             (default 0xA6 ICV prepended). plain.size() must be a multiple
//             of 8 and >= 16.
//   - unwrap: in.size() bytes in, plain_out must be in.size() - 8 bytes.
//             in.size() must be a multiple of 8 and >= 24. ICV is verified.
// In-place operation is supported: plain.data() == out.data() for wrap and
// in.data() == plain_out.data() for unwrap are both safe.
Status nist_sp800_38f_kw_wrap(const Kek256&            kek,
                              std::span<const uint8_t> plain,
                              std::span<uint8_t>       out);

Status nist_sp800_38f_kw_unwrap(const Kek256&            kek,
                                std::span<const uint8_t> in,
                                std::span<uint8_t>       plain_out);

}  // namespace nv::crypto
