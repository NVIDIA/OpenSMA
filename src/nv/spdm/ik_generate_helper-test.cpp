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
#include "nv/spdm/ik_generate_helper.h"

#include "nv/ut/unittest.h"

using namespace nv::spdm::ik;
using namespace ut;

namespace {

uint8_t certificate_tbs_length_lsb(DevIkCertificateProfile profile,
                                   std::array<uint8_t, 5>  fmc_ordinal)
{
    DevIkRequest req{};
    req.fmc_ordinal_number = fmc_ordinal;

    DevIkHelper                  helper(req, profile);
    nv::spdm::certlib::CertArray certificate{};
    std::span<uint8_t>           certificate_span(certificate);
    helper.construct_cert(certificate_span);

    constexpr size_t TbsLengthLsbOffset = sizeof(nv::spdm::certlib::Sequence) + 3;
    return certificate.at(TbsLengthLsbOffset);
}

nv::spdm::certlib::CertArray make_certificate(DevIkCertificateProfile profile, bool is_fmc_v3)
{
    DevIkRequest req{};
    req.fmc_ordinal_number = is_fmc_v3 ? std::array<uint8_t, 5>{'0', '0', '0', '0', '3'}
                                       : std::array<uint8_t, 5>{'0', '0', '0', '0', '4'};

    DevIkHelper                  helper(req, profile);
    nv::spdm::certlib::CertArray certificate{};
    std::span<uint8_t>           certificate_span(certificate);
    helper.construct_cert(certificate_span);
    return certificate;
}

TemplateComparisonError compare_equal_templates(DevIkCertificateProfile profile, bool is_fmc_v3)
{
    const auto  certificate   = make_certificate(profile, is_fmc_v3);
    const auto& layout        = get_dev_ik_template_layout(profile, is_fmc_v3);
    const auto  template_span = std::span<const uint8_t>(certificate)
                                   .first(layout.template_size);
    return check_two_template_is_same(template_span, template_span, layout);
}

TemplateComparisonError
compare_with_byte_changed(DevIkCertificateProfile profile, bool is_fmc_v3, size_t offset)
{
    const auto expected  = make_certificate(profile, is_fmc_v3);
    auto       actual    = expected;
    actual.at(offset)   ^= 0x01;

    const auto& layout = get_dev_ik_template_layout(profile, is_fmc_v3);
    return check_two_template_is_same(
        std::span<const uint8_t>(expected).first(layout.template_size),
        std::span<const uint8_t>(actual).first(layout.template_size),
        layout);
}

struct TemplateTestCase
{
    DevIkCertificateProfile profile;
    bool                    is_fmc_v3;
};

constexpr std::array<TemplateTestCase, 4> TemplateTestCases{
    {
     {DevIkCertificateProfile::Legacy, true},
     {DevIkCertificateProfile::Legacy, false},
     {DevIkCertificateProfile::EdgeLock2Go, true},
     {DevIkCertificateProfile::EdgeLock2Go, false},
     }
};

const DevIkTemplateStaticFieldLayout& comparison_region(const DevIkTemplateLayoutInfo& layout,
                                                        DevIkTemplateComparisonRegion  region)
{
    return layout.comparison_regions[static_cast<size_t>(region)];
}

void expect_region_mismatch(const TemplateTestCase&       test_case,
                            DevIkTemplateComparisonRegion region_id,
                            TemplateComparisonError       expected_error,
                            bool                          compare_last_byte)
{
    const auto& layout = get_dev_ik_template_layout(test_case.profile, test_case.is_fmc_v3);
    const auto& region = comparison_region(layout, region_id);
    ensure::is_eq(
        compare_with_byte_changed(test_case.profile, test_case.is_fmc_v3, region.offset),
        expected_error);
    if (compare_last_byte) {
        ensure::is_eq(compare_with_byte_changed(test_case.profile,
                                                test_case.is_fmc_v3,
                                                region.offset + region.size - 1),
                      expected_error);
    }
}

template<typename TemplateType>
void expect_layout_matches_type(DevIkCertificateProfile profile, bool is_fmc_v3)
{
    TemplateType typed_template{};
    const auto*  template_begin = reinterpret_cast<const uint8_t*>(&typed_template);
    const auto&  layout         = get_dev_ik_template_layout(profile, is_fmc_v3);
    const auto   byte_offset    = [template_begin](const auto& member) {
        return static_cast<uint16_t>(reinterpret_cast<const uint8_t*>(&member)
                                     - template_begin);
    };
    const auto expect_field_region = [&](DevIkTemplateComparisonRegion region_id,
                                         const auto&                   template_part,
                                         const auto&                   value,
                                         bool                          compare_value) {
        const auto& region          = comparison_region(layout, region_id);
        const auto  template_offset = byte_offset(template_part);
        const auto  value_offset    = byte_offset(value);
        ensure::is_eq(region.offset, template_offset);
        ensure::is_eq(region.size,
                      static_cast<uint16_t>(value_offset - template_offset
                                            + (compare_value ? sizeof(value) : 0)));
    };

    ensure::is_eq(layout.template_size, static_cast<uint16_t>(sizeof(TemplateType)));
    ensure::is_eq(layout.serial_number_value_offset,
                  byte_offset(typed_template.serial_fields.serial_number));
    ensure::is_eq(layout.dda_ordinal_number_value_offset,
                  byte_offset(typed_template.identity_fields.dda_ordinal_number));
    expect_field_region(DevIkTemplateComparisonRegion::SerialNumber,
                        typed_template.serial_fields.template_to_serial_number,
                        typed_template.serial_fields.serial_number,
                        profile != DevIkCertificateProfile::EdgeLock2Go);
    expect_field_region(DevIkTemplateComparisonRegion::DdaOrdinalNumber,
                        typed_template.identity_fields.template_to_dda_ordinal_number,
                        typed_template.identity_fields.dda_ordinal_number,
                        true);
    expect_field_region(DevIkTemplateComparisonRegion::FmcOrdinalNumber,
                        typed_template.identity_fields.template_to_fmc_ordinal_number,
                        typed_template.identity_fields.fmc_ordinal_number,
                        true);
    expect_field_region(DevIkTemplateComparisonRegion::SubjectSerialNumber,
                        typed_template.identity_fields.template_to_subject_serial_number,
                        typed_template.identity_fields.subject_serial_number,
                        true);
    expect_field_region(DevIkTemplateComparisonRegion::PublicKey,
                        typed_template.identity_fields.template_to_public_key,
                        typed_template.identity_fields.public_key,
                        true);
    expect_field_region(DevIkTemplateComparisonRegion::SubjectKeyIdentifier,
                        typed_template.extension_fields.template_to_subject_key_identifier,
                        typed_template.extension_fields.subject_key_identifier,
                        true);
    expect_field_region(DevIkTemplateComparisonRegion::AuthorityKeyIdentifier,
                        typed_template.extension_fields.template_to_authority_key_identifier,
                        typed_template.extension_fields.authority_key_identifier,
                        false);

    if constexpr (TemplateType::CertificateProfile == DevIkCertificateProfile::EdgeLock2Go) {
        const auto& region = comparison_region(
            layout, DevIkTemplateComparisonRegion::AuthorityInformationAccess);
        ensure::is_eq(
            region.offset,
            byte_offset(
                typed_template.extension_fields.template_to_authority_information_access));
        ensure::is_eq(
            region.size,
            static_cast<uint16_t>(sizeof(
                typed_template.extension_fields.template_to_authority_information_access)));
    }
    else {
        ensure::is_eq(
            comparison_region(layout, DevIkTemplateComparisonRegion::AuthorityInformationAccess)
                .size,
            0);
    }
    const auto& signature_region = comparison_region(layout,
                                                     DevIkTemplateComparisonRegion::Signature);
    ensure::is_eq(signature_region.offset,
                  byte_offset(typed_template.signature_fields.template_to_signature));
    ensure::is_eq(
        signature_region.size,
        static_cast<uint16_t>(sizeof(typed_template.signature_fields.template_to_signature)));
}

template<typename TemplateType>
void expect_vendor_offsets_match_type(DevIkCertificateProfile profile, bool is_fmc_v3)
{
    const auto&  layout = get_dev_ik_template_layout(profile, is_fmc_v3);
    TemplateType typed_template{};
    auto         raw_template = std::span<uint8_t>(reinterpret_cast<uint8_t*>(&typed_template),
                                           sizeof(typed_template));

    constexpr std::array<uint8_t, 5> DdaOrdinal{'1', '2', '3', '4', '5'};
    std::copy(DdaOrdinal.begin(),
              DdaOrdinal.end(),
              raw_template.begin() + layout.dda_ordinal_number_value_offset);
    ensure::is_eq(typed_template.identity_fields.dda_ordinal_number, DdaOrdinal);

    std::array<uint8_t, 16> expected_serial{};
    for (size_t i = 0; i < expected_serial.size(); ++i) {
        expected_serial.at(i) = static_cast<uint8_t>(i + 1);
    }
    typed_template.serial_fields.serial_number = expected_serial;
    std::array<uint8_t, 16> extracted_serial{};
    std::copy_n(raw_template.begin() + layout.serial_number_value_offset,
                extracted_serial.size(),
                extracted_serial.begin());
    ensure::is_eq(extracted_serial, expected_serial);
}

template<size_t Size>
void fill_pattern(std::array<uint8_t, Size>& value, uint8_t seed)
{
    for (size_t index = 0; index < value.size(); ++index) {
        value.at(index) = static_cast<uint8_t>(seed + index);
    }
}

template<typename TemplateType>
void expect_generated_template_matches_type(DevIkCertificateProfile profile, bool is_fmc_v3)
{
    DevIkRequest request{};
    fill_pattern(request.serial_number, 0x10);
    fill_pattern(request.dda_ordinal_number, 0x20);
    request.fmc_ordinal_number = is_fmc_v3 ? std::array<uint8_t, 5>{'0', '0', '0', '0', '3'}
                                           : std::array<uint8_t, 5>{'0', '0', '0', '0', '4'};
    fill_pattern(request.public_key, 0x30);
    fill_pattern(request.subject_serial_number, 0x40);
    fill_pattern(request.authority_key_identifier, 0x50);
    fill_pattern(request.subject_key_identifier, 0x60);

    TemplateType expected{};
    expected.serial_fields.serial_number               = request.serial_number;
    expected.identity_fields.dda_ordinal_number        = request.dda_ordinal_number;
    expected.identity_fields.fmc_ordinal_number        = request.fmc_ordinal_number;
    expected.identity_fields.public_key                = request.public_key;
    expected.identity_fields.subject_serial_number     = request.subject_serial_number;
    expected.extension_fields.authority_key_identifier = request.authority_key_identifier;
    expected.extension_fields.subject_key_identifier   = request.subject_key_identifier;
    const uint32_t WholeCertLength = sizeof(TemplateType) + request.signature.bit_string.length
                                   + sizeof(request.signature.bit_string.length)
                                   + sizeof(request.signature.bit_string.token)
                                   - sizeof(expected.total_certificate);
    expected.total_certificate.length_msb = WholeCertLength >> 8u;
    expected.total_certificate.length_lsb = WholeCertLength % 256;

    DevIkHelper                  helper(request, profile);
    nv::spdm::certlib::CertArray certificate{};
    std::span<uint8_t>           certificate_span(certificate);
    helper.construct_cert(certificate_span);
    const auto expected_bytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(&expected), sizeof(expected));
    ensure::is_eq(std::equal(expected_bytes.begin(), expected_bytes.end(), certificate.begin()),
                  true);
}

}  // namespace

