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
DevIkHelper::DevIkHelper(nv::spdm::ik::DevIkRequest& req) : _signature(req.signature)
{
    _dev_ik_template.serial_number            = req.serial_number;
    _dev_ik_template.dda_ordinal_number       = req.dda_ordinal_number;
    _dev_ik_template.fmc_ordinal_number       = req.fmc_ordinal_number;
    _dev_ik_template.public_key               = req.public_key;
    _dev_ik_template.subject_serial_number    = req.subject_serial_number;
    _dev_ik_template.authority_key_identifier = req.authority_key_identifier;
    _dev_ik_template.subject_key_identifier   = req.subject_key_identifier;

    // write the length of total certificate due to signature may have variable length.
    const uint32_t WholeCertLength = sizeof(DevIkTemplate) + _signature.bit_string.length
                                   + sizeof(_signature.bit_string.length)
                                   + sizeof(_signature.bit_string.token)
                                   - sizeof(_dev_ik_template.total_certificate);
    _dev_ik_template.total_certificate.length_msb = (WholeCertLength >> 8u);
    _dev_ik_template.total_certificate.length_lsb = WholeCertLength % 256;
};

void DevIkHelper::construct_cert(std::span<uint8_t>& input_buffer)
{
    auto  it           = input_buffer.begin();
    auto& dev_ik_array = *std::bit_cast<std::array<uint8_t, sizeof(DevIkTemplate)>*>(
        &_dev_ik_template);

    if (std::distance(it, input_buffer.end())
        < std::distance(dev_ik_array.begin(), dev_ik_array.end())) {
        return;
    }
    it = std::copy(dev_ik_array.begin(), dev_ik_array.end(), it);

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
}  // namespace nv::spdm::ik