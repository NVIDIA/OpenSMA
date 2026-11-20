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
#include "nv/spdm/cert_library.h"
#include "nv/spdm/cert_profile.h"

#include <type_traits>

using namespace nv;

namespace nv::spdm::ik {

enum class DevIkCertificateProfile : uint8_t
{
    Legacy,
    EdgeLock2Go,
};

constexpr bool is_edgelock2go_dda_ordinal(uint32_t dda_ordinal_number)
{
    constexpr uint32_t DdaPrefixDivisor     = 1000;
    constexpr uint32_t EdgeLock2GoDdaPrefix = 1;
    return dda_ordinal_number / DdaPrefixDivisor == EdgeLock2GoDdaPrefix;
}

constexpr DevIkCertificateProfile certificate_profile_from_dda(uint32_t dda_ordinal_number)
{
    return is_edgelock2go_dda_ordinal(dda_ordinal_number) ? DevIkCertificateProfile::EdgeLock2Go
                                                          : DevIkCertificateProfile::Legacy;
}

struct __attribute__((packed)) LegacySerialFieldsV4
{
    std::array<uint8_t, 12> template_to_serial_number{
        0x30, 0x82, 0x02, 0x1B, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x11, 0x40};
    std::array<uint8_t, 16> serial_number{};
};

template<cert::FmcCertificateProfile FmcProfileValue>
struct __attribute__((packed)) EdgeLock2GoSerialFieldsV4Layout
{
    static_assert(FmcProfileValue == cert::FmcCertificateProfile::NvidiaV4
                  || FmcProfileValue == cert::FmcCertificateProfile::OpenSmaV4);
    static constexpr bool IsOpenSma = FmcProfileValue == cert::FmcCertificateProfile::OpenSmaV4;

    std::array<uint8_t, 11> template_to_serial_number{0x30,
                                                      0x82,
                                                      0x02,
                                                      IsOpenSma ? uint8_t{0x1B} : uint8_t{0x1A},
                                                      0xA0,
                                                      0x03,
                                                      0x02,
                                                      0x01,
                                                      0x02,
                                                      0x02,
                                                      0x10};
    std::array<uint8_t, 16> serial_number{};
};

using EdgeLock2GoSerialFieldsV4 = EdgeLock2GoSerialFieldsV4Layout<
    cert::FmcCertificateProfile::NvidiaV4>;
using OpenSmaEdgeLock2GoSerialFieldsV4 = EdgeLock2GoSerialFieldsV4Layout<
    cert::FmcCertificateProfile::OpenSmaV4>;

struct __attribute__((packed)) LegacySerialFieldsV3
{
    std::array<uint8_t, 12> template_to_serial_number{
        0x30, 0x82, 0x02, 0x1F, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x11, 0x40};
    std::array<uint8_t, 16> serial_number{};
};

struct __attribute__((packed)) EdgeLock2GoSerialFieldsV3
{
    std::array<uint8_t, 11> template_to_serial_number{
        0x30, 0x82, 0x02, 0x1E, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x10};
    std::array<uint8_t, 16> serial_number{};
};

template<cert::FmcCertificateProfile FmcProfileValue>
struct __attribute__((packed)) DevIkIdentityFieldsV4Layout
{
    static_assert(FmcProfileValue == cert::FmcCertificateProfile::NvidiaV4
                  || FmcProfileValue == cert::FmcCertificateProfile::OpenSmaV4);
    static constexpr bool IsOpenSma = FmcProfileValue == cert::FmcCertificateProfile::OpenSmaV4;

    std::array<uint8_t, 44> template_to_dda_ordinal_number = {
        0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03,
        0x03, 0x30, 0x41, 0x31, 0x21, 0x30, 0x1F, 0x06, 0x03, 0x55, 0x04,
        0x03, 0x0C, 0x18, 0x4E, 0x56, 0x49, 0x44, 0x49, 0x41, 0x20, 0x4D,
        0x43, 0x55, 0x20, 0x53, 0x4D, 0x41, 0x20, 0x70, 0x43, 0x41, 0x20};

    std::array<uint8_t, 5> dda_ordinal_number;

