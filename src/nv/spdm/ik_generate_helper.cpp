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
#include "nv/spdm/ik_generate_helper.h"

#include <cstddef>

namespace nv::spdm::ik {

namespace {

constexpr DevIkTemplateStaticFieldLayout make_static_field_layout(size_t offset, size_t size)
{
    return {static_cast<uint16_t>(offset), static_cast<uint16_t>(size)};
}

constexpr DevIkTemplateStaticFieldLayout make_comparison_region(size_t template_offset,
                                                                size_t value_offset,
                                                                size_t value_size,
                                                                bool   compare_value)
{
    return make_static_field_layout(
        template_offset, value_offset - template_offset + (compare_value ? value_size : 0));
}

template<typename TemplateType>
constexpr DevIkTemplateLayoutInfo make_template_layout()
{
    using SerialFields    = decltype(TemplateType{}.serial_fields);
    using IdentityFields  = decltype(TemplateType{}.identity_fields);
    using ExtensionFields = decltype(TemplateType{}.extension_fields);
    using SignatureFields = decltype(TemplateType{}.signature_fields);

    constexpr size_t SerialFieldsOffset    = offsetof(TemplateType, serial_fields);
    constexpr size_t IdentityFieldsOffset  = offsetof(TemplateType, identity_fields);
    constexpr size_t ExtensionFieldsOffset = offsetof(TemplateType, extension_fields);
    constexpr size_t SignatureFieldsOffset = offsetof(TemplateType, signature_fields);
    static_assert(sizeof(TemplateType) <= UINT16_MAX);
    constexpr auto AuthorityInformationAccess = [] {
        if constexpr (TemplateType::DdaProfile == DevIkCertificateProfile::EdgeLock2Go) {
            return make_static_field_layout(
                ExtensionFieldsOffset
                    + offsetof(ExtensionFields, template_to_authority_information_access),
                sizeof(ExtensionFields{}.template_to_authority_information_access));
        }
        return DevIkTemplateStaticFieldLayout{};
    }();

    return {
        static_cast<uint16_t>(sizeof(TemplateType)),
        static_cast<uint16_t>(SerialFieldsOffset + offsetof(SerialFields, serial_number)),
        static_cast<uint16_t>(IdentityFieldsOffset
                              + offsetof(IdentityFields, dda_ordinal_number)),
        std::array{
                   make_comparison_region(
                SerialFieldsOffset + offsetof(SerialFields, template_to_serial_number),
                   SerialFieldsOffset + offsetof(SerialFields, serial_number),
                   sizeof(SerialFields{}.serial_number),
                   TemplateType::DdaProfile != DevIkCertificateProfile::EdgeLock2Go),
                   make_comparison_region(
                IdentityFieldsOffset + offsetof(IdentityFields, template_to_dda_ordinal_number),
                   IdentityFieldsOffset + offsetof(IdentityFields, dda_ordinal_number),
                   sizeof(IdentityFields{}.dda_ordinal_number),
                   true),
                   make_comparison_region(
                IdentityFieldsOffset + offsetof(IdentityFields, template_to_fmc_ordinal_number),
                   IdentityFieldsOffset + offsetof(IdentityFields, fmc_ordinal_number),
                   sizeof(IdentityFields{}.fmc_ordinal_number),
                   true),
                   make_comparison_region(
                IdentityFieldsOffset
                    + offsetof(IdentityFields, template_to_subject_serial_number),
                   IdentityFieldsOffset + offsetof(IdentityFields, subject_serial_number),
                   sizeof(IdentityFields{}.subject_serial_number),
                   true),
                   make_comparison_region(IdentityFieldsOffset
                                       + offsetof(IdentityFields, template_to_public_key),
                   IdentityFieldsOffset + offsetof(IdentityFields, public_key),
                   sizeof(IdentityFields{}.public_key),
                   true),
                   make_comparison_region(
                ExtensionFieldsOffset
                    + offsetof(ExtensionFields, template_to_subject_key_identifier),
                   ExtensionFieldsOffset + offsetof(ExtensionFields, subject_key_identifier),
                   sizeof(ExtensionFields{}.subject_key_identifier),
                   true),
                   make_comparison_region(
                ExtensionFieldsOffset
                    + offsetof(ExtensionFields, template_to_authority_key_identifier),
                   ExtensionFieldsOffset + offsetof(ExtensionFields, authority_key_identifier),
                   sizeof(ExtensionFields{}.authority_key_identifier),
                   false),
                   AuthorityInformationAccess, make_static_field_layout(SignatureFieldsOffset
                                         + offsetof(SignatureFields, template_to_signature),
                   sizeof(SignatureFields{}.template_to_signature)),
                   }
    };
}

constexpr auto LegacyV4Layout      = make_template_layout<DevIkTemplate>();
constexpr auto EdgeLock2GoV4Layout = make_template_layout<EdgeLock2GoDevIkTemplate>();
constexpr auto
    OpenSmaEdgeLock2GoV4Layout     = make_template_layout<OpenSmaEdgeLock2GoDevIkTemplate>();
constexpr auto LegacyV3Layout      = make_template_layout<DevIkTemplateV3>();
constexpr auto EdgeLock2GoV3Layout = make_template_layout<EdgeLock2GoDevIkTemplateV3>();

bool regions_are_same(std::span<const uint8_t> template_1,
                      std::span<const uint8_t> template_2,
                      size_t                   offset,
                      size_t                   size)
{
    if (offset > template_1.size() || size > template_1.size() - offset
        || offset > template_2.size() || size > template_2.size() - offset) {
        return false;
    }

    const auto region_1 = template_1.subspan(offset, size);
    const auto region_2 = template_2.subspan(offset, size);
    return std::equal(region_1.begin(), region_1.end(), region_2.begin());
}

void append_template_part(std::span<uint8_t> destination,
                          uint32_t&          offset,
                          const void*        part,
                          size_t             part_size)
{
    const auto* bytes = static_cast<const uint8_t*>(part);
    std::copy_n(bytes, part_size, destination.subspan(offset).begin());
    offset += part_size;
}

constexpr auto BaseSerialNumberPrefix  = LegacySerialFieldsV4{}.template_to_serial_number;
constexpr auto LegacyExtensionTemplate = LegacyExtensionFields{};
void           make_identity_fields(std::span<uint8_t>          destination,
                                    const DevIkRequest&         req,
                                    cert::FmcCertificateProfile fmc_profile)
{
    const auto write_fields = [&]<typename IdentityFields>(IdentityFields fields) {
        fields.dda_ordinal_number    = req.dda_ordinal_number;
        fields.fmc_ordinal_number    = req.fmc_ordinal_number;
        fields.subject_serial_number = req.subject_serial_number;
        fields.public_key            = req.public_key;
        const auto* fields_bytes     = static_cast<const uint8_t*>(
            static_cast<const void*>(&fields));
        std::copy_n(fields_bytes, sizeof(fields), destination.begin());
    };

    if (fmc_profile == cert::FmcCertificateProfile::LegacyV3) {
        write_fields(DevIkIdentityFieldsV3{});
    }
    else if (fmc_profile == cert::FmcCertificateProfile::OpenSmaV4) {
        write_fields(OpenSmaDevIkIdentityFieldsV4{});
    }
    else {
        write_fields(DevIkIdentityFieldsV4{});
    }
}

}  // namespace

const DevIkTemplateLayoutInfo&
get_dev_ik_template_layout(DevIkCertificateProfile     dda_profile,
                           cert::FmcCertificateProfile fmc_profile)
{
    NV_ASSERT(is_supported_template_combination(dda_profile, fmc_profile),
              "Unsupported DDA/FMC certificate profile combination");
    if (fmc_profile == cert::FmcCertificateProfile::OpenSmaV4) {
        return OpenSmaEdgeLock2GoV4Layout;
    }
    if (dda_profile == DevIkCertificateProfile::EdgeLock2Go) {
        return fmc_profile == cert::FmcCertificateProfile::LegacyV3 ? EdgeLock2GoV3Layout
                                                                    : EdgeLock2GoV4Layout;
    }
    return fmc_profile == cert::FmcCertificateProfile::LegacyV3 ? LegacyV3Layout
                                                                : LegacyV4Layout;
}

DevIkHelper::DevIkHelper(nv::spdm::ik::DevIkRequest& req,
                         DevIkCertificateProfile     dda_profile,
                         cert::FmcCertificateProfile fmc_profile)
: _template_size(sizeof(nv::spdm::certlib::Sequence))
, _signature(req.signature)
{
    NV_ASSERT(is_supported_template_combination(dda_profile, fmc_profile),
              "Unsupported DDA/FMC certificate profile combination");
    const bool IsFmcV3   = fmc_profile == cert::FmcCertificateProfile::LegacyV3;
    const bool IsOpenSma = fmc_profile == cert::FmcCertificateProfile::OpenSmaV4;

    auto serial_number_prefix = BaseSerialNumberPrefix;
    if (IsFmcV3) {
        constexpr uint8_t V3CertificateLengthDelta  = 4;
        serial_number_prefix.at(3)                 += V3CertificateLengthDelta;
    }
    if (IsOpenSma) {
        ++serial_number_prefix.at(3);
    }
    const bool IsEdgeLock2Go = dda_profile == DevIkCertificateProfile::EdgeLock2Go;
    if (IsEdgeLock2Go) {
        constexpr size_t SerialNumberLengthOffset = 10;
        --serial_number_prefix.at(3);
        --serial_number_prefix.at(SerialNumberLengthOffset);
    }
    append_template_part(_template_bytes,
                         _template_size,
                         serial_number_prefix.data(),
                         serial_number_prefix.size() - static_cast<size_t>(IsEdgeLock2Go));
    append_template_part(
        _template_bytes, _template_size, req.serial_number.data(), req.serial_number.size());

    const size_t identity_size = IsFmcV3 ? sizeof(DevIkIdentityFieldsV3)
                                         : (IsOpenSma ? sizeof(OpenSmaDevIkIdentityFieldsV4)
                                                      : sizeof(DevIkIdentityFieldsV4));
    make_identity_fields(
        std::span<uint8_t>(_template_bytes).subspan(_template_size, identity_size),
        req,
        fmc_profile);
    _template_size += identity_size;

    auto extension_template = LegacyExtensionTemplate;
    if (IsOpenSma) {
        extension_template
            .template_to_subject_key_identifier = LegacyExtensionFieldsLayout<
                                                      cert::FmcCertificateProfile::OpenSmaV4>{}
                                                      .template_to_subject_key_identifier;
    }

    if (IsEdgeLock2Go) {
        constexpr size_t
            HeaderAndBasicConstraintsSize = sizeof(EdgeLock2GoExtensionFields{}
                                                       .template_to_authority_key_identifier)
                                          - sizeof(LegacyExtensionFields{}
                                                       .template_to_authority_key_identifier);
        constexpr size_t KeyUsageAndAuthorityInfoAccessSize = sizeof(
            EdgeLock2GoExtensionFields{}.template_to_authority_information_access);
        constexpr size_t  EdgeLock2GoKeyUsageBitsOffset    = 15;
        constexpr uint8_t LegacyKeyUsageBits               = 0x04;
        constexpr uint8_t EdgeLock2GoKeyUsageBits          = 0x84;
        constexpr size_t  SubjectKeyIdentifierPrefixOffset = HeaderAndBasicConstraintsSize
                                                          + KeyUsageAndAuthorityInfoAccessSize;
        static_assert(
            SubjectKeyIdentifierPrefixOffset
                + sizeof(EdgeLock2GoExtensionFields{}.template_to_subject_key_identifier)
            == sizeof(LegacyExtensionFields{}.template_to_subject_key_identifier));

        append_template_part(_template_bytes,
                             _template_size,
                             extension_template.template_to_subject_key_identifier.data(),
                             HeaderAndBasicConstraintsSize);
        append_template_part(_template_bytes,
                             _template_size,
                             extension_template.template_to_authority_key_identifier.data(),
                             extension_template.template_to_authority_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             req.authority_key_identifier.data(),
                             req.authority_key_identifier.size());
        append_template_part(
            _template_bytes,
            _template_size,
            extension_template.template_to_subject_key_identifier.data()
                + SubjectKeyIdentifierPrefixOffset,
            sizeof(EdgeLock2GoExtensionFields{}.template_to_subject_key_identifier));
        append_template_part(_template_bytes,
                             _template_size,
                             req.subject_key_identifier.data(),
                             req.subject_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             extension_template.template_to_subject_key_identifier.data()
                                 + HeaderAndBasicConstraintsSize,
                             KeyUsageAndAuthorityInfoAccessSize);
        static_assert(LegacyExtensionTemplate.template_to_subject_key_identifier.at(
                          HeaderAndBasicConstraintsSize + EdgeLock2GoKeyUsageBitsOffset)
                      == LegacyKeyUsageBits);
        static_assert(EdgeLock2GoExtensionFields{}.template_to_authority_information_access.at(
                          EdgeLock2GoKeyUsageBitsOffset)
                      == EdgeLock2GoKeyUsageBits);
        _template_bytes.at(_template_size - KeyUsageAndAuthorityInfoAccessSize
                           + EdgeLock2GoKeyUsageBitsOffset) = EdgeLock2GoKeyUsageBits;
    }
    else {
        append_template_part(_template_bytes,
                             _template_size,
                             extension_template.template_to_subject_key_identifier.data(),
                             extension_template.template_to_subject_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             req.subject_key_identifier.data(),
                             req.subject_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             extension_template.template_to_authority_key_identifier.data(),
                             extension_template.template_to_authority_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             req.authority_key_identifier.data(),
                             req.authority_key_identifier.size());
    }

    const DevIkSignatureFields signature_fields{};
    append_template_part(
        _template_bytes, _template_size, &signature_fields, sizeof(signature_fields));

    const uint32_t WholeCertLength = _template_size + _signature.bit_string.length
                                   + sizeof(_signature.bit_string.length)
                                   + sizeof(_signature.bit_string.token)
                                   - sizeof(nv::spdm::certlib::Sequence);
    const nv::spdm::certlib::Sequence total_certificate{
        .length_msb = static_cast<uint8_t>(WholeCertLength >> 8u),
        .length_lsb = static_cast<uint8_t>(WholeCertLength % 256u),
    };
    const auto
        total_certificate_bytes = std::bit_cast<std::array<uint8_t, sizeof(total_certificate)>>(
            total_certificate);
    std::copy(total_certificate_bytes.begin(),
              total_certificate_bytes.end(),
              _template_bytes.begin());
};

void DevIkHelper::construct_cert(std::span<uint8_t>& input_buffer)
{
    auto it = input_buffer.begin();

    if (std::cmp_less(std::distance(it, input_buffer.end()), _template_size)) {
        return;
    }
    it = std::copy(_template_bytes.begin(), _template_bytes.begin() + _template_size, it);

    auto& bit_string_view = *std::bit_cast<std::array<uint8_t, sizeof(_signature.bit_string)>*>(
        &_signature.bit_string);
    auto& sequence_small_view = *std::bit_cast<
        std::array<uint8_t, sizeof(_signature.sequence_small)>*>(&_signature.sequence_small);
    if (std::distance(it, input_buffer.end())
        < std::distance(bit_string_view.begin(), bit_string_view.end())) {
        return;
    }

    it = std::copy(bit_string_view.begin(), bit_string_view.end(), it);
    if (std::distance(it, input_buffer.end())
        < std::distance(sequence_small_view.begin(), sequence_small_view.end())) {
        return;
    }
    it = std::copy(sequence_small_view.begin(), sequence_small_view.end(), it);

    // write the signature into buffer
    if (std::cmp_less(std::distance(it, input_buffer.end()), 2)) {
        return;
    }
    *it = _signature.r_int_token;
    ++it;
    *it = _signature.r_length_token;
    ++it;

    if (std::cmp_less(std::distance(it, input_buffer.end()), _signature.r_length_token)
        || std::cmp_greater(_signature.r_length_token, _signature.r_value.size())) {
        return;
    }
    it = std::copy(
        _signature.r_value.begin(), _signature.r_value.begin() + _signature.r_length_token, it);

    if (std::cmp_less(std::distance(it, input_buffer.end()), 2)) {
        return;
    }
    *it = _signature.s_int_token;
    ++it;
    *it = _signature.s_length_token;
    ++it;

    if (std::cmp_less(std::distance(it, input_buffer.end()), _signature.s_length_token)
        || std::cmp_greater(_signature.s_length_token, _signature.s_value.size())) {
        return;
    }
    it = std::copy(
        _signature.s_value.begin(), _signature.s_value.begin() + _signature.s_length_token, it);
}
TemplateComparisonError check_two_template_is_same(std::span<const uint8_t>       template_1,
                                                   std::span<const uint8_t>       template_2,
                                                   const DevIkTemplateLayoutInfo& layout)
{
    size_t index{};
    for (const auto& region : layout.comparison_regions) {
        if (region.size != 0
            && !regions_are_same(template_1, template_2, region.offset, region.size)) {
            constexpr auto LastSequentialError = static_cast<size_t>(
                DevIkTemplateComparisonRegion::AuthorityKeyIdentifier);
            if (index <= LastSequentialError) {
                return static_cast<TemplateComparisonError>(index + 1);
            }
            if (index
                == static_cast<size_t>(
                    DevIkTemplateComparisonRegion::AuthorityInformationAccess)) {
                return TemplateComparisonError::AuthorityInfoAccessMismatch;
            }
            return TemplateComparisonError::SignatureMismatch;
        }
        ++index;
    }

    return TemplateComparisonError::Success;
}
}  // namespace nv::spdm::ik
