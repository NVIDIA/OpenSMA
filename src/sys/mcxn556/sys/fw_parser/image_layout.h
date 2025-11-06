#pragma once

#include <array>
#include <cstdint>
#ifdef CPU_MCXN547VDF
namespace sys::fw_parser::mcu {
using Address = uint32_t;
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
typedef struct [[gnu::packed]]
{
    uint16_t build;
    uint16_t patch;
    uint8_t  minor;
    uint16_t major;
} FirmwareVersionT;

enum class ParsingErrorCode : uint8_t
{
    NvHeaderSyncNumberNotMatch           = 0x01,
    CertificateBlockMagicValueNotMatch   = 0x02,
    ImageManifestBlockMagicValueNotMatch = 0x03,
    FlashAccessFail                      = 0x04,
    ParsingCertificateBlockAddressWrap   = 0x05,
    CertificateBlockAddressWrap          = 0x06,
    NvHeaderAddressWrap                  = 0x07
};
struct [[gnu::packed]] HashRangeItem
{
    uint32_t start_address;
    uint32_t length;
};
using ImageHashRange    = std::array<HashRangeItem, 1>;
using MetadataHashRange = std::array<HashRangeItem, 2>;

}  // namespace sys::fw_parser::mcu
#else
namespace sys::fw_parser::mcu {

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

using Address = uint32_t;
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

// Container Header (16 bytes)
struct __attribute__((packed)) ContainerHeader
{
    uint8_t     version;         // Must be 0x2
    uint16_t    length;          // Size of container in bytes
    uint8_t     tag;             // Container: 0x87
    uint32_t    flags;           // SRK settings and flags
    uint16_t    sw_version;      // Used to compare against Secure_FW_Version
    uint8_t     fuse_version;    // Used to compare against Secure_FW_Version
    uint8_t     num_images;      // Number of images in the image array
    uint16_t    sig_blk_offset;  // Offset in bytes to signature block
    uint16_t    reserved;        // Reserved, filled with 0
    static bool validate_container_header(const ContainerHeader& InputContainerHeader)
    {
        if (InputContainerHeader.version != 0x2 || InputContainerHeader.tag != 0x87) {
            return false;
        }
        return true;
    }
};

// Signature Block Header (16 bytes)
struct __attribute__((packed)) SignatureBlockHeader
{
    uint8_t  version;             // Must be 0x1
    uint16_t length;              // Size of signature block in bytes
    uint8_t  tag;                 // Must be 0x90
    uint16_t certificate_offset;  // Offset to signing key certificate
    uint16_t srk_table_offset;    // Offset to SRK table array (0x0000=not present)
    uint16_t signature_offset;    // Offset to signature
    uint16_t blob_offset;         // Offset to DEK Blob (0x0000=no encryption)
    uint32_t reserved;            // Reserved
    static bool
    validate_signature_block_header(const SignatureBlockHeader& InputSignatureBlockHeader)
    {
        if (InputSignatureBlockHeader.version != 0x1 || InputSignatureBlockHeader.tag != 0x90) {
            return false;
        }
        return true;
    }
};
// Certificate Block Header
struct __attribute__((packed)) CertificateBlockHeader
{
    uint8_t  version;           // Must be 0x2
    uint16_t length;            // Size of the certificate
    uint8_t  tag;               // 0xAF
    uint16_t signature_offset;  // Offset of first signature from beginning of certificate
    uint16_t perm;              // Permissions associated with certificate
    uint8_t  perm_data[12];  // Perm Data (96 bits) - complementary info for debug auth feature
    uint8_t  fuse_version;   // Version of certificate
    uint8_t  uuid[16];       // UUID (128 bits) - Unique ID of targeted device (NXP Certs)

    static bool
    validate_certificate_block_header(const CertificateBlockHeader& InputCertificateBlockHeader)
    {
        if (InputCertificateBlockHeader.version != 0x2
            || InputCertificateBlockHeader.tag != 0xAF) {
            return false;
        }
        return true;
    }
};

typedef struct [[gnu::packed]]
{
    uint16_t build;
    uint16_t patch;
    uint8_t  minor;
    uint16_t major;
} FirmwareVersionT;

enum class ParsingErrorCode : uint8_t
{
    NvHeaderSyncNumberNotMatch               = 0x01,
    NvHeaderAddressWrap                      = 0x02,
    FlashAccessFail                          = 0x03,
    ParsingCertificateBlockAddressWrap       = 0x04,
    CertificateBlockHeaderMagicValueNotMatch = 0x05,
    SignatureBlockHeaderMagicValueNotMatch   = 0x06,
    ContainerHeaderMagicValueNotMatch        = 0x07,
    CertificateBlockHeaderAddressWrap        = 0x08,
    SignatureBlockHeaderAddressWrap          = 0x09,
    ContainerHeaderAddressWrap               = 0x0A,
    FwNotUseIskCertificate                   = 0x0B
};
struct [[gnu::packed]] HashRangeItem
{
    uint32_t start_address;
    uint32_t length;
};
using ImageHashRange    = std::array<HashRangeItem, 1>;
using MetadataHashRange = std::array<HashRangeItem, 2>;
}  // namespace sys::fw_parser::mcu
#endif