    std::array<uint8_t, 89> template_to_fmc_ordinal_number = {
        0x31, 0x0F, 0x30, 0x0D, 0x06, 0x03,
        0x55, 0x04, 0x0A, 0x0C, 0x06, 0x4E,
        0x56, 0x49, 0x44, 0x49, 0x41, 0x31,
        0x0B, 0x30, 0x09, 0x06, 0x03, 0x55,
        0x04, 0x06, 0x13, 0x02, 0x55, 0x53,
        0x30, 0x20, 0x17, 0x0D, 0x32, 0x35,
        0x31, 0x30, 0x33, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x5A, 0x18,
        0x0F, 0x39, 0x39, 0x39, 0x39, 0x31,
        0x32, 0x33, 0x31, 0x32, 0x33, 0x35,
        0x39, 0x35, 0x39, 0x5A, 0x30, IsOpenSma ? uint8_t{0x76} : uint8_t{0x75},
        0x31, 0x2A, 0x30, 0x28, 0x06, 0x03,
        0x55, 0x04, 0x03, 0x0C, 0x21, 0x4D,
        0x43, 0x58, 0x4E, 0x35, 0x35, 0x36,
        0x20, 0x53, 0x4D, 0x41, 0x20};

    std::array<uint8_t, 5> fmc_ordinal_number;

    using SubjectSerialPrefix = std::
        conditional_t<IsOpenSma, std::array<uint8_t, 58>, std::array<uint8_t, 57>>;
    SubjectSerialPrefix template_to_subject_serial_number = [] {
        if constexpr (IsOpenSma) {
            return SubjectSerialPrefix{
                0x20, 0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x20, 0x49, 0x64, 0x65, 0x6E,
                0x74, 0x69, 0x74, 0x79, 0x31, 0x10, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x04,
                0x0A, 0x0C, 0x07, 0x4F, 0x70, 0x65, 0x6E, 0x53, 0x4D, 0x41, 0x31, 0x0B,
                0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31,
                0x29, 0x30, 0x27, 0x06, 0x03, 0x55, 0x04, 0x05, 0x13, 0x20};
        }
        return SubjectSerialPrefix{0x20, 0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x20, 0x49, 0x64,
                                   0x65, 0x6E, 0x74, 0x69, 0x74, 0x79, 0x31, 0x0F, 0x30, 0x0D,
                                   0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x06, 0x4E, 0x56, 0x49,
                                   0x44, 0x49, 0x41, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55,
                                   0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x29, 0x30, 0x27,
                                   0x06, 0x03, 0x55, 0x04, 0x05, 0x13, 0x20};
    }();

    std::array<uint8_t, 32> subject_serial_number;

    std::array<uint8_t, 24> template_to_public_key = {
        0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02,
        0x01, 0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x22, 0x03, 0x62, 0x00, 0x04};

    std::array<uint8_t, 96> public_key;
};

using DevIkIdentityFieldsV4 = DevIkIdentityFieldsV4Layout<
    cert::FmcCertificateProfile::NvidiaV4>;
using OpenSmaDevIkIdentityFieldsV4 = DevIkIdentityFieldsV4Layout<
    cert::FmcCertificateProfile::OpenSmaV4>;

struct __attribute__((packed)) DevIkIdentityFieldsV3
{
    std::array<uint8_t, 44> template_to_dda_ordinal_number = {
        0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03,
        0x03, 0x30, 0x41, 0x31, 0x21, 0x30, 0x1F, 0x06, 0x03, 0x55, 0x04,
        0x03, 0x0C, 0x18, 0x4E, 0x56, 0x49, 0x44, 0x49, 0x41, 0x20, 0x4D,
        0x43, 0x55, 0x20, 0x53, 0x4D, 0x41, 0x20, 0x70, 0x43, 0x41, 0x20};

    std::array<uint8_t, 5> dda_ordinal_number;

    std::array<uint8_t, 93> template_to_fmc_ordinal_number = {
        0x31, 0x0F, 0x30, 0x0D, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x06, 0x4E, 0x56, 0x49,
        0x44, 0x49, 0x41, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
        0x55, 0x53, 0x30, 0x20, 0x17, 0x0D, 0x32, 0x35, 0x31, 0x30, 0x33, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x5A, 0x18, 0x0F, 0x39, 0x39, 0x39, 0x39, 0x31, 0x32, 0x33,
        0x31, 0x32, 0x33, 0x35, 0x39, 0x35, 0x39, 0x5A, 0x30, 0x79, 0x31, 0x2E, 0x30, 0x2C,
        0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x25, 0x4D, 0x43, 0x55, 0x20, 0x53, 0x4D, 0x41,
        0x20, 0x4D, 0x43, 0x58, 0x4E, 0x35, 0x35, 0x36, 0x20};

