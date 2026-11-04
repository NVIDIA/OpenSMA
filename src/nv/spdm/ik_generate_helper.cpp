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
        if constexpr (TemplateType::CertificateProfile
                      == DevIkCertificateProfile::EdgeLock2Go) {
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
                   TemplateType::CertificateProfile != DevIkCertificateProfile::EdgeLock2Go),
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

constexpr auto BaseSerialNumberPrefix   = LegacySerialFieldsV4{}.template_to_serial_number;
constexpr auto LegacyExtensionTemplate  = LegacyExtensionFields{};
constexpr auto IdentityDdaOrdinalPrefix = DevIkIdentityFieldsV4{}
                                              .template_to_dda_ordinal_number;
constexpr auto IdentityFmcOrdinalPrefixV4 = DevIkIdentityFieldsV4{}
                                                .template_to_fmc_ordinal_number;
constexpr auto IdentitySubjectSerialPrefix = DevIkIdentityFieldsV4{}
                                                 .template_to_subject_serial_number;
constexpr auto IdentityPublicKeyPrefix    = DevIkIdentityFieldsV4{}.template_to_public_key;
constexpr auto IdentityFmcOrdinalPrefixV3 = DevIkIdentityFieldsV3{}
                                                .template_to_fmc_ordinal_number;

constexpr size_t                  FmcDnLengthIndex      = 65;
constexpr size_t                  FmcRdnLengthIndex     = 67;
constexpr size_t                  FmcCnLengthIndex      = 69;
constexpr size_t                  FmcCnValueLengthIndex = 76;
constexpr size_t                  FmcCnValueIndex       = 77;
constexpr uint8_t                 FmcV3DnLength         = 0x79;
constexpr uint8_t                 FmcV3RdnLength        = 0x2E;
constexpr uint8_t                 FmcV3CnLength         = 0x2C;
constexpr uint8_t                 FmcV3CnValueLength    = 0x25;
constexpr std::array<uint8_t, 15> FmcV3CnPrefix{
    'M', 'C', 'U', ' ', 'S', 'M', 'A', ' ', 'M', 'C', 'X', 'N', '5', '5', '6'};

constexpr size_t IdentityFmcTemplateOffset  = offsetof(DevIkIdentityFieldsV4,
                                                      template_to_fmc_ordinal_number);
constexpr size_t IdentityV3SharedTailOffset = IdentityFmcTemplateOffset
                                            + sizeof(DevIkIdentityFieldsV3{}
                                                         .template_to_fmc_ordinal_number)
                                            - 1;
constexpr size_t IdentityV3SizeDelta = sizeof(DevIkIdentityFieldsV3)
                                     - sizeof(DevIkIdentityFieldsV4);

static_assert(IdentityV3SizeDelta == 4);
static_assert(offsetof(DevIkIdentityFieldsV3, dda_ordinal_number)
              == offsetof(DevIkIdentityFieldsV4, dda_ordinal_number));
static_assert(offsetof(DevIkIdentityFieldsV3, fmc_ordinal_number)
              == offsetof(DevIkIdentityFieldsV4, fmc_ordinal_number) + IdentityV3SizeDelta);
static_assert(offsetof(DevIkIdentityFieldsV3, subject_serial_number)
              == offsetof(DevIkIdentityFieldsV4, subject_serial_number) + IdentityV3SizeDelta);
static_assert(offsetof(DevIkIdentityFieldsV3, public_key)
              == offsetof(DevIkIdentityFieldsV4, public_key) + IdentityV3SizeDelta);
static_assert(DevIkIdentityFieldsV3{}.template_to_dda_ordinal_number
              == IdentityDdaOrdinalPrefix);
static_assert(DevIkIdentityFieldsV3{}.template_to_subject_serial_number
              == IdentitySubjectSerialPrefix);
static_assert(DevIkIdentityFieldsV3{}.template_to_public_key == IdentityPublicKeyPrefix);

constexpr bool v3_fmc_prefix_is_derived_from_v4()
{
    for (size_t index = 0; index < IdentityFmcOrdinalPrefixV3.size(); ++index) {
        uint8_t expected{};
        if (index == FmcDnLengthIndex) {
            expected = FmcV3DnLength;
        }
        else if (index == FmcRdnLengthIndex) {
            expected = FmcV3RdnLength;
        }
        else if (index == FmcCnLengthIndex) {
            expected = FmcV3CnLength;
        }
        else if (index == FmcCnValueLengthIndex) {
            expected = FmcV3CnValueLength;
        }
        else if (index >= FmcCnValueIndex && index < FmcCnValueIndex + FmcV3CnPrefix.size()) {
            expected = FmcV3CnPrefix.at(index - FmcCnValueIndex);
        }
        else if (index == IdentityFmcOrdinalPrefixV3.size() - 1) {
            expected = IdentityFmcOrdinalPrefixV4.back();
        }
        else {
            expected = IdentityFmcOrdinalPrefixV4.at(index);
        }

        if (IdentityFmcOrdinalPrefixV3.at(index) != expected) {
            return false;
        }
    }
    return true;
}

static_assert(v3_fmc_prefix_is_derived_from_v4());

