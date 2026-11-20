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

// Numeric build input. The certificate field is always emitted as five ASCII
// digits, so 1004 becomes "01004".
#ifndef FMC_ORDINAL_NUMBER
#define FMC_ORDINAL_NUMBER 4
#endif

#define NVIDIA_FMC_V4_ORDINAL_NUMBER  4
#define OPENSMA_FMC_V4_ORDINAL_NUMBER 1004
#define FMC_ORDINAL_NUMBER_MIN        1
#define NVIDIA_FMC_ORDINAL_MAX        1000
#define FMC_ORDINAL_NUMBER_MAX        99999

#if FMC_ORDINAL_NUMBER < FMC_ORDINAL_NUMBER_MIN || FMC_ORDINAL_NUMBER > FMC_ORDINAL_NUMBER_MAX
#error "FMC_ORDINAL_NUMBER must fit in the five-digit certificate field (1..99999)"
#elif FMC_ORDINAL_NUMBER > NVIDIA_FMC_ORDINAL_MAX
#define FMC_BUILD_USES_OPEN_SMA_PROFILE 1
#else
#define FMC_BUILD_USES_OPEN_SMA_PROFILE 0
#endif

namespace nv::spdm::cert {

enum class FmcCertificateProfile : uint8_t
{
    LegacyV3,
    NvidiaV4,
    OpenSmaV4,
    Unsupported,
};

inline constexpr std::array<uint8_t, 5> NvidiaFmcV4Ordinal{'0', '0', '0', '0', '4'};
inline constexpr std::array<uint8_t, 5> OpenSmaFmcV4Ordinal{'0', '1', '0', '0', '4'};
inline constexpr std::array<uint8_t, 5> FmcOrdinalMin{'0', '0', '0', '0', '1'};
inline constexpr std::array<uint8_t, 5> NvidiaFmcOrdinalMax{'0', '1', '0', '0', '0'};
inline constexpr uint32_t               OpenSmaV4AkTemplateSize = 985u;

constexpr bool build_uses_open_sma_profile(uint32_t ordinal)
{
    return ordinal > NVIDIA_FMC_ORDINAL_MAX;
}

constexpr std::array<uint8_t, 5> fmc_ordinal_to_ascii(uint32_t ordinal)
{
    std::array<uint8_t, 5> result{};
    for (std::size_t i = result.size(); i > 0; --i) {
        result[i - 1]  = static_cast<uint8_t>('0' + ordinal % 10u);
        ordinal       /= 10u;
    }
    return result;
}

constexpr FmcCertificateProfile
certificate_profile_from_fmc_ordinal(const std::array<uint8_t, 5>& ordinal)
{
    for (const auto digit : ordinal) {
        if (digit < static_cast<uint8_t>('0') || digit > static_cast<uint8_t>('9')) {
            return FmcCertificateProfile::Unsupported;
        }
    }
    if (ordinal < FmcOrdinalMin) {
        return FmcCertificateProfile::Unsupported;
    }
    if (ordinal < NvidiaFmcV4Ordinal) {
        return FmcCertificateProfile::LegacyV3;
    }
    if (ordinal > NvidiaFmcOrdinalMax) {
        return FmcCertificateProfile::OpenSmaV4;
    }
    return FmcCertificateProfile::NvidiaV4;
}

constexpr FmcCertificateProfile certificate_profile_from_fmc_numeric_ordinal(uint32_t ordinal)
{
    if (ordinal < FMC_ORDINAL_NUMBER_MIN || ordinal > FMC_ORDINAL_NUMBER_MAX) {
        return FmcCertificateProfile::Unsupported;
    }
    if (ordinal < NVIDIA_FMC_V4_ORDINAL_NUMBER) {
        return FmcCertificateProfile::LegacyV3;
    }
    if (ordinal > NVIDIA_FMC_ORDINAL_MAX) {
        return FmcCertificateProfile::OpenSmaV4;
    }
    return FmcCertificateProfile::NvidiaV4;
}

inline constexpr auto CompiledFmcOrdinal = fmc_ordinal_to_ascii(FMC_ORDINAL_NUMBER);
static_assert(FMC_BUILD_USES_OPEN_SMA_PROFILE
              == build_uses_open_sma_profile(FMC_ORDINAL_NUMBER));
static_assert(fmc_ordinal_to_ascii(4u) == NvidiaFmcV4Ordinal);
static_assert(fmc_ordinal_to_ascii(1004u) == OpenSmaFmcV4Ordinal);
static_assert(certificate_profile_from_fmc_numeric_ordinal(0u)
              == FmcCertificateProfile::Unsupported);
static_assert(certificate_profile_from_fmc_numeric_ordinal(3u)
              == FmcCertificateProfile::LegacyV3);
static_assert(certificate_profile_from_fmc_numeric_ordinal(4u)
              == FmcCertificateProfile::NvidiaV4);
static_assert(certificate_profile_from_fmc_numeric_ordinal(1004u)
              == FmcCertificateProfile::OpenSmaV4);
static_assert(certificate_profile_from_fmc_numeric_ordinal(5u)
              == FmcCertificateProfile::NvidiaV4);
static_assert(certificate_profile_from_fmc_numeric_ordinal(1000u)
              == FmcCertificateProfile::NvidiaV4);
static_assert(certificate_profile_from_fmc_numeric_ordinal(1001u)
              == FmcCertificateProfile::OpenSmaV4);

}  // namespace nv::spdm::cert
