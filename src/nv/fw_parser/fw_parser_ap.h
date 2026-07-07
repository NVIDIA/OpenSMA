#pragma once
#include <array>
#include <cstdint>
#include <expected>

namespace nv::fw_parser::ap {

enum class ApFwStatus : uint8_t
{
    Boot_Complete                    = 0xff,
    Update_Complete                  = 0x1,
    Update_Complete_But_Not_Activate = 0x2,
    Update_In_Progress               = 0x3,
    Auth_In_Progress                 = 0x4,
    Auth_Failed                      = 0x5,
    // Auth signature checks passed but the post-auth physical sequence
    // (release_reset, post_authenticate, check_booted) failed. Distinguished
    // from Auth_Failed so BMC/diag don't misread it as a signature failure.
    Boot_Failed   = 0x6,
    Not_Supported = 0x00
};

enum class ParsingApFwType : uint8_t
{
    Begin,
    ActiveSlot,
    UpdateSlot,  // for pldm update fw
    End,
};

enum class PublicKeyIndex : uint32_t
{
    DebugKeyIndex = 0,
    ProdKeyIndex  = 1,
    KeyIndexCount = 2,
};

// ECDSA P-384 signature structure with separated r and s components
struct [[gnu::packed]] NvSignature
{
    std::array<uint8_t, 48> r;  // R component of ECDSA P-384 signature
    std::array<uint8_t, 48> s;  // S component of ECDSA P-384 signature
};

struct [[gnu::packed]] ApFwVersion
{
    uint8_t build;
    uint8_t patch;
    uint8_t minor;
    uint8_t major;
};

// Compile-time assertion to ensure the signature struct is exactly 96 bytes
static_assert(sizeof(NvSignature) == 96, "nv_signature must be exactly 96 bytes");

struct [[gnu::packed]] MetadataRegionAttributeEntry
{
    uint32_t base_address;  // Offset relative to physical SPI flash of the region
    uint32_t size;          // Size of the region
    uint32_t flags;  // OR of bitmask flags (ALLOW_READ: 0x1, ALLOW_WRITE: 0x2, ALLOW_32K_ERASE:
                     // 0x4, ALLOW_64K_ERASE: 0x8)

    // Flag constants for easier use
    static constexpr uint32_t ALLOW_READ      = 0x1;
    static constexpr uint32_t ALLOW_WRITE     = 0x2;
    static constexpr uint32_t ALLOW_32K_ERASE = 0x4;
    static constexpr uint32_t ALLOW_64K_ERASE = 0x8;
};

// Compile-time assertion to ensure the region attribute entry is exactly 12 bytes
static_assert(sizeof(MetadataRegionAttributeEntry) == 12,
              "MetadataRegionAttributeEntry must be exactly 12 bytes");

struct [[gnu::packed]] MetadataHashTableEntry
{
    uint32_t                offset;  // Base address of the sub-image relative to the partition
    uint32_t                length;  // Size of the sub-images
    uint64_t                ap_specific_info;  // Not in use now
    std::array<uint8_t, 48> hash;              // SHA384 of the sub-image
};

// Compile-time assertion to ensure the hash table entry is exactly 64 bytes
static_assert(sizeof(MetadataHashTableEntry) == 64,
              "MetadataHashTableEntry must be exactly 64 bytes");

struct [[gnu::packed]] ApFwMetadata
{
    struct [[gnu::packed]] TbsData
    {
        // Basic metadata info (offset 0-15)
        uint16_t id;            // 0x0000 - AP_METADATA ID
        uint16_t revision;      // 0x0200 - AP_METADATA Revision
        uint32_t image_offset;  // Offset where concatenated images can be found in the blob
        uint32_t flash_offset_partition;  // Offset where concatenated images can be found in
                                          // flash
        uint8_t ap_cfg_key_idx;           // Same as config.SecInfo.ap_cfg_key_idx
        uint8_t ap_fw_images_count;       // Number of sub-images
        uint8_t sec_version;              // Same as config.SecInfo.sec_version
        uint8_t ap_strap_value;           // Same as config.SecInfo.strap

        // Version and date info (offset 16-31)
        ApFwVersion fw_version;  // Same as config.SecInfo.fw_version
        uint32_t build_date;  // 32-bit date value: B[31:24]=month, B[23:16]=day, B[15:0]=year
        uint64_t key_revocation_info;  // Same as config.SecInfo.key_revocation_info