TEST(DevIkHelper, select_profile_from_dda_ordinal)
{
    ensure::is_eq(certificate_profile_from_dda(999), DevIkCertificateProfile::Legacy);
    ensure::is_eq(certificate_profile_from_dda(1000), DevIkCertificateProfile::EdgeLock2Go);
    ensure::is_eq(certificate_profile_from_dda(1999), DevIkCertificateProfile::EdgeLock2Go);
    ensure::is_eq(certificate_profile_from_dda(2000), DevIkCertificateProfile::Legacy);
};

TEST(DevIkHelper, select_all_template_combinations)
{
    constexpr std::array<uint8_t, 5> FmcV3Ordinal = {'0', '0', '0', '0', '3'};
    constexpr std::array<uint8_t, 5> FmcV4Ordinal = {'0', '0', '0', '0', '4'};

    ensure::is_eq(certificate_tbs_length_lsb(DevIkCertificateProfile::Legacy, FmcV3Ordinal),
                  0x1F);
    ensure::is_eq(
        certificate_tbs_length_lsb(DevIkCertificateProfile::EdgeLock2Go, FmcV3Ordinal), 0x1E);
    ensure::is_eq(certificate_tbs_length_lsb(DevIkCertificateProfile::Legacy, FmcV4Ordinal),
                  0x1B);
    ensure::is_eq(
        certificate_tbs_length_lsb(DevIkCertificateProfile::EdgeLock2Go, FmcV4Ordinal), 0x1A);
};

