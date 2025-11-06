#include "nv/fw_parser/fw_parser_ap.h"

#include "nv/flash/flash.h"
#include "nv/spdm/secure_boot.h"
namespace nv::fw_parser::ap {
namespace {
bool check_input_parsing_ap_fw_type_valid(const ParsingApFwType input_parsing_ap_fw_type)
{
    if (input_parsing_ap_fw_type != ParsingApFwType::ActiveSlot
        && input_parsing_ap_fw_type != ParsingApFwType::UpdateSlot) {
        return false;
    }
    return true;
}
nv::flash::Status get_ap_metadata_data_from_flash(
    const ParsingApFwType                                input_parsing_ap_fw_type,
    nv::spdm::secure_boot::SecureBoot::AuthenticateData& authenticate_data)
{
    auto npds_key = input_parsing_ap_fw_type == ParsingApFwType::ActiveSlot
                      ? nv::flash::Key::NpdsActiveApFwAuthenticateData
                      : nv::flash::Key::NpdsUpdateApFwAuthenticateData;
    return nv::flash::Flash::get_ap_fw_authenticate_data(authenticate_data, npds_key);
}
}  // namespace
std::expected<ApFwMetadata::TbsData, ApFwParsingErrorCode>
get_ap_metadata_data(const ParsingApFwType input_parsing_ap_fw_type)
{
    if (!check_input_parsing_ap_fw_type_valid(input_parsing_ap_fw_type)) {
        return std::unexpected<ApFwParsingErrorCode>(
            ApFwParsingErrorCode::InvalidParsingApFwType);
    }
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    auto flash_status = get_ap_metadata_data_from_flash(input_parsing_ap_fw_type,
                                                        authenticate_data);
    if (flash_status != nv::flash::Status::Ok) {
        return std::unexpected<ApFwParsingErrorCode>(ApFwParsingErrorCode::FlashAccessFail);
    }
    if (authenticate_data.ap_auth_result != nv::spdm::crypto::CryptoStatus::Success) {
        return std::unexpected<ApFwParsingErrorCode>(
            ApFwParsingErrorCode::AuthenticationFailed);
    }

    return authenticate_data.ap_metadata_tbs_data;
}

std::expected<ApFwVersion, ApFwParsingErrorCode>
get_ap_fw_version(const ParsingApFwType input_parsing_ap_fw_type)
{
    if (!check_input_parsing_ap_fw_type_valid(input_parsing_ap_fw_type)) {
        return std::unexpected<ApFwParsingErrorCode>(
            ApFwParsingErrorCode::InvalidParsingApFwType);
    }

    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    auto flash_status = get_ap_metadata_data_from_flash(input_parsing_ap_fw_type,
                                                        authenticate_data);
    if (flash_status != nv::flash::Status::Ok) {
        return std::unexpected<ApFwParsingErrorCode>(ApFwParsingErrorCode::FlashAccessFail);
    }
    if (authenticate_data.ap_auth_result != nv::spdm::crypto::CryptoStatus::Success) {
        return std::unexpected<ApFwParsingErrorCode>(
            ApFwParsingErrorCode::AuthenticationFailed);
    }

    return authenticate_data.ap_metadata_tbs_data.fw_version;
}

}  // namespace nv::fw_parser::ap