    std::array<uint8_t, 5> fmc_ordinal_number;

    std::array<uint8_t, 57> template_to_subject_serial_number = {
        0x20, 0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x20, 0x49, 0x64, 0x65, 0x6E,
        0x74, 0x69, 0x74, 0x79, 0x31, 0x0F, 0x30, 0x0D, 0x06, 0x03, 0x55, 0x04,
        0x0A, 0x0C, 0x06, 0x4E, 0x56, 0x49, 0x44, 0x49, 0x41, 0x31, 0x0B, 0x30,
        0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x29,
        0x30, 0x27, 0x06, 0x03, 0x55, 0x04, 0x05, 0x13, 0x20};

    std::array<uint8_t, 32> subject_serial_number;

    std::array<uint8_t, 24> template_to_public_key = {
        0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02,
        0x01, 0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x22, 0x03, 0x62, 0x00, 0x04};

    std::array<uint8_t, 96> public_key;
};

template<std::size_t Size>
constexpr std::array<uint8_t, Size> use_open_sma_ocsp_domain(std::array<uint8_t, Size> bytes)
{
    constexpr std::array<uint8_t, 6> NvidiaDomain{'n', 'v', 'i', 'd', 'i', 'a'};
    constexpr std::array<uint8_t, 6> OpenSmaDomain{'o', 'p', 'e', 'n', 's', 'm'};
    for (std::size_t offset = 0; offset + NvidiaDomain.size() <= bytes.size(); ++offset) {
        bool matches = true;
        for (std::size_t index = 0; index < NvidiaDomain.size(); ++index) {
            matches = matches && bytes[offset + index] == NvidiaDomain[index];
        }
        if (matches) {
            for (std::size_t index = 0; index < OpenSmaDomain.size(); ++index) {
                bytes[offset + index] = OpenSmaDomain[index];
            }
            return bytes;
        }
    }
    return bytes;
}

template<cert::FmcCertificateProfile FmcProfileValue>
struct __attribute__((packed)) EdgeLock2GoExtensionFieldsLayout
{
    std::array<uint8_t, 39> template_to_authority_key_identifier = {
        0xA3, 0x81, 0xA0, 0x30, 0x81, 0x9D, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1D, 0x13,
        0x01, 0x01, 0xFF, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xFF, 0x02, 0x01, 0x00,
        0x30, 0x1F, 0x06, 0x03, 0x55, 0x1D, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14};

    std::array<uint8_t, 20> authority_key_identifier;

    std::array<uint8_t, 11> template_to_subject_key_identifier = {
        0x30, 0x1D, 0x06, 0x03, 0x55, 0x1D, 0x0E, 0x04, 0x16, 0x04, 0x14};
    std::array<uint8_t, 20> subject_key_identifier;

    std::array<uint8_t, 73> template_to_authority_information_access = [] {
        std::array<uint8_t, 73> value{
            0x30, 0x0E, 0x06, 0x03, 0x55, 0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04, 0x03,
            0x02, 0x02, 0x84, 0x30, 0x37, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07,
            0x01, 0x01, 0x04, 0x2B, 0x30, 0x29, 0x30, 0x27, 0x06, 0x08, 0x2B, 0x06, 0x01,
            0x05, 0x05, 0x07, 0x30, 0x01, 0x86, 0x1B, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F,
            0x2F, 0x6F, 0x63, 0x73, 0x70, 0x2E, 0x6E, 0x64, 0x69, 0x73, 0x2E, 0x6E, 0x76,
            0x69, 0x64, 0x69, 0x61, 0x2E, 0x63, 0x6F, 0x6D};
        if constexpr (FmcProfileValue == cert::FmcCertificateProfile::OpenSmaV4) {
            return use_open_sma_ocsp_domain(value);
        }
        return value;
    }();
};

template<cert::FmcCertificateProfile FmcProfileValue>
struct __attribute__((packed)) LegacyExtensionFieldsLayout
{
    std::array<uint8_t, 110> template_to_subject_key_identifier = [] {
        std::array<uint8_t, 110> value{
            0xA3, 0x81, 0xA0, 0x30, 0x81, 0x9D, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1D, 0x13, 0x01,
            0x01, 0xFF, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xFF, 0x02, 0x01, 0x00, 0x30, 0x0E,
            0x06, 0x03, 0x55, 0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04, 0x03, 0x02, 0x02, 0x04,
            0x30, 0x37, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01, 0x04, 0x2B,
            0x30, 0x29, 0x30, 0x27, 0x06, 0x08, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01,
            0x86, 0x1B, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x6F, 0x63, 0x73, 0x70, 0x2E,
            0x6E, 0x64, 0x69, 0x73, 0x2E, 0x6E, 0x76, 0x69, 0x64, 0x69, 0x61, 0x2E, 0x63, 0x6F,
            0x6D, 0x30, 0x1D, 0x06, 0x03, 0x55, 0x1D, 0x0E, 0x04, 0x16, 0x04, 0x14};
        if constexpr (FmcProfileValue == cert::FmcCertificateProfile::OpenSmaV4) {
            return use_open_sma_ocsp_domain(value);
        }
        return value;
    }();
    std::array<uint8_t, 20> subject_key_identifier;