void make_identity_fields(std::span<uint8_t>  destination,
                          const DevIkRequest& req,
                          bool                is_fmc_v3)
{
    const size_t version_offset = is_fmc_v3 ? IdentityV3SizeDelta : 0;
    std::copy(
        IdentityDdaOrdinalPrefix.begin(), IdentityDdaOrdinalPrefix.end(), destination.begin());
    std::copy(req.dda_ordinal_number.begin(),
              req.dda_ordinal_number.end(),
              destination.begin() + offsetof(DevIkIdentityFieldsV4, dda_ordinal_number));

    std::copy(IdentityFmcOrdinalPrefixV4.begin(),
              IdentityFmcOrdinalPrefixV4.end(),
              destination.begin() + IdentityFmcTemplateOffset);
    if (is_fmc_v3) {
        destination[IdentityV3SharedTailOffset] = IdentityFmcOrdinalPrefixV4.back();

        destination[IdentityFmcTemplateOffset + FmcDnLengthIndex]      = FmcV3DnLength;
        destination[IdentityFmcTemplateOffset + FmcRdnLengthIndex]     = FmcV3RdnLength;
        destination[IdentityFmcTemplateOffset + FmcCnLengthIndex]      = FmcV3CnLength;
        destination[IdentityFmcTemplateOffset + FmcCnValueLengthIndex] = FmcV3CnValueLength;
        std::copy(FmcV3CnPrefix.begin(),
                  FmcV3CnPrefix.end(),
                  destination.begin() + IdentityFmcTemplateOffset + FmcCnValueIndex);
    }

    std::copy(req.fmc_ordinal_number.begin(),
              req.fmc_ordinal_number.end(),
              destination
                  .subspan(offsetof(DevIkIdentityFieldsV4, fmc_ordinal_number) + version_offset)
                  .begin());
    std::copy(IdentitySubjectSerialPrefix.begin(),
              IdentitySubjectSerialPrefix.end(),
              destination
                  .subspan(offsetof(DevIkIdentityFieldsV4, template_to_subject_serial_number)
                           + version_offset)
                  .begin());
    std::copy(
        req.subject_serial_number.begin(),
        req.subject_serial_number.end(),
        destination
            .subspan(offsetof(DevIkIdentityFieldsV4, subject_serial_number) + version_offset)
            .begin());
    std::copy(
        IdentityPublicKeyPrefix.begin(),
        IdentityPublicKeyPrefix.end(),
        destination
            .subspan(offsetof(DevIkIdentityFieldsV4, template_to_public_key) + version_offset)
            .begin());
    std::copy(req.public_key.begin(),
              req.public_key.end(),
              destination.subspan(offsetof(DevIkIdentityFieldsV4, public_key) + version_offset)
                  .begin());
}

}  // namespace

const DevIkTemplateLayoutInfo& get_dev_ik_template_layout(DevIkCertificateProfile profile,
                                                          bool                    is_fmc_v3)
{
    if (profile == DevIkCertificateProfile::EdgeLock2Go) {
        return is_fmc_v3 ? EdgeLock2GoV3Layout : EdgeLock2GoV4Layout;
    }
    return is_fmc_v3 ? LegacyV3Layout : LegacyV4Layout;
}

DevIkHelper::DevIkHelper(nv::spdm::ik::DevIkRequest& req,
                         DevIkCertificateProfile     certificate_profile)
: _template_size(sizeof(nv::spdm::certlib::Sequence))
, _signature(req.signature)
{
    const bool IsFmcV3 = req.fmc_ordinal_number < FmcV4OrdinalThreshold;

    auto serial_number_prefix = BaseSerialNumberPrefix;
    if (IsFmcV3) {
        constexpr uint8_t V3CertificateLengthDelta  = 4;
        serial_number_prefix.at(3)                 += V3CertificateLengthDelta;
    }
    const bool IsEdgeLock2Go = certificate_profile == DevIkCertificateProfile::EdgeLock2Go;
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
                                         : sizeof(DevIkIdentityFieldsV4);
    make_identity_fields(
        std::span<uint8_t>(_template_bytes).subspan(_template_size, identity_size),
        req,
        IsFmcV3);
    _template_size += identity_size;

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
                             LegacyExtensionTemplate.template_to_subject_key_identifier.data(),
                             HeaderAndBasicConstraintsSize);
        append_template_part(
            _template_bytes,
            _template_size,
            LegacyExtensionTemplate.template_to_authority_key_identifier.data(),
            LegacyExtensionTemplate.template_to_authority_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             req.authority_key_identifier.data(),
                             req.authority_key_identifier.size());
        append_template_part(
            _template_bytes,
            _template_size,
            LegacyExtensionTemplate.template_to_subject_key_identifier.data()
                + SubjectKeyIdentifierPrefixOffset,
            sizeof(EdgeLock2GoExtensionFields{}.template_to_subject_key_identifier));
        append_template_part(_template_bytes,
                             _template_size,
                             req.subject_key_identifier.data(),
                             req.subject_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             LegacyExtensionTemplate.template_to_subject_key_identifier.data()
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
                             LegacyExtensionTemplate.template_to_subject_key_identifier.data(),
                             LegacyExtensionTemplate.template_to_subject_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             req.subject_key_identifier.data(),
                             req.subject_key_identifier.size());
        append_template_part(
            _template_bytes,
            _template_size,
            LegacyExtensionTemplate.template_to_authority_key_identifier.data(),
            LegacyExtensionTemplate.template_to_authority_key_identifier.size());
        append_template_part(_template_bytes,
                             _template_size,
                             req.authority_key_identifier.data(),
                             req.authority_key_identifier.size());
    }

    DevIkSignatureFields signature_fields{};
    append_template_part(
        _template_bytes, _template_size, &signature_fields, sizeof(signature_fields));

    nv::spdm::certlib::Sequence total_certificate{};
    const uint32_t              WholeCertLength = _template_size + _signature.bit_string.length
                                   + sizeof(_signature.bit_string.length)
                                   + sizeof(_signature.bit_string.token)
                                   - sizeof(total_certificate);
    total_certificate.length_msb = (WholeCertLength >> 8u);
    total_certificate.length_lsb = WholeCertLength % 256;
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
