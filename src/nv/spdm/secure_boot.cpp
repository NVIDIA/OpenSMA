#include "nv/spdm/secure_boot.h"
#include <algorithm>
#include "nv/spdm/task.h"
#include "nv/flash/flash.h"
#include "nv/ap_operation/ap_operation.h"

namespace nv::spdm::secure_boot {

void SecureBoot::secure_boot_main()
{
    auto prepare_result = nv::ap_operation::secure_boot_ap_fw_authenticate_prepare();
    if (prepare_result != nv::ap_operation::ApOperationErrorCode::Success) {
        nv::flash::Flash::set_ap_fw_authenticate_data(
            nv::spdm::secure_boot::SecureBoot::AuthenticateData{
                .ap_auth_result = nv::spdm::crypto::CryptoStatus::FailUnknown,
            },
            nv::flash::Key::NpdsActiveApFwAuthenticateData);
        nv::flash::Flash::set_ap_fw_authenticate_data(
            nv::spdm::secure_boot::SecureBoot::AuthenticateData{
                .ap_auth_result = nv::spdm::crypto::CryptoStatus::FailUnknown,
            },
            nv::flash::Key::NpdsUpdateApFwAuthenticateData);
        return;
    }
    auto auth_result = nv::spdm::crypto::authenticate_ap_firmware(
        nv::fw_parser::ap::ParsingApFwType::ActiveSlot);

    nv::ap_operation::secure_boot_ap_fw_authenticate_callback(auth_result);
}

void SecureBoot::secure_boot_ap_auth_callback(
    const nv::fw_parser::ap::ParsingApFwType        auth_ap_type,
    const nv::spdm::crypto::CryptoStatus            ap_auth_result,
    const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data)
{
    nv::spdm::secure_boot::SecureBoot::AuthenticateData authenticate_data{};
    authenticate_data.ap_metadata_tbs_data = tbs_data;
    authenticate_data.ap_auth_result       = ap_auth_result;
    if (auth_ap_type == nv::fw_parser::ap::ParsingApFwType::ActiveSlot) {
        nv::flash::Flash::set_ap_fw_authenticate_data(
            authenticate_data, nv::flash::Key::NpdsActiveApFwAuthenticateData);
        auto status = nv::flash::Flash::set_ap_fw_authenticate_data(
            authenticate_data, nv::flash::Key::NpdsUpdateApFwAuthenticateData);
        if (status != nv::flash::Status::Ok) {
            (void)nv::flash::Flash::set_data(
                nv::flash::Key::NpdsAp0FwStatus,
                static_cast<nv::flash::Data>(nv::fw_parser::ap::ApFwStatus::Auth_Failed));
        }
        else {
            (void)nv::flash::Flash::set_data(
                nv::flash::Key::NpdsAp0FwStatus,
                static_cast<nv::flash::Data>(nv::fw_parser::ap::ApFwStatus::Sb_Auth_Success));
        }
    }
    else if (auth_ap_type == nv::fw_parser::ap::ParsingApFwType::UpdateSlot) {
        nv::flash::Flash::set_ap_fw_authenticate_data(
            authenticate_data, nv::flash::Key::NpdsUpdateApFwAuthenticateData);
    }

    if (ap_auth_result != nv::spdm::crypto::CryptoStatus::Success) {
        (void)nv::flash::Flash::set_data(
            nv::flash::Key::NpdsAp0FwStatus,
            static_cast<nv::flash::Data>(nv::fw_parser::ap::ApFwStatus::Auth_Failed));
    }
}

void SecureBoot::persist_ap_fw_authenticate_data_in_progress(
    const nv::fw_parser::ap::ParsingApFwType        auth_ap_type,
    const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data)
{
    AuthenticateData authenticate_data{};
    authenticate_data.ap_metadata_tbs_data = tbs_data;
    authenticate_data.ap_auth_result       = nv::spdm::crypto::CryptoStatus::ApAuthInProgress;
    if (auth_ap_type == nv::fw_parser::ap::ParsingApFwType::ActiveSlot) {
        (void)nv::flash::Flash::set_ap_fw_authenticate_data(
            authenticate_data, nv::flash::Key::NpdsActiveApFwAuthenticateData);
        (void)nv::flash::Flash::set_ap_fw_authenticate_data(
            authenticate_data, nv::flash::Key::NpdsUpdateApFwAuthenticateData);
    }
}

}  // namespace nv::spdm::secure_boot