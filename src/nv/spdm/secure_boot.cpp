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
        nv::info("secure_boot_ap_fw_authenticate_prepare: %d", prepare_result);
        auto status = nv::flash::Flash::set_ap_fw_authenticate_data(
            nv::spdm::secure_boot::SecureBoot::AuthenticateData{
                .ap_auth_result = nv::spdm::crypto::CryptoStatus::FailUnknown,
            },
            nv::flash::Key::NpdsActiveApFwAuthenticateData);
        if (status != nv::flash::Status::Ok) {
            nv::info("secure_boot_main: set ActiveSlot ap fw authenticate data failed\n");
        }
        status = nv::flash::Flash::set_ap_fw_authenticate_data(
            nv::spdm::secure_boot::SecureBoot::AuthenticateData{
                .ap_auth_result = nv::spdm::crypto::CryptoStatus::FailUnknown,
            },
            nv::flash::Key::NpdsUpdateApFwAuthenticateData);
        if (status != nv::flash::Status::Ok) {
            nv::info("secure_boot_main: set UpdateSlot ap fw authenticate data failed\n");
        }
        return;
    }
    auto auth_result = nv::spdm::crypto::authenticate_ap_firmware(
        nv::fw_parser::ap::ParsingApFwType::ActiveSlot);

    auto callback_result = nv::ap_operation::secure_boot_ap_fw_authenticate_callback(
        auth_result);
    if (callback_result != nv::ap_operation::ApOperationErrorCode::Success) {
        nv::info("secure_boot_ap_fw_authenticate_callback: %d", callback_result);
        return;
    }
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
        nv::info("secure_boot_ap_auth_callback: ActiveSlot, ap_auth_result: %d\n",
                 ap_auth_result);
        auto status = nv::flash::Flash::set_ap_fw_authenticate_data(
            authenticate_data, nv::flash::Key::NpdsActiveApFwAuthenticateData);

        if (status != nv::flash::Status::Ok) {
            nv::info(
                "secure_boot_ap_auth_callback: ActiveSlot, set ap fw authenticate data "
                "failed\n");
        }
        status = nv::flash::Flash::set_ap_fw_authenticate_data(
            authenticate_data, nv::flash::Key::NpdsUpdateApFwAuthenticateData);
        if (status != nv::flash::Status::Ok) {
            nv::info(
                "secure_boot_ap_auth_callback: UpdateSlot, set ap fw authenticate data "
                "failed\n");
        }
    }
    else if (auth_ap_type == nv::fw_parser::ap::ParsingApFwType::UpdateSlot) {
        nv::info("secure_boot_ap_auth_callback: UpdateSlot, ap_auth_result: %d\n",
                 ap_auth_result);
        auto status = nv::flash::Flash::set_ap_fw_authenticate_data(
            authenticate_data, nv::flash::Key::NpdsUpdateApFwAuthenticateData);
        if (status != nv::flash::Status::Ok) {
            nv::info(
                "secure_boot_ap_auth_callback: UpdateSlot, set ap fw authenticate data "
                "failed\n");
        }
    }
}

}  // namespace nv::spdm::secure_boot