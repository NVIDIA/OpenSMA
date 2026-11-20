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

#include "nv/spdm/cert_profile.h"

#include "nv/ut/unittest.h"

#include <array>

using namespace nv::ut;

// UBS discovers tests by matching them to an existing production .cpp file.

TEST(CertProfile, MapsSupportedNumericOrdinals)
{
    using nv::spdm::cert::certificate_profile_from_fmc_numeric_ordinal;
    using nv::spdm::cert::FmcCertificateProfile;

    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(0u),
                  FmcCertificateProfile::Unsupported);
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(1u),
                  FmcCertificateProfile::LegacyV3);
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(3u),
                  FmcCertificateProfile::LegacyV3);
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(4u),
                  FmcCertificateProfile::NvidiaV4);
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(1004u),
                  FmcCertificateProfile::OpenSmaV4);
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(5u),
                  FmcCertificateProfile::NvidiaV4);
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(1005u),
                  FmcCertificateProfile::OpenSmaV4);
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(101004u),
                  FmcCertificateProfile::Unsupported);
};

TEST(CertProfile, MapsSupportedAsciiOrdinals)
{
    using nv::spdm::cert::certificate_profile_from_fmc_ordinal;
    using nv::spdm::cert::FmcCertificateProfile;

    constexpr std::array<uint8_t, 5> UnsupportedOrdinal{'0', '0', '0', '0', '0'};
    constexpr std::array<uint8_t, 5> FirstLegacyOrdinal{'0', '0', '0', '0', '1'};
    constexpr std::array<uint8_t, 5> LegacyOrdinal{'0', '0', '0', '0', '3'};
    constexpr std::array<uint8_t, 5> NvidiaOrdinal{'0', '0', '0', '0', '5'};
    constexpr std::array<uint8_t, 5> OpenSmaOrdinal{'0', '1', '0', '0', '5'};

    ensure::is_eq(certificate_profile_from_fmc_ordinal(UnsupportedOrdinal),
                  FmcCertificateProfile::Unsupported);
    ensure::is_eq(certificate_profile_from_fmc_ordinal(FirstLegacyOrdinal),
                  FmcCertificateProfile::LegacyV3);
    ensure::is_eq(certificate_profile_from_fmc_ordinal(LegacyOrdinal),
                  FmcCertificateProfile::LegacyV3);
    ensure::is_eq(certificate_profile_from_fmc_ordinal(nv::spdm::cert::NvidiaFmcV4Ordinal),
                  FmcCertificateProfile::NvidiaV4);
    ensure::is_eq(certificate_profile_from_fmc_ordinal(nv::spdm::cert::OpenSmaFmcV4Ordinal),
                  FmcCertificateProfile::OpenSmaV4);
    ensure::is_eq(certificate_profile_from_fmc_ordinal(NvidiaOrdinal),
                  FmcCertificateProfile::NvidiaV4);
    ensure::is_eq(certificate_profile_from_fmc_ordinal(OpenSmaOrdinal),
                  FmcCertificateProfile::OpenSmaV4);
};

TEST(CertProfile, UsesSameBoundaryForBuildAndRuntime)
{
    using nv::spdm::cert::build_uses_open_sma_profile;
    using nv::spdm::cert::certificate_profile_from_fmc_numeric_ordinal;
    using nv::spdm::cert::FmcCertificateProfile;

    ensure::is_false(build_uses_open_sma_profile(1000u));
    ensure::is_true(build_uses_open_sma_profile(1001u));
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(1000u),
                  FmcCertificateProfile::NvidiaV4);
    ensure::is_eq(certificate_profile_from_fmc_numeric_ordinal(1001u),
                  FmcCertificateProfile::OpenSmaV4);
};

TEST(CertProfile, RejectsMalformedAsciiOrdinal)
{
    constexpr std::array<uint8_t, 5> NonDecimalOrdinal{'0', '0', '0', 'A', '4'};
    constexpr std::array<uint8_t, 5> EmbeddedNullOrdinal{'0', '0', '\0', '0', '4'};

    ensure::is_eq(nv::spdm::cert::certificate_profile_from_fmc_ordinal(NonDecimalOrdinal),
                  nv::spdm::cert::FmcCertificateProfile::Unsupported);
    ensure::is_eq(nv::spdm::cert::certificate_profile_from_fmc_ordinal(EmbeddedNullOrdinal),
                  nv::spdm::cert::FmcCertificateProfile::Unsupported);
};
