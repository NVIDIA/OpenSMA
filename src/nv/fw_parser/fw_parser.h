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
namespace nv::fw_parser {

using Address = uint32_t;
struct [[gnu::packed]] ImageHeader
{
    // start of the signed image
    uint32_t                initial_sp;      // Stack pointer
    uint32_t                initial_pc;      // Application's first execution instruction
    std::array<uint8_t, 24> vector_table_1;  //	Cortex-M33 core's vector table entries

    uint32_t image_length;  // Length of the current image (total length, including the
                            // signature), which is set to the actual image length if the image
                            // type is another value	O
    uint32_t image_type;    // bit[10], bit[31:16] = version

    uint32_t offset_to_extended_header;

    std::array<uint8_t, 8> vector_table_2;           // Cortex-M33 core's vector table entries
    uint32_t               image_execution_address;  // XIP or RAM
    // std::array<uint8_t, 0x278> vector_table_3;  // Cortex-M33 core's vector table entries
};
constexpr uint32_t NvHeaderOffset = 0x2b0;
struct [[gnu::packed]] NvHeader
{
    std::array<uint8_t, 4> sync_nymber;
    uint16_t               header_version;
    uint16_t               major_version;
    uint8_t                minor_version;

    uint16_t patch_version;

    uint16_t build_version;

    uint32_t security_version_info;
    uint32_t apsku_id;

    uint16_t pci_vendor_id;

    uint16_t pci_device_id;

    uint16_t pci_subsystem_vendor_id;

    uint16_t pci_subsystem_id;

    uint32_t key_revocation_list;

    uint16_t    nv_header_length;
    static bool validate_nv_header(const NvHeader& InputNvHeader)
    {
        if (InputNvHeader.sync_nymber != std::array<uint8_t, 4>{'N', 'V', 'D', 'A'}) {
            return false;
        }
        return true;
    }
};
struct [[gnu::packed]] CertificateBlock
{
    uint32_t magic_value = 0x72646863u;
    uint32_t format_version;
    uint32_t cert_block_size;
    struct [[gnu::packed]] RoTRecord
    {
        uint32_t flags; /* little endian
        flags[bit 31]: NoCA flag, if set to 0, used RoTK acts as Certificate Authority and is
        used to sign ISK certificate, does not sign the full image. If set to 1, used RoTK does
        not act as Certificate Authority and signs directly the full image or SB3 Block0. If the
        NoCa flag is set to 1, then the iskCertificate section is not present in the certificate
        block. flags[bits 30:12]: Reserved for future use. flags[bits 7:4]: Used root cert
        number [0-3] (specify root cert used to ISK/image signature). flags[bits3:0]: Type of
        root certificate, secp256r1 = 0x1u or secp384r1 = 0x2u, other values are reserved.
        */

        struct [[gnu::packed]] RoTKT
        {
            std::array<uint8_t, 48> rot_key_hash_0;
            std::array<uint8_t, 48> rot_key_hash_1;
            std::array<uint8_t, 48> rot_key_hash_2;
            std::array<uint8_t, 48> rot_key_hash_3;
        } rot_key_table;

        std::array<uint8_t, 96> rot_key_pub;

    } rot_record;
    struct IskCertificate
    {
        uint32_t signature_offset;
        uint32_t constraint;  // this is act as image key version.
        uint32_t flags;
        // little endian
        //  flags[bit 31]: User data flag, if set to 1, user data are included in ISK
        //  certificate. flags[bits 30:4]: Reserved for future use. flags[3:0]: Type of ISK
        //  certificate, secp256r1 = 0x1u or secp384r1 = 0x2u, other values are reserved

    } isk_certificate;

    static bool validate_certificate_block(const CertificateBlock& InputCertificateBlock)
    {
        constexpr uint32_t expected_magic_value = 0x72646863u;
        if (InputCertificateBlock.magic_value != expected_magic_value) {
            return false;
        }
        return true;
    };
};
struct [[gnu::packed]] ImageManifestBlock
{
    uint32_t    magic_value = 0x6D676D69u;
    uint32_t    format_version;
    uint32_t    firmware_version;
    uint32_t    image_manifest_size;
    static bool validate_image_manifest_block(const ImageManifestBlock& InputImageManifestBlock)
    {
        constexpr uint32_t expected_magic_value = 0x6D676D69u;
        if (InputImageManifestBlock.magic_value != expected_magic_value) {
            return false;
        }
        return true;
    };
};

enum class ParsingFwType : uint8_t
{
    Begin        = 0x00,
    Fmc          = 0x01,
    ActiveSlot   = 0x02,
    InactiveSlot = 0x03,
    Slot0        = 0x04,
    Slot1        = 0x05,
    End,

};

enum class ParsingErrorCode : uint8_t
{
    NvHeaderSyncNumberNotMatch           = 0x01,
    CertificateBlockMagicValueNotMatch   = 0x02,
    ImageManifestBlockMagicValueNotMatch = 0x03,
    FlashAccessFail                      = 0x04,
    ParsingCertificateBlockAddressWrap   = 0x05,
    CertificateBlockAddressWrap          = 0x06
};
// Api for get the each block struct on fw.
std::expected<ImageHeader, ParsingErrorCode>
get_image_header(const ParsingFwType InputParseingFwType);

std::expected<ImageManifestBlock, ParsingErrorCode>
get_image_manifest(const ParsingFwType InputParseingFwType);

std::expected<CertificateBlock, ParsingErrorCode>
get_certificate_block(const ParsingFwType InputParseingFwType);

std::expected<NvHeader, ParsingErrorCode>
get_nv_header(const ParsingFwType InputParseingFwType);

// Api for get the each block address in flash.
Address get_fw_image_address(const ParsingFwType InputParseingFwType);

Address get_nv_header_address(const ParsingFwType InputParseingFwType);

std::expected<Address, ParsingErrorCode>
get_certificate_block_address(const ParsingFwType InputParseingFwType);

std::expected<Address, ParsingErrorCode>
get_image_manifest_block_address(const ParsingFwType InputParseingFwType);

// Helper api for getting some common usage info on fw
std::expected<uint32_t, ParsingErrorCode>
get_security_version(const ParsingFwType InputParseingFwType);

std::expected<uint32_t, ParsingErrorCode>
get_image_signing_key_version(const ParsingFwType InputParseingFwType);

typedef struct [[gnu::packed]]
{
    uint16_t build;
    uint16_t patch;
    uint8_t  minor;
    uint16_t major;
} FirmwareVersionT;

std::expected<FirmwareVersionT, ParsingErrorCode>
get_firmware_version(const ParsingFwType InputParseingFwType);

constexpr Address FmcFwAddress      = sys::flash::config::FmcFwAddress;
constexpr Address ActiveFwAddress   = 0x0;
constexpr Address InactiveFwAddress = sys::flash::config::Slot1FwAddress;
constexpr Address Slot0FwAddress    = 0x0;
constexpr Address Slot1FwAddress    = sys::flash::config::Slot1FwAddress;

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
}  // namespace nv::fw_parser