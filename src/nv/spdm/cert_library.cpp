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
#include "nv/spdm/cert_library.h"
namespace nv::spdm::certlib {
namespace {

// check the difference type is large enough to hold the need copy size, and move the overhead
// to compile time
template<typename Iter, typename Object>
concept Iter_diff_type_is_large_enough = std::numeric_limits<typename std::iterator_traits<
                                             Iter>::difference_type>::max()
                                      >= sizeof(Object);
template<typename Iter, typename Object>
requires Iter_diff_type_is_large_enough<Iter, Object>
bool static fill(Iter& input_it_start, Iter input_it_end, Object& output_obj)
{
    constexpr size_t NeedCopySize = sizeof(Object);
    // check the input iter of the buffe is large enough to hold the need copy size
    if (std::cmp_less(std::distance(input_it_start, input_it_end), NeedCopySize)) {
        return false;
    }
    std::array<uint8_t, NeedCopySize>&
        output_obj_array = *std::bit_cast<std::array<uint8_t, NeedCopySize>*>(&output_obj);

    std::copy(input_it_start, input_it_start + NeedCopySize, output_obj_array.begin());
    input_it_start = input_it_start + NeedCopySize;
    return true;
}
}  // namespace

Signature Signature::from(std::span<const uint8_t>& buffer)
{
    Signature construct_signature{};
    auto      buffer_iter = buffer.begin();

    auto result = fill(buffer_iter, buffer.end(), construct_signature.bit_string);
    if (!result) {
        return {};
    }

    result = fill(buffer_iter, buffer.end(), construct_signature.sequence_small);
    if (!result) {
        return {};
    }

    // signature r
    result = fill(buffer_iter, buffer.end(), construct_signature.r_int_token);
    if (!result) {
        return {};
    }

    result = fill(buffer_iter, buffer.end(), construct_signature.r_length_token);
    if (!result) {
        return {};
    }

    // check the length token is in range 0~49( max = signature 48 + padding 1 )
    if (construct_signature.r_length_token > construct_signature.r_value.size()) {
        return {};
    }
    // check the buffer iter is large enough to hold the need copy size
    if (std::cmp_less(std::distance(buffer_iter, buffer.end()),
                      construct_signature.r_length_token)) {
        return {};
    }
    std::copy(buffer_iter,
              buffer_iter + construct_signature.r_length_token,
              construct_signature.r_value.begin());
    buffer_iter = buffer_iter + construct_signature.r_length_token;

    // signature s
    result = fill(buffer_iter, buffer.end(), construct_signature.s_int_token);
    if (!result) {
        return {};
    }

    result = fill(buffer_iter, buffer.end(), construct_signature.s_length_token);
    if (!result) {
        return {};
    }

    // check the length token is in range 0~49( max = signature 48 + padding 1 )
    if (construct_signature.s_length_token > construct_signature.s_value.size()) {
        return {};
    }
    // check the buffer iter is large enough to hold the need copy size
    if (std::cmp_less(std::distance(buffer_iter, buffer.end()),
                      construct_signature.s_length_token)) {
        return {};
    }

    std::copy(buffer_iter,
              buffer_iter + construct_signature.s_length_token,
              construct_signature.s_value.begin());
    buffer_iter = buffer_iter + construct_signature.s_length_token;

    return construct_signature;
}

bool check_certificate_format_valid(std::span<const uint8_t> input_cert,
                                    uint32_t                 expected_cert_length)
{
    constexpr size_t CertLengthFieldEnd = 4u;
    if (input_cert.size() < CertLengthFieldEnd) {
        return false;
    }
    // the format not correct (should be 0x3082)
    if (input_cert[0u] != certlib::SequenceToken || input_cert[1u] != certlib::LengthToken) {
        return false;
    }
    // for coverity, but this should not happen
    static_assert(
        (std::numeric_limits<std::remove_reference_t<decltype(input_cert[2])>>::max() << 8U)
            + std::numeric_limits<std::remove_reference_t<decltype(input_cert[3])>>::max() + 4u
        <= std::numeric_limits<uint32_t>::max());

    // coverity[cert_int30_c_violation] - already checked on above static_assert
    const uint32_t CertLength = (input_cert[2] * 256u) + input_cert[3] + 4u;

    // check the cert length is the same as the input certificate length
    if (CertLength != expected_cert_length) {
        return false;
    }
    return true;
};

const std::array<uint8_t, Ecdsa384PublicKeySize>
parse_ecdsa_p384_pubkey(std::span<const uint8_t> cert)
{
    std::array<uint8_t, Ecdsa384PublicKeySize> ret_pub_key{};
    auto                                       find_it = std::find_end(cert.begin(),
                                 cert.end(),
                                 ObjectIdentifier_1_3_132_0_34.begin(),
                                 ObjectIdentifier_1_3_132_0_34.end());

    if (find_it != cert.end()) {
        auto offset_to_pubilc_key = std::distance(
            cert.begin(),
            find_it + ObjectIdentifier_1_3_132_0_34.size()
                + sizeof(SubjectPublicKeyInfo::SubjectPublicKey::bit_string)
                + sizeof(SubjectPublicKeyInfo::SubjectPublicKey::unconpress_token));

        if (offset_to_pubilc_key < 0
            || static_cast<size_t>(offset_to_pubilc_key) + Ecdsa384PublicKeySize
                   > cert.size()) {
            return ret_pub_key;
        }
        std::copy(cert.begin() + offset_to_pubilc_key,
                  cert.begin() + offset_to_pubilc_key + Ecdsa384PublicKeySize,
                  ret_pub_key.begin());
        return ret_pub_key;
    }
    // should not reach
    return ret_pub_key;
}

Signature parse_signature(std::span<const uint8_t> cert)

{
    auto find_it = std::find_end(cert.begin(),
                                 cert.end(),
                                 ObjectIdentifier_1_2_840_10045_4_3_3.begin(),
                                 ObjectIdentifier_1_2_840_10045_4_3_3.end());

    if (find_it != cert.end()
        && std::cmp_greater_equal(std::distance(find_it, cert.end()),
                                  ObjectIdentifier_1_2_840_10045_4_3_3.size())) {
        auto offset_to_signature = std::distance(
            cert.begin(), find_it + ObjectIdentifier_1_2_840_10045_4_3_3.size());

        if (offset_to_signature <= 0) {
            return Signature{};
        }
        auto signature_span = cert.subspan(static_cast<size_t>(offset_to_signature));
        return Signature::from(signature_span);
    }
    // should not reach
    return Signature{};
};

bool validate_certificate_signature(std::span<const uint8_t> preceding_cert,
                                    std::span<const uint8_t> current_cert)
{
    // mbedtls_ecdsa_verify
    using namespace nv;
    std::array<uint8_t, Sha384HashSize> current_cert_hash{};
    constexpr uint32_t                  StartOffsetOfCert  = 4;
    constexpr size_t                    CertLengthFieldEnd = 8;
    if (current_cert.size() < CertLengthFieldEnd) {
        return false;
    }
    // for coverity, but this should not happen
    static_assert(
        (std::numeric_limits<std::remove_reference_t<decltype(current_cert[6])>>::max() << 8U)
            + 4U
            + std::numeric_limits<std::remove_reference_t<decltype(current_cert[7])>>::max()
        <= std::numeric_limits<uint32_t>::max());

    // coverity[cert_int30_c_violation] -  already checked on above static_assert
    const uint32_t LengthOfCert = 4u + (current_cert[6] << 8u) + current_cert[7];
    // check the length of the cert is large enough to hold the need copy size
    if (LengthOfCert > current_cert.size() - StartOffsetOfCert) {
        return false;
    }
    Signature sig_cur_cert = parse_signature(current_cert);
    const std::array<uint8_t, Ecdsa384PublicKeySize> pre_pub_key = parse_ecdsa_p384_pubkey(
        preceding_cert);

    if (nv::spdm::crypto::spdm_hash_data(
            current_cert_hash.data(), current_cert.data() + StartOffsetOfCert, LengthOfCert)
        != nv::spdm::crypto::CryptoStatus::Success) {
        return false;
    }
    auto r_signature_span = std::span<const uint8_t>(sig_cur_cert.r_value)
                                .subspan(0, sig_cur_cert.r_length_token);
    auto s_signature_span = std::span<const uint8_t>(sig_cur_cert.s_value)
                                .subspan(0, sig_cur_cert.s_length_token);

    if (nv::spdm::crypto::spdm_ecdsa_verify(
            pre_pub_key, r_signature_span, s_signature_span, current_cert_hash)
        != nv::spdm::crypto::CryptoStatus::Success) {
        return false;
    }
    return true;
};

// NOLINTBEGIN
uint8_t hex_in_int_to_ascii(uint8_t input_char)
{
    switch (input_char) {
        case 0u : return '0';
        case 1u : return '1';
        case 2u : return '2';
        case 3u : return '3';
        case 4u : return '4';
        case 5u : return '5';
        case 6u : return '6';
        case 7u : return '7';
        case 8u : return '8';
        case 9u : return '9';
        case 10u: return 'A';
        case 11u: return 'B';
        case 12u: return 'C';
        case 13u: return 'D';
        case 14u: return 'E';
        case 15u: return 'F';
        default : return '0';
    }
}
// NOLINTEND

uint32_t parse_dda_ordinal_number(std::span<const uint8_t> cert)
{
    auto find_it = std::find_end(cert.begin(), cert.end(), DdaPatent.begin(), DdaPatent.end());
    if (find_it != cert.end()) {
        find_it                                     = find_it + DdaPatent.size();
        uint32_t                          ret_value = 0;
        constexpr std::array<uint32_t, 5> Base10Arr{10000u, 1000u, 100u, 10u, 1u};
        constexpr uint8_t                 MaxDigit = '9';
        constexpr uint8_t                 MinDigit = '0';

        static_assert(std::accumulate(Base10Arr.begin(), Base10Arr.end(), 0u)
                          * (MaxDigit - MinDigit)
                      <= std::numeric_limits<uint32_t>::max());
        for (auto base_10_iter = Base10Arr.begin();
             base_10_iter != Base10Arr.end() and find_it != cert.end() and *find_it >= MinDigit
             and *find_it <= MaxDigit;
             base_10_iter++, find_it++) {
            // coverity[cert_int30_c_violation] - already checked on above static_assert
            ret_value += (*base_10_iter) * (*find_it - MinDigit);
        }
        return ret_value;
    }
    // should not reach
    return std::numeric_limits<uint32_t>::max();
};

std::array<uint8_t, Sha1HashSize> find_subject_key_identifier(std::span<const uint8_t>& cert)
{
    std::array<uint8_t, Sha1HashSize> subject_key_identifier{0};

    // Find the last occurrence of the object identifier pattern using ranges
    auto pattern_range = std::ranges::find_end(cert, ObjectIdentifier_2_5_29_14);

    if (!pattern_range.empty()) {
        // Create a view of the data after the found pattern
        auto data_after_pattern = std::ranges::subrange(pattern_range.end(), cert.end());

        // Safely copy only the required amount of data using views
        auto source_data = data_after_pattern | std::views::take(subject_key_identifier.size());

        // Only copy if we have enough data available
        if (std::ranges::size(source_data) == subject_key_identifier.size()) {
            std::ranges::copy(source_data, subject_key_identifier.begin());
        }
    }

    return subject_key_identifier;
}

}  // namespace nv::spdm::certlib