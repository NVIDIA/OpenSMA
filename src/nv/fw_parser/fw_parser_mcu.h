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

#include <array>
#include <stdint.h>
#include <optional>
#include <expected>
#include "sys/flash/flash_config.h"
#include "sys/fw_parser/image_layout.h"
namespace nv::fw_parser::mcu {

using Address = uint32_t;

constexpr Address FmcFwAddress      = sys::flash::config::FmcFwAddress;
constexpr Address ActiveFwAddress   = 0x0;
constexpr Address InactiveFwAddress = sys::flash::config::Slot1FwAddress;
constexpr Address Slot0FwAddress    = 0x0;
constexpr Address Slot1FwAddress    = sys::flash::config::Slot1FwAddress;

using ParsingFwType = sys::fw_parser::mcu::ParsingFwType;

using ParsingErrorCode  = sys::fw_parser::mcu::ParsingErrorCode;
using FirmwareVersionT  = sys::fw_parser::mcu::FirmwareVersionT;
using ImageHashRange    = sys::fw_parser::mcu::ImageHashRange;
using MetadataHashRange = sys::fw_parser::mcu::MetadataHashRange;
Address get_fw_image_address(const ParsingFwType InputParseingFwType);
// Helper api for getting some common usage info on fw
std::expected<uint32_t, ParsingErrorCode>
get_security_version(const ParsingFwType InputParseingFwType);

std::expected<uint32_t, ParsingErrorCode>
get_image_signing_key_version(const ParsingFwType InputParseingFwType);

std::expected<FirmwareVersionT, ParsingErrorCode>
get_firmware_version(const ParsingFwType InputParseingFwType);

std::expected<ImageHashRange, ParsingErrorCode>
get_fw_image_hash_range(const ParsingFwType InputParseingFwType);

std::expected<MetadataHashRange, ParsingErrorCode>
get_fw_metadata_hash_range(const ParsingFwType InputParseingFwType);

// namespace {  // In most of cases, please do not use the ImageLayout struct directly, since
// its
//              // size is large.
// struct [[gnu::packed]] ImageLayout
// {
//     // start of the signed image
//     ImageHeader image_header;
//     // start of the NvHeader, the offset should be 0x2b0
//     NvHeader nv_header;

//     // offset should follow the offset_to_extended_header attribute in ImageHeader
//     CertificateBlock certificate_block;
//     // offset should follow the cert_block_size attribute in CertificateBlock
//     // (offset_to_extended_header + cert_block_size)
//     ImageManifestBlock image_manifest_block;
// };
// }  // namespace
}  // namespace nv::fw_parser::mcu