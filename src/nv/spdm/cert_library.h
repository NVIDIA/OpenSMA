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

#include <algorithm>
#include <array>
#include <span>
#include <stdint.h>
#include <ranges>
#include <utility>
#include <type_traits>
#include <numeric>
#include "nv/flash/common.h"
#include <nv/nv.h>
#include "nv/spdm/spdm_crypto_helper.h"
namespace nv::spdm::certlib {

constexpr uint8_t  IntToken        = 0x02;
constexpr uint8_t  BitStringToken  = 0x03;
constexpr uint8_t  SequenceToken   = 0x30;
constexpr uint8_t  UncompressToken = 0x04;
constexpr uint8_t  LengthToken     = 0x82;
constexpr uint32_t MaxCertSize     = 0x500;
// check the cert size is align to flash buffer size, since we have some function will read cert
// from flash.
static_assert(MaxCertSize % nv::flash::BufferSize == 0);
constexpr uint32_t Ecdsa384PublicKeySize  = 96;
constexpr uint32_t Ecdsa384PrivateKeySize = 48;
constexpr uint32_t Sha384HashSize         = 48;
constexpr uint32_t Sha1HashSize           = 20;

// NOLINTBEGIN
constexpr std::array<uint8_t, 12> ObjectIdentifier_1_2_840_10045_4_3_3{
    0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x03};
constexpr std::array<uint8_t, 7> ObjectIdentifier_1_3_132_0_34{
    0x06, 0x05, 0x2B, 0x81, 0x04, 0x00, 0x22};
constexpr std::array<uint8_t, 9> ObjectIdentifier_1_2_840_10045_2_1{
    0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01};
constexpr std::array<uint8_t, 9> ObjectIdentifier_2_5_29_14{
    0x06, 0x03, 0x55, 0x1D, 0x0E, 0x04, 0x16, 0x04, 0x14};
// NOLINTEND

constexpr std::array<uint8_t, 26> DdaPatent{
    0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x18, 0x4E, 0x56, 0x49, 0x44, 0x49, 0x41,
    0x20, 0x4D, 0x43, 0x55, 0x20, 0x53, 0x4D, 0x41, 0x20, 0x70, 0x43, 0x41, 0x20};
// define the container of max cert size
using CertArray               = std::array<uint8_t, MaxCertSize>;
using Ecdsa384PrivateKeyArray = std::array<uint8_t, Ecdsa384PrivateKeySize>;
struct __attribute__((packed)) CsrHeader
{
    uint32_t hdr_tag_major_minor_patch;
    uint32_t hdr_size; /* size in bytes of data, excluding the header */
};
struct __attribute__((packed)) BitString
{
    /* data */
    uint8_t token          = BitStringToken;
    uint8_t length         = 0;
    uint8_t padding_length = 0;
};

struct __attribute__((packed)) SequenceSmall
{
    uint8_t token  = SequenceToken;
    uint8_t length = 0;
};

struct __attribute__((packed)) Sequence
{
    uint8_t token        = SequenceToken;
    uint8_t length_token = LengthToken;
    uint8_t length_msb   = 0;  // big endian
    uint8_t length_lsb   = 0;
};

// only support ecdsaWithSHA384
struct __attribute__((packed)) Signature
{
    BitString               bit_string;
    SequenceSmall           sequence_small;
    uint8_t                 r_int_token = IntToken;
    uint8_t                 r_length_token{};  // should be 48(0x30) if no padding zero
    std::array<uint8_t, 49> r_value{};
    uint8_t                 s_int_token = IntToken;
    uint8_t                 s_length_token{};  // should be 48(0x30) if no padding zero
    std::array<uint8_t, 49> s_value{};

    // the input buffer should start with BitString
    static Signature from(std::span<const uint8_t>& buffer);
};

struct __attribute__((packed)) SubjectPublicKeyInfo
{
    SequenceSmall seq;
    struct __attribute__((packed)) Algorithm
    {
        SequenceSmall          seq;
        std::array<uint8_t, 9> algorithm = ObjectIdentifier_1_2_840_10045_2_1;
        std::array<uint8_t, 7> parameter = ObjectIdentifier_1_3_132_0_34;
    } algorithm;
    struct __attribute__((packed)) SubjectPublicKey
    {
        BitString               bit_string;
        uint8_t                 unconpress_token = UncompressToken;
        std::array<uint8_t, 96> pub_key;
    } subject_public_key;
};

bool check_certificate_format_valid(std::span<const uint8_t> input_cert,
                                    uint32_t                 expected_cert_length);

const std::array<uint8_t, 96> parse_ecdsa_p384_pubkey(std::span<const uint8_t> cert);

Signature parse_signature(std::span<const uint8_t> cert);

bool    validate_certificate_signature(std::span<const uint8_t> preceding_cert,
                                       std::span<const uint8_t> current_cert);
uint8_t hex_in_int_to_ascii(uint8_t input_char);

// this function will return the first one sequence meet subject key token
std::array<uint8_t, 20> find_subject_key_identifier(std::span<const uint8_t>& cert);

uint32_t parse_dda_ordinal_number(std::span<const uint8_t> cert);
}  // namespace nv::spdm::certlib