TEST(DevIkHelper, compare_all_template_combinations)
{
    ensure::is_eq(compare_equal_templates(DevIkCertificateProfile::Legacy, true),
                  TemplateComparisonError::Success);
    ensure::is_eq(compare_equal_templates(DevIkCertificateProfile::Legacy, false),
                  TemplateComparisonError::Success);
    ensure::is_eq(compare_equal_templates(DevIkCertificateProfile::EdgeLock2Go, true),
                  TemplateComparisonError::Success);
    ensure::is_eq(compare_equal_templates(DevIkCertificateProfile::EdgeLock2Go, false),
                  TemplateComparisonError::Success);
};

TEST(DevIkHelper, layout_matches_all_template_types)
{
    expect_layout_matches_type<DevIkTemplate>(DevIkCertificateProfile::Legacy, false);
    expect_layout_matches_type<DevIkTemplateV3>(DevIkCertificateProfile::Legacy, true);
    expect_layout_matches_type<EdgeLock2GoDevIkTemplate>(DevIkCertificateProfile::EdgeLock2Go,
                                                         false);
    expect_layout_matches_type<EdgeLock2GoDevIkTemplateV3>(DevIkCertificateProfile::EdgeLock2Go,
                                                           true);

    expect_vendor_offsets_match_type<DevIkTemplate>(DevIkCertificateProfile::Legacy, false);
    expect_vendor_offsets_match_type<DevIkTemplateV3>(DevIkCertificateProfile::Legacy, true);
    expect_vendor_offsets_match_type<EdgeLock2GoDevIkTemplate>(
        DevIkCertificateProfile::EdgeLock2Go, false);
    expect_vendor_offsets_match_type<EdgeLock2GoDevIkTemplateV3>(
        DevIkCertificateProfile::EdgeLock2Go, true);
};

