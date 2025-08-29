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
#include <stdint.h>

namespace pdk::spdm::app::res::algorithms {
enum class MeasurementHashAlgo : uint32_t
{
    NotSupport       = 0,
    RawBitStreamOnly = 1 << 0,
    TpmAlgSha256     = 1 << 1,
    TpmAlgSha384     = 1 << 2,
    TpmAlgSha512     = 1 << 3,
    TpmAlgSha3_256   = 1 << 4,
    TpmAlgSha3_384   = 1 << 5,
    TpmAlgSha3_512   = 1 << 6
};

struct HashAlgoDigestSize
{
    static constexpr size_t TpmAlgSha256Size   = 256 / 8;
    static constexpr size_t TpmAlgSha384Size   = 384 / 8;
    static constexpr size_t TpmAlgSha512Size   = 512 / 8;
    static constexpr size_t TpmAlgSha3_256Size = 256 / 8;
    static constexpr size_t TpmAlgSha3_384Size = 384 / 8;
    static constexpr size_t TpmAlgSha3_512Size = 512 / 8;
};

struct AsymAlgoSignatureSize
{
    static constexpr size_t TpmAlgRsassa2048Size       = 256;
    static constexpr size_t TpmAlgRsapss2048Size       = 256;
    static constexpr size_t TpmAlgRsassa3072Size       = 384;
    static constexpr size_t TpmAlgRsapss3072Size       = 384;
    static constexpr size_t TpmAlgEcdsaEccNistP256Size = 64;
    static constexpr size_t TpmAlgRsassa4096Size       = 512;
    static constexpr size_t TpmAlgRsapss4096Size       = 512;
    static constexpr size_t TpmAlgEcdsaEccNistP384Size = 96;
    static constexpr size_t TpmAlgEcdsaEccNistP521Size = 132;
};

enum class BaseAsymSel : uint32_t
{
    NotSupport             = 0,
    TpmAlgRsassa2048       = 1 << 0,
    TpmAlgRsapss2048       = 1 << 1,
    TpmAlgRsassa3072       = 1 << 2,
    TpmAlgRsapss3072       = 1 << 3,
    TpmAlgEcdsaEccNistP256 = 1 << 4,
    TpmAlgRsassa4096       = 1 << 5,
    TpmAlgRsapss4096       = 1 << 6,
    TpmAlgEcdsaEccNistP384 = 1 << 7,
    TpmAlgEcdsaEccNistP521 = 1 << 8,
};

enum class BaseHashSel : uint32_t
{
    NotSupport     = 0,
    TpmAlgSha256   = 1 << 0,
    TpmAlgSha384   = 1 << 1,
    TpmAlgSha512   = 1 << 2,
    TpmAlgSha3_256 = 1 << 3,
    TpmAlgSha3_384 = 1 << 4,
    TpmAlgSha3_512 = 1 << 5
};

}  // namespace pdk::spdm::app::res::algorithms