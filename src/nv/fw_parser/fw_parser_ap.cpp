#include <cstring>
#include "nv/fw_parser/fw_parser_ap.h"

#include "nv/flash/flash.h"
#include "nv/spdm/secure_boot.h"
#include "nv/spdm/spdm_crypto_helper.h"
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
bool get_ap_metadata_data_from_flash(
    const ParsingApFwType                                input_parsing_ap_fw_type,
    nv::spdm::secure_boot::SecureBoot::AuthenticateData& authenticate_data,
    ApFwParsingErrorCode&                                auth_status)
{
    if (!check_input_parsing_ap_fw_type_valid(input_parsing_ap_fw_type)) {
        auth_status = ApFwParsingErrorCode::InvalidParsingApFwType;
        return false;
    }
    auto npds_key = input_parsing_ap_fw_type == ParsingApFwType::ActiveSlot
                      ? nv::flash::Key::NpdsActiveApFwAuthenticateData
                      : nv::flash::Key::NpdsUpdateApFwAuthenticateData;
    auto status   = nv::flash::Flash::get_ap_fw_authenticate_data(authenticate_data, npds_key);
    if (status != nv::flash::Status::Ok) {
        auth_status = ApFwParsingErrorCode::FlashAccessFail;
        return false;
    }
    if (authenticate_data.ap_auth_result != nv::spdm::crypto::CryptoStatus::Success) {
        auth_status = ApFwParsingErrorCode::AuthenticationFailed;
        return false;
    }
    return true;
}
}  // namespace

std::expected<ApFwMetadata::TbsData, ApFwParsingErrorCode>
get_ap_metadata_data(const ParsingApFwType input_parsing_ap_fw_type)
{
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    ApFwParsingErrorCode                                auth_status{};
    if (!get_ap_metadata_data_from_flash(
            input_parsing_ap_fw_type, authenticate_data, auth_status)) {
        return std::unexpected<ApFwParsingErrorCode>(auth_status);
    }
    return authenticate_data.ap_metadata_tbs_data;
}

std::expected<ApFwVersion, ApFwParsingErrorCode>
get_ap_fw_version(const ParsingApFwType input_parsing_ap_fw_type)
{
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    ApFwParsingErrorCode                                auth_status{};
    if (!get_ap_metadata_data_from_flash(
            input_parsing_ap_fw_type, authenticate_data, auth_status)) {
        return std::unexpected<ApFwParsingErrorCode>(auth_status);
    }
    return authenticate_data.ap_metadata_tbs_data.fw_version;
}

std::expected<uint8_t, ApFwParsingErrorCode>
get_ap_sec_version(const ParsingApFwType input_parsing_ap_fw_type)
{
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    ApFwParsingErrorCode                                auth_status{};
    if (!get_ap_metadata_data_from_flash(
            input_parsing_ap_fw_type, authenticate_data, auth_status)) {
        return std::unexpected<ApFwParsingErrorCode>(auth_status);
    }
    return authenticate_data.ap_metadata_tbs_data.sec_version;
}

std::expected<uint8_t, ApFwParsingErrorCode>
get_ap_build_type(const ParsingApFwType input_parsing_ap_fw_type)
{
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    ApFwParsingErrorCode                                auth_status{};
    if (!get_ap_metadata_data_from_flash(
            input_parsing_ap_fw_type, authenticate_data, auth_status)) {
        return std::unexpected<ApFwParsingErrorCode>(auth_status);
    }
    return authenticate_data.ap_metadata_tbs_data.ap_build_type;
}

std::expected<std::array<uint8_t, 16>, ApFwParsingErrorCode>
get_ap_comp_version_str(const ParsingApFwType input_parsing_ap_fw_type)
{
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    ApFwParsingErrorCode                                auth_status{};
    if (!get_ap_metadata_data_from_flash(
            input_parsing_ap_fw_type, authenticate_data, auth_status)) {
        return std::unexpected<ApFwParsingErrorCode>(auth_status);
    }
    return authenticate_data.ap_metadata_tbs_data.comp_version_str;
}

std::expected<PublicKeyIndex, ApFwParsingErrorCode>
get_ap_signing_key_index(const ParsingApFwType input_parsing_ap_fw_type)
{
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    ApFwParsingErrorCode                                auth_status{};
    if (!get_ap_metadata_data_from_flash(
            input_parsing_ap_fw_type, authenticate_data, auth_status)) {
        return std::unexpected<ApFwParsingErrorCode>(auth_status);
    }

    // check if the signing key is debug or prod
    if (authenticate_data.ap_metadata_tbs_data.verif_pub_key
        == nv::spdm::crypto::ApFwPublicKeys[std::to_underlying(
            PublicKeyIndex::DebugKeyIndex)]) {
        return PublicKeyIndex::DebugKeyIndex;
    }
    else if (authenticate_data.ap_metadata_tbs_data.verif_pub_key
             == nv::spdm::crypto::ApFwPublicKeys[std::to_underlying(
                 PublicKeyIndex::ProdKeyIndex)]) {
        return PublicKeyIndex::ProdKeyIndex;
    }
    else {
        return std::unexpected<ApFwParsingErrorCode>(
            ApFwParsingErrorCode::MismatchPublicKeyIndex);
    }
}

// Get the number of sub-images in the AP firmware
std::expected<uint8_t, ApFwParsingErrorCode>
get_ap_fw_images_count(const ParsingApFwType input_parsing_ap_fw_type)
{
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    ApFwParsingErrorCode                                auth_status{};
    if (!get_ap_metadata_data_from_flash(
            input_parsing_ap_fw_type, authenticate_data, auth_status)) {
        return std::unexpected<ApFwParsingErrorCode>(auth_status);
    }
    return authenticate_data.ap_metadata_tbs_data.ap_fw_images_count;
}

// Get the hash table entry of the AP firmware
std::expected<MetadataHashTableEntry, ApFwParsingErrorCode>
get_ap_hash_table_entry(const ParsingApFwType input_parsing_ap_fw_type, uint8_t index)
{
    if (index
        >= std::tuple_size_v<decltype(nv::fw_parser::ap::ApFwMetadata::TbsData::hash_table)>) {
        return std::unexpected<ApFwParsingErrorCode>(
            ApFwParsingErrorCode::InvalidHashTableEntryIndex);
    }
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    ApFwParsingErrorCode                                auth_status{};
    if (!get_ap_metadata_data_from_flash(
            input_parsing_ap_fw_type, authenticate_data, auth_status)) {
        return std::unexpected<ApFwParsingErrorCode>(auth_status);
    }
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
    return authenticate_data.ap_metadata_tbs_data.hash_table[index];
    // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
}

}  // namespace nv::fw_parser::ap