TEST(DevIkHelper, generated_bytes_match_all_template_types)
{
    expect_generated_template_matches_type<DevIkTemplate>(DevIkCertificateProfile::Legacy,
                                                          false);
    expect_generated_template_matches_type<DevIkTemplateV3>(DevIkCertificateProfile::Legacy,
                                                            true);
    expect_generated_template_matches_type<EdgeLock2GoDevIkTemplate>(
        DevIkCertificateProfile::EdgeLock2Go, false);
    expect_generated_template_matches_type<EdgeLock2GoDevIkTemplateV3>(
        DevIkCertificateProfile::EdgeLock2Go, true);
};

TEST(DevIkHelper, report_each_template_mismatch)
{
    for (const auto& test_case : TemplateTestCases) {
        expect_region_mismatch(test_case,
                               DevIkTemplateComparisonRegion::SerialNumber,
                               TemplateComparisonError::SerialNumberMismatch,
                               test_case.profile != DevIkCertificateProfile::EdgeLock2Go);
        expect_region_mismatch(test_case,
                               DevIkTemplateComparisonRegion::DdaOrdinalNumber,
                               TemplateComparisonError::DdaOrdinalNumberMismatch,
                               true);
        expect_region_mismatch(test_case,
                               DevIkTemplateComparisonRegion::FmcOrdinalNumber,
                               TemplateComparisonError::FmcOrdinalNumberMismatch,
                               true);
        expect_region_mismatch(test_case,
                               DevIkTemplateComparisonRegion::SubjectSerialNumber,
                               TemplateComparisonError::SubjectSerialNumberMismatch,
                               true);
        expect_region_mismatch(test_case,
                               DevIkTemplateComparisonRegion::PublicKey,
                               TemplateComparisonError::PublicKeyMismatch,
                               true);
        expect_region_mismatch(test_case,
                               DevIkTemplateComparisonRegion::SubjectKeyIdentifier,
                               TemplateComparisonError::SubjectKeyIdentifierMismatch,
                               true);
        expect_region_mismatch(test_case,
                               DevIkTemplateComparisonRegion::AuthorityKeyIdentifier,
                               TemplateComparisonError::AuthorityKeyIdentifierMismatch,
                               false);
        expect_region_mismatch(test_case,
                               DevIkTemplateComparisonRegion::Signature,
                               TemplateComparisonError::SignatureMismatch,
                               false);

        if (test_case.profile == DevIkCertificateProfile::EdgeLock2Go) {
            expect_region_mismatch(test_case,
                                   DevIkTemplateComparisonRegion::AuthorityInformationAccess,
                                   TemplateComparisonError::AuthorityInfoAccessMismatch,
                                   false);
        }
    }
};

