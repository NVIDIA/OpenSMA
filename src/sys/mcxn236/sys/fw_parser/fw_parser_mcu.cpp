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
#include "nv/fw_parser/fw_parser_mcu.h"
#include "nv/flash/flash.h"

namespace sys::fw_parser::mcu {

namespace {
// forward the function declaration for anonymous namespace
using namespace nv::fw_parser::mcu;
std::expected<ImageHeader, ParsingErrorCode>
get_image_header(const ParsingFwType InputParseingFwType);
std::expected<Address, ParsingErrorCode>
get_certificate_block_address(const ParsingFwType InputParseingFwType);
std::expected<Address, ParsingErrorCode>
get_image_manifest_block_address(const ParsingFwType InputParseingFwType);
std::expected<NvHeader, ParsingErrorCode>
get_nv_header(const ParsingFwType InputParseingFwType);
std::expected<CertificateBlock, ParsingErrorCode>
get_certificate_block(const ParsingFwType InputParseingFwType);
std::expected<ImageManifestBlock, ParsingErrorCode>
get_image_manifest(const ParsingFwType InputParseingFwType);
// declare some helper function for read from flash
// this function help to check the value will not wrap after add.
template<typename InputT>
std::optional<InputT> check_wrap(InputT input_value, InputT add_value)
{
    static_assert(std::numeric_limits<InputT>::is_signed == false,
                  "check_wrap function is only for unsigned input data");

    if (input_value > std::numeric_limits<InputT>::max() - add_value) {
        return std::nullopt;
    }
    return input_value + add_value;
}

// this function will read the length as the same as read_span size
static uint32_t read_from_flash(const std::span<uint8_t>& read_span, Address read_address)
{
    const uint32_t NeedToReadSize = read_span.size();

    if (!check_wrap(read_address, NeedToReadSize)) {  // if address will wrap, perform no
                                                      // operation.
        return 0;
    }
    uint32_t complete_size = 0;
    while (complete_size < NeedToReadSize) {
        const uint32_t ReadSize = complete_size + nv::flash::BufferSize <= NeedToReadSize
                                    ? nv::flash::BufferSize
                                    : NeedToReadSize - complete_size;
        const auto     Chunk    = read_span.subspan(complete_size, ReadSize);
        if (nv::flash::Status::Ok != nv::flash::Flash::read(read_address, Chunk)) {
            // check the flash read success
            return 0;
        }
        if (!check_wrap(read_address, ReadSize)) {  // if address will wrap, perform no
                                                    // operation.
            return 0;
        }
        read_address  += ReadSize;
        complete_size += ReadSize;
    }
    return complete_size;
}

template<typename InputT>
std::array<uint8_t, sizeof(InputT)>& to_array_view(InputT& input_data)
{
    return *std::bit_cast<std::array<uint8_t, sizeof(input_data)>*>(&input_data);
}
template<typename InputT>
static std::span<uint8_t> to_span_view(InputT& input_data)
{
    return std::span(*std::bit_cast<std::array<uint8_t, sizeof(InputT)>*>(&input_data));
}

Address get_nv_header_address(const ParsingFwType InputParseingFwType)
{
    auto nv_header_address_check = check_wrap(
        nv::fw_parser::mcu::get_fw_image_address(InputParseingFwType), NvHeaderOffset);
    if (!nv_header_address_check.has_value()) {
        return std::numeric_limits<Address>::max();
    }
    return *nv_header_address_check;
}

std::expected<ImageHeader, ParsingErrorCode>
get_image_header(const ParsingFwType InputParseingFwType)
{
    const Address ImageBaseAddress = nv::fw_parser::mcu::get_fw_image_address(
        InputParseingFwType);
    ImageHeader    image_header{};
    auto           image_header_span_view = to_span_view(image_header);
    const uint32_t ReadCompleteSize = read_from_flash(image_header_span_view, ImageBaseAddress);
    if (ReadCompleteSize != sizeof(decltype(image_header))) {
        return std::unexpected<ParsingErrorCode>(ParsingErrorCode::FlashAccessFail);
    }
    return image_header;
}
std::expected<Address, ParsingErrorCode>
get_certificate_block_address(const ParsingFwType InputParseingFwType)
{
    auto image_header_check = get_image_header(InputParseingFwType);
    if (!image_header_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(image_header_check.error());
    }
    // get the certificate block address first
    auto certificate_block_address_check = check_wrap(
        nv::fw_parser::mcu::get_fw_image_address(InputParseingFwType),
        image_header_check->offset_to_extended_header);
    if (!certificate_block_address_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(ParsingErrorCode::CertificateBlockAddressWrap);
    }

    return *certificate_block_address_check;
}

std::expected<Address, ParsingErrorCode>
get_image_manifest_block_address(const ParsingFwType InputParseingFwType)
{
    auto certificate_address_check = get_certificate_block_address(InputParseingFwType);
    if (!certificate_address_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(certificate_address_check.error());
    }

    Address image_manifest_block_address = 0;
    // get the certificate block address first
    auto certificate_block_check = get_certificate_block(InputParseingFwType);
    if (!certificate_block_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(certificate_block_check.error());
    }
    image_manifest_block_address = *certificate_address_check
                                 + certificate_block_check->cert_block_size;

    return image_manifest_block_address;
}

std::expected<NvHeader, ParsingErrorCode> get_nv_header(const ParsingFwType InputParseingFwType)
{
    const Address  NvHeaderAddress = get_nv_header_address(InputParseingFwType);
    NvHeader       nv_header{};
    auto           nv_header_span_view = to_span_view(nv_header);
    const uint32_t ReadCompleteSize    = read_from_flash(nv_header_span_view, NvHeaderAddress);
    if (ReadCompleteSize != sizeof(decltype(nv_header))) {
        return std::unexpected<ParsingErrorCode>(ParsingErrorCode::FlashAccessFail);
    }
    if (!NvHeader::validate_nv_header(nv_header)) {
        return std::unexpected<ParsingErrorCode>(ParsingErrorCode::NvHeaderSyncNumberNotMatch);
    }
    return nv_header;
}

std::expected<CertificateBlock, ParsingErrorCode>
get_certificate_block(const ParsingFwType InputParseingFwType)
{
    auto certificate_block_address_check = get_certificate_block_address(InputParseingFwType);
    if (!certificate_block_address_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(certificate_block_address_check.error());
    }
    const Address    CertificateBlockAddress = *certificate_block_address_check;
    CertificateBlock certificate_block{};
    auto             cert_block_span_view = to_span_view(certificate_block);
    const uint32_t   ReadCompleteSize     = read_from_flash(cert_block_span_view,
                                                      CertificateBlockAddress);

    if (ReadCompleteSize != sizeof(decltype(certificate_block))) {
        return std::unexpected<ParsingErrorCode>(ParsingErrorCode::FlashAccessFail);
    }
    if (!CertificateBlock::validate_certificate_block(certificate_block)) {
        return std::unexpected<ParsingErrorCode>(
            ParsingErrorCode::CertificateBlockMagicValueNotMatch);
    }

    return certificate_block;
}

std::expected<ImageManifestBlock, ParsingErrorCode>
get_image_manifest(const ParsingFwType InputParseingFwType)
{
    auto image_manifest_block_address_check = get_image_manifest_block_address(
        InputParseingFwType);
    if (!image_manifest_block_address_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(image_manifest_block_address_check.error());
    }
    const Address      IamgeManifestBlockAddress = *image_manifest_block_address_check;
    ImageManifestBlock image_manifest_block{};
    auto               image_manifest_block_span_view = to_span_view(image_manifest_block);
    const uint32_t     ReadCompleteSize = read_from_flash(image_manifest_block_span_view,
                                                      IamgeManifestBlockAddress);
    if (ReadCompleteSize != sizeof(decltype(image_manifest_block))) {
        return std::unexpected<ParsingErrorCode>(ParsingErrorCode::FlashAccessFail);
    }

    if (!ImageManifestBlock::validate_image_manifest_block(image_manifest_block)) {
        return std::unexpected<ParsingErrorCode>(
            ParsingErrorCode::ImageManifestBlockMagicValueNotMatch);
    }

    return image_manifest_block;
}

}  // namespace

std::expected<ImageHashRange, ParsingErrorCode>
get_fw_image_hash_range(const ParsingFwType InputParseingFwType)
{
    auto Image_Base_Address = get_fw_image_address(InputParseingFwType);
    auto image_header_check = get_image_header(InputParseingFwType);
    if (!image_header_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(image_header_check.error());
    }

    return ImageHashRange{{{Image_Base_Address, image_header_check->image_length}}};
}

std::expected<MetadataHashRange, ParsingErrorCode>
get_fw_metadata_hash_range(const ParsingFwType InputParseingFwType)
{
    MetadataHashRange metadata_hash_range{};
    // first hash
    {
        const auto ImageBaseAddress = get_fw_image_address(InputParseingFwType);
        auto const NvHeader         = get_nv_header(InputParseingFwType);
        const auto NvHeaderAddress  = get_nv_header_address(InputParseingFwType);
        if (!NvHeader.has_value()) {
            return std::unexpected<ParsingErrorCode>(NvHeader.error());
        }
        if (NvHeaderAddress == std::numeric_limits<Address>::max()) {
            return std::unexpected<ParsingErrorCode>(ParsingErrorCode::NvHeaderAddressWrap);
        }
        metadata_hash_range[0] = HashRangeItem{ImageBaseAddress,
                                               NvHeaderOffset + NvHeader->nv_header_length};
    }
    // second hash
    {
        auto const CertificateAddressParsingResult = get_certificate_block_address(
            InputParseingFwType);
        const auto CertificateBlockParsingResult = get_certificate_block(InputParseingFwType);
        const auto ImageManifestParsingResult    = get_image_manifest(InputParseingFwType);
        if (!CertificateAddressParsingResult.has_value()
            || !CertificateBlockParsingResult.has_value()
            || !ImageManifestParsingResult.has_value()) {
            return std::unexpected<ParsingErrorCode>(CertificateAddressParsingResult.error());
        }
        const auto DataLengthCheck = check_wrap(
            CertificateBlockParsingResult->cert_block_size,
            ImageManifestParsingResult->image_manifest_size);
        metadata_hash_range[1] = HashRangeItem{*CertificateAddressParsingResult,
                                               *DataLengthCheck};
    }

    return metadata_hash_range;
}

std::expected<uint32_t, ParsingErrorCode>
get_security_version(const ParsingFwType InputParseingFwType)
{
    auto image_manifest_check = get_image_manifest(InputParseingFwType);

    if (!image_manifest_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(image_manifest_check.error());
    }
    constexpr uint32_t SvnMask = 0x000000ffu;
    return (image_manifest_check->firmware_version) & SvnMask;
}

std::expected<uint8_t, ParsingErrorCode> get_build_type(const ParsingFwType InputParseingFwType)
{
    auto nv_header_check = get_nv_header(InputParseingFwType);
    if (!nv_header_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(nv_header_check.error());
    }
    return nv_header_check->build_mode;
}

std::expected<FirmwareVersionT, ParsingErrorCode>
get_firmware_version(const ParsingFwType InputParseingFwType)
{
    auto nv_header_check = get_nv_header(InputParseingFwType);

    if (!nv_header_check.has_value()) {
        return std::unexpected<ParsingErrorCode>(nv_header_check.error());
    }

    return FirmwareVersionT{.build = nv_header_check->build_version,
                            .patch = nv_header_check->patch_version,
                            .minor = nv_header_check->minor_version,
                            .major = nv_header_check->major_version};
}
std::expected<uint32_t, ParsingErrorCode>
get_image_signing_key_version(const ParsingFwType InputParseingFwType)
{
    auto certificate_block_check = get_certificate_block(InputParseingFwType);
    if (!certificate_block_check) {
        return std::unexpected<ParsingErrorCode>(certificate_block_check.error());
    }
    const uint32_t SigningKeyValue = certificate_block_check->isk_certificate.constraint;
    return SigningKeyValue;
}

}  // namespace sys::fw_parser::mcu