    std::array<uint8_t, 13> template_to_authority_key_identifier = {
        0x30, 0x1F, 0x06, 0x03, 0x55, 0x1D, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14};

    std::array<uint8_t, 20> authority_key_identifier;
};

using EdgeLock2GoExtensionFields = EdgeLock2GoExtensionFieldsLayout<
    cert::FmcCertificateProfile::NvidiaV4>;
using OpenSmaEdgeLock2GoExtensionFields = EdgeLock2GoExtensionFieldsLayout<
    cert::FmcCertificateProfile::OpenSmaV4>;
using LegacyExtensionFields = LegacyExtensionFieldsLayout<
    cert::FmcCertificateProfile::NvidiaV4>;

struct __attribute__((packed)) DevIkSignatureFields
{
    std::array<uint8_t, 12> template_to_signature{
        0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x03};
};

template<DevIkCertificateProfile     DdaProfileValue,
         cert::FmcCertificateProfile FmcProfileValue,
         typename SerialFields,
         typename IdentityFields,
         typename ExtensionFields>
struct __attribute__((packed)) DevIkTemplateLayout
{
    static constexpr DevIkCertificateProfile     DdaProfile = DdaProfileValue;
    static constexpr cert::FmcCertificateProfile FmcProfile = FmcProfileValue;