TEST(DevIkHelper, preserve_ignored_dynamic_fields)
{
    for (const auto& test_case : TemplateTestCases) {
        const auto& layout = get_dev_ik_template_layout(test_case.profile, test_case.is_fmc_v3);

        if (test_case.profile == DevIkCertificateProfile::EdgeLock2Go) {
            ensure::is_eq(compare_with_byte_changed(test_case.profile,
                                                    test_case.is_fmc_v3,
                                                    layout.serial_number_value_offset),
                          TemplateComparisonError::Success);
        }
        const auto& authority_key_identifier = comparison_region(
            layout, DevIkTemplateComparisonRegion::AuthorityKeyIdentifier);
        ensure::is_eq(compare_with_byte_changed(test_case.profile,
                                                test_case.is_fmc_v3,
                                                authority_key_identifier.offset
                                                    + authority_key_identifier.size),
                      TemplateComparisonError::Success);
        const auto& signature = comparison_region(layout,
                                                  DevIkTemplateComparisonRegion::Signature);
        ensure::is_eq(compare_with_byte_changed(test_case.profile,
                                                test_case.is_fmc_v3,
                                                signature.offset + signature.size),
                      TemplateComparisonError::Success);
    }
};

TEST(DevIkHelper, reject_short_template_spans)
{
    constexpr auto Profile     = DevIkCertificateProfile::Legacy;
    constexpr bool IsFmcV3     = false;
    const auto     certificate = make_certificate(Profile, IsFmcV3);
    const auto&    layout      = get_dev_ik_template_layout(Profile, IsFmcV3);

    ensure::is_eq(
        check_two_template_is_same(
            {}, std::span<const uint8_t>(certificate).first(layout.template_size), layout),
        TemplateComparisonError::SerialNumberMismatch);
};
