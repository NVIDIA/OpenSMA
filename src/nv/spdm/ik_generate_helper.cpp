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
namespace nv::spdm::ik {

template<typename TemplateType>
void DevIkHelper::fill_template(TemplateType& templ, nv::spdm::ik::DevIkRequest& req)
{
    templ.serial_number            = req.serial_number;
    templ.dda_ordinal_number       = req.dda_ordinal_number;
    templ.fmc_ordinal_number       = req.fmc_ordinal_number;
    templ.public_key               = req.public_key;
    templ.subject_serial_number    = req.subject_serial_number;
    templ.authority_key_identifier = req.authority_key_identifier;
    templ.subject_key_identifier   = req.subject_key_identifier;

    _template_size = sizeof(TemplateType);

    const uint32_t WholeCertLength = _template_size + _signature.bit_string.length
                                   + sizeof(_signature.bit_string.length)
                                   + sizeof(_signature.bit_string.token)
                                   - sizeof(templ.total_certificate);
    templ.total_certificate.length_msb = (WholeCertLength >> 8u);
    templ.total_certificate.length_lsb = WholeCertLength % 256;

    auto& bytes = *std::bit_cast<std::array<uint8_t, sizeof(TemplateType)>*>(&templ);
    std::copy(bytes.begin(), bytes.end(), _template_bytes.begin());
}

DevIkHelper::DevIkHelper(nv::spdm::ik::DevIkRequest& req) : _signature(req.signature)
{
    const bool IsFmcV3 = req.fmc_ordinal_number < FmcV4OrdinalThreshold;

    if (IsFmcV3) {
        DevIkTemplateV3 templ{};
        fill_template(templ, req);
    }
    else {
        DevIkTemplate templ{};
        fill_template(templ, req);
    }
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
template<typename TemplateType>
TemplateComparisonError check_two_template_is_same(const TemplateType& template_1,
                                                   const TemplateType& template_2)
{
    // Check each field individually and return specific error code
    if (template_1.template_to_serial_number != template_2.template_to_serial_number
        || template_1.serial_number != template_2.serial_number) {
        return TemplateComparisonError::SerialNumberMismatch;
    }

    if (template_1.template_to_dda_ordinal_number != template_2.template_to_dda_ordinal_number
        || template_1.dda_ordinal_number != template_2.dda_ordinal_number) {
        return TemplateComparisonError::DdaOrdinalNumberMismatch;
    }

    if (template_1.template_to_fmc_ordinal_number != template_2.template_to_fmc_ordinal_number
        || template_1.fmc_ordinal_number != template_2.fmc_ordinal_number) {
        return TemplateComparisonError::FmcOrdinalNumberMismatch;
    }

    if (template_1.template_to_subject_serial_number
            != template_2.template_to_subject_serial_number
        || template_1.subject_serial_number != template_2.subject_serial_number) {
        return TemplateComparisonError::SubjectSerialNumberMismatch;
    }

    if (template_1.template_to_public_key != template_2.template_to_public_key
        || template_1.public_key != template_2.public_key) {
        return TemplateComparisonError::PublicKeyMismatch;
    }

    if (template_1.template_to_subject_key_identifier
            != template_2.template_to_subject_key_identifier
        || template_1.subject_key_identifier != template_2.subject_key_identifier) {
        return TemplateComparisonError::SubjectKeyIdentifierMismatch;
    }

    if (template_1.template_to_authority_key_identifier
        != template_2.template_to_authority_key_identifier) {
        // Note: authority_key_identifier itself is not checked as per original comment
        return TemplateComparisonError::AuthorityKeyIdentifierMismatch;
    }

    if (template_1.template_to_signature != template_2.template_to_signature) {
        return TemplateComparisonError::SignatureMismatch;
    }

    return TemplateComparisonError::Success;
}

template TemplateComparisonError
check_two_template_is_same<DevIkTemplate>(const DevIkTemplate&, const DevIkTemplate&);
template TemplateComparisonError
check_two_template_is_same<DevIkTemplateV3>(const DevIkTemplateV3&, const DevIkTemplateV3&);
}  // namespace nv::spdm::ik