    nv::spdm::certlib::Sequence total_certificate;
    SerialFields                serial_fields;
    IdentityFields              identity_fields;
    ExtensionFields             extension_fields;
    DevIkSignatureFields        signature_fields;
};

using DevIkTemplate            = DevIkTemplateLayout<DevIkCertificateProfile::Legacy,
                                                     cert::FmcCertificateProfile::NvidiaV4,
                                                     LegacySerialFieldsV4,
                                                     DevIkIdentityFieldsV4,
                                                     LegacyExtensionFields>;
using EdgeLock2GoDevIkTemplate = DevIkTemplateLayout<DevIkCertificateProfile::EdgeLock2Go,
                                                     cert::FmcCertificateProfile::NvidiaV4,
                                                     EdgeLock2GoSerialFieldsV4,
                                                     DevIkIdentityFieldsV4,
                                                     EdgeLock2GoExtensionFields>;
using OpenSmaEdgeLock2GoDevIkTemplate = DevIkTemplateLayout<
    DevIkCertificateProfile::EdgeLock2Go,
    cert::FmcCertificateProfile::OpenSmaV4,
    OpenSmaEdgeLock2GoSerialFieldsV4,
    OpenSmaDevIkIdentityFieldsV4,
    OpenSmaEdgeLock2GoExtensionFields>;
using DevIkTemplateV3            = DevIkTemplateLayout<DevIkCertificateProfile::Legacy,
                                                       cert::FmcCertificateProfile::LegacyV3,
                                                       LegacySerialFieldsV3,
                                                       DevIkIdentityFieldsV3,
                                                       LegacyExtensionFields>;
using EdgeLock2GoDevIkTemplateV3 = DevIkTemplateLayout<DevIkCertificateProfile::EdgeLock2Go,
                                                       cert::FmcCertificateProfile::LegacyV3,
                                                       EdgeLock2GoSerialFieldsV3,
                                                       DevIkIdentityFieldsV3,
                                                       EdgeLock2GoExtensionFields>;

static_assert(sizeof(DevIkTemplate) == 559);
static_assert(sizeof(EdgeLock2GoDevIkTemplate) == 558);
static_assert(sizeof(OpenSmaEdgeLock2GoDevIkTemplate) == 559);
static_assert(sizeof(DevIkTemplateV3) == 563);
static_assert(sizeof(EdgeLock2GoDevIkTemplateV3) == 562);

enum class DevIkTemplateComparisonRegion : uint8_t
{
    SerialNumber,
    DdaOrdinalNumber,
    FmcOrdinalNumber,
    SubjectSerialNumber,
    PublicKey,
    SubjectKeyIdentifier,
    AuthorityKeyIdentifier,
    AuthorityInformationAccess,
    Signature,
    Count,
};

struct DevIkTemplateStaticFieldLayout
{
    uint16_t offset;
    uint16_t size;
};

struct DevIkTemplateLayoutInfo
{
    uint16_t template_size;
    uint16_t serial_number_value_offset;
    uint16_t dda_ordinal_number_value_offset;
    std::array<DevIkTemplateStaticFieldLayout,
               static_cast<size_t>(DevIkTemplateComparisonRegion::Count)>
        comparison_regions;
};

constexpr bool is_supported_template_combination(DevIkCertificateProfile     dda_profile,
                                                 cert::FmcCertificateProfile fmc_profile)
{
    return fmc_profile != cert::FmcCertificateProfile::Unsupported
        && !(dda_profile == DevIkCertificateProfile::Legacy
             && fmc_profile == cert::FmcCertificateProfile::OpenSmaV4);
}

const DevIkTemplateLayoutInfo&
get_dev_ik_template_layout(DevIkCertificateProfile     dda_profile,
                           cert::FmcCertificateProfile fmc_profile);

struct __attribute__((packed)) DevIkRequest
{
    std::array<uint8_t, 16>      serial_number;
    std::array<uint8_t, 5>       dda_ordinal_number;
    std::array<uint8_t, 5>       fmc_ordinal_number;
    std::array<uint8_t, 96>      public_key;
    std::array<uint8_t, 32>      subject_serial_number;
    std::array<uint8_t, 20>      authority_key_identifier;
    std::array<uint8_t, 20>      subject_key_identifier;
    nv::spdm::certlib::Signature signature;
};

class DevIkHelper
{
private:
    static constexpr size_t MaxIkTemplateSize = sizeof(DevIkTemplateV3);
    static_assert(MaxIkTemplateSize >= sizeof(DevIkTemplate));
    static_assert(MaxIkTemplateSize >= sizeof(EdgeLock2GoDevIkTemplate));
    static_assert(MaxIkTemplateSize >= sizeof(OpenSmaEdgeLock2GoDevIkTemplate));
    static_assert(MaxIkTemplateSize >= sizeof(DevIkTemplateV3));
    static_assert(MaxIkTemplateSize >= sizeof(EdgeLock2GoDevIkTemplateV3));

    std::array<uint8_t, MaxIkTemplateSize> _template_bytes{};
    uint32_t                               _template_size{};
    nv::spdm::certlib::Signature           _signature;

public:
    DevIkHelper(nv::spdm::ik::DevIkRequest& req,
                DevIkCertificateProfile     dda_profile,
                cert::FmcCertificateProfile fmc_profile);
    void construct_cert(std::span<uint8_t>& input_buffer);
};

enum class TemplateComparisonError : uint32_t
{
    Success                        = 0,
    SerialNumberMismatch           = 1,
    DdaOrdinalNumberMismatch       = 2,
    FmcOrdinalNumberMismatch       = 3,
    SubjectSerialNumberMismatch    = 4,
    PublicKeyMismatch              = 5,
    SubjectKeyIdentifierMismatch   = 6,
    AuthorityKeyIdentifierMismatch = 7,
    SignatureMismatch              = 8,
    AuthorityInfoAccessMismatch    = 9
};

TemplateComparisonError check_two_template_is_same(std::span<const uint8_t>       template_1,
                                                   std::span<const uint8_t>       template_2,
                                                   const DevIkTemplateLayoutInfo& layout);
}  // namespace nv::spdm::ik