        // SPI configuration (offset 32-159)
        std::array<uint8_t, 32> spi_opcode_allow_list;  // Allowed AP SPI flash opcode
        std::array<MetadataRegionAttributeEntry, 8> spi_flash_rgn_attr;  // Array of 8 Metadata
                                                                         // Region Attribute
                                                                         // Entry (12 bytes
                                                                         // each)

        // Hash table and verification (offset 160-671)
        std::array<MetadataHashTableEntry, 8> hash_table;  // Array of 8 Metadata Hash Table
                                                           // Entry (64 bytes each)

        // Verification key (offset 672-767)
        std::array<uint8_t, 96> verif_pub_key;  // ECDSA public key that used to sign the
                                                // metadata

        // Security and customization (offset 768-847)
        std::array<uint8_t, 16> sec_version_info;  // Security version rollback protection mask
        std::array<uint8_t, 64> customize;  // Type-Length-Value (TLV) based customization

        // AP configuration (offset 848-915)
        uint8_t                 wp_region;         // Write protection region for the AP SPI
        std::array<uint8_t, 39> ap_strap_setting;  // AP strap setting (including dev mode
                                                   // option)
        std::array<uint8_t, 24> reserved;          // Reserved bytes, must be 0
        std::array<uint8_t, 4>  reserved_for_ap_sku_id_alt;  // Reserved for future SKU
                                                             // conversion solution

        // SKU and PCI info (offset 916-929)
        uint32_t ap_sku_id;             // [23:0] = AP SKU ID, [31:24] = Reserved
        uint16_t pci_vendor_id;         // PCI Vendor ID
        uint16_t pci_device_id;         // PCI Device ID
        uint16_t pci_subsys_vendor_id;  // PCI Subsystem Vendor ID
        uint16_t pci_subsystem_id;      // PCI Subsystem ID

        // Payload and signature info (offset 930-949)
        uint16_t nv_payload_size;      // Number of bytes from start of metadata up until
                                       // corresponding signature
        uint16_t nv_signature_offset;  // Pointer to the corresponding nv signature
        uint16_t dot_field_offset;     // Offset address to DOT Fields
        std::array<uint8_t, 16> comp_version_str;  // String used for ComponentVersionString
        uint8_t                 ap_build_type;     // AP Build Type
    } tbs_data;
    // Signature (offset 951-1046)
    NvSignature nv_signature;  // ECDSA P-384 signature with r and s components

    // Final reserved space (offset 1047-4095)
    std::array<uint8_t, 3049> reserved_2;  // Reserved bytes, must be 0
};
enum class ApFwParsingErrorCode : uint8_t
{
    AuthenticationFailed       = 0x02,
    FlashAccessFail            = 0x03,
    InvalidParsingApFwType     = 0x04,
    MismatchPublicKeyIndex     = 0x05,
    InvalidHashTableEntryIndex = 0x06,
};

std::expected<ApFwMetadata::TbsData, ApFwParsingErrorCode>
get_ap_metadata_data(const ParsingApFwType input_parsing_ap_fw_type);

std::expected<ApFwVersion, ApFwParsingErrorCode>
get_ap_fw_version(const ParsingApFwType input_parsing_ap_fw_type);

std::expected<uint8_t, ApFwParsingErrorCode>
get_ap_sec_version(const ParsingApFwType input_parsing_ap_fw_type);

std::expected<uint8_t, ApFwParsingErrorCode>
get_ap_build_type(const ParsingApFwType input_parsing_ap_fw_type);

std::expected<std::array<uint8_t, 16>, ApFwParsingErrorCode>
get_ap_comp_version_str(const ParsingApFwType input_parsing_ap_fw_type);

std::expected<PublicKeyIndex, ApFwParsingErrorCode>
get_ap_signing_key_index(const ParsingApFwType input_parsing_ap_fw_type);

// Get the number of sub-images in the AP firmware
std::expected<uint8_t, ApFwParsingErrorCode>
get_ap_fw_images_count(const ParsingApFwType input_parsing_ap_fw_type);

// Get the hash table entry of the AP firmware
std::expected<MetadataHashTableEntry, ApFwParsingErrorCode>
get_ap_hash_table_entry(const ParsingApFwType input_parsing_ap_fw_type, uint8_t index);

// Compile-time assertion to ensure the struct is exactly 4096 bytes
static_assert(sizeof(ApFwMetadata) == 4096, "ApFwMetadata must be exactly 4096 bytes");
}  // namespace nv::fw_parser::ap
