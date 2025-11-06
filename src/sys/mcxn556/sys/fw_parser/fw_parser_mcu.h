#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include "image_layout.h"
namespace sys::fw_parser::mcu {

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

}  // namespace sys::fw_parser::mcu