#include "nv/fw_parser/fw_parser_mcu.h"
#include "nv/flash/flash.h"
#include "sys/fw_parser/fw_parser_mcu.h"
namespace nv::fw_parser::mcu {

Address get_fw_image_address(const ParsingFwType InputParseingFwType)
{
    switch (InputParseingFwType) {
        case ParsingFwType::Fmc         : return FmcFwAddress;
        case ParsingFwType::ActiveSlot  : return ActiveFwAddress;
        case ParsingFwType::InactiveSlot: return InactiveFwAddress;
        case ParsingFwType::Slot0:
            return nv::flash::Flash::get_flash_address(
                Slot0FwAddress, nv::bootloader::Driver::current_boot_index());
        case ParsingFwType::Slot1:
            return nv::flash::Flash::get_flash_address(
                Slot1FwAddress, nv::bootloader::Driver::current_boot_index());
        default: return 0x0; break;
    }
}
// Helper api for getting some common usage info on fw
std::expected<uint32_t, ParsingErrorCode>
get_security_version(const ParsingFwType InputParseingFwType)
{
    return sys::fw_parser::mcu::get_security_version(InputParseingFwType);
}

std::expected<uint32_t, ParsingErrorCode>
get_image_signing_key_version(const ParsingFwType InputParseingFwType)
{
    return sys::fw_parser::mcu::get_image_signing_key_version(InputParseingFwType);
}

std::expected<FirmwareVersionT, ParsingErrorCode>
get_firmware_version(const ParsingFwType InputParseingFwType)
{
    return sys::fw_parser::mcu::get_firmware_version(InputParseingFwType);
}

std::expected<ImageHashRange, ParsingErrorCode>
get_fw_image_hash_range(const ParsingFwType InputParseingFwType)
{
    return sys::fw_parser::mcu::get_fw_image_hash_range(InputParseingFwType);
}

std::expected<MetadataHashRange, ParsingErrorCode>
get_fw_metadata_hash_range(const ParsingFwType InputParseingFwType)
{
    return sys::fw_parser::mcu::get_fw_metadata_hash_range(InputParseingFwType);
}

}  // namespace nv::fw_parser::mcu