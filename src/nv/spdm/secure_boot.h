#pragma once

#include "sys/ipc/event.h"
#include "nv/ipc/supervisor.h"
#include "nv/fw_parser/fw_parser_mcu.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/fw_parser/fw_parser_ap.h"
namespace nv::spdm::secure_boot {

class SecureBoot
{
public:
    struct AuthenticateData
    {
        nv::fw_parser::ap::ApFwMetadata::TbsData ap_metadata_tbs_data{};
        nv::spdm::crypto::CryptoStatus           ap_auth_result{};
    };
    SecureBoot()  = default;
    ~SecureBoot() = default;
    void        secure_boot_main();
    static void secure_boot_ap_auth_callback(
        const nv::fw_parser::ap::ParsingApFwType        auth_ap_type,
        const nv::spdm::crypto::CryptoStatus            ap_auth_result,
        const nv::fw_parser::ap::ApFwMetadata::TbsData& authenticate_data);
    // Persist metadata read from AP before verification completes; does not change Ap0FwStatus.
    static void persist_ap_fw_authenticate_data_in_progress(
        const nv::fw_parser::ap::ParsingApFwType        auth_ap_type,
        const nv::fw_parser::ap::ApFwMetadata::TbsData& tbs_data);
};

}  // namespace nv::spdm::secure_boot