#pragma once

#include <cstdint>
#include <span>
#include <cstddef>
#include <array>
#include "nv/spdm/spdm_crypto_helper.h"
namespace nv::ap_operation {
enum class ApOperationErrorCode : uint8_t
{
    // [TODO] add the error code
    Success = 0,
    Fail,
    InvalidParam,
    Timeout,
    Busy,
    NotSupported,
};

ApOperationErrorCode read_data_from_ap(const uint32_t start_address, std::span<uint8_t> data);
ApOperationErrorCode write_data_to_ap(const uint32_t                  start_address,
                                      const std::span<const uint8_t>& data);

// this will be called by secure boot for ap auth prepare, the calling task will be the spdm
// task. if secure boot get the ApOperationErrorCode != success, the secure boot authenticate
// will be failed.
// Note this will only call for active ap, in other words, the update ap will not call this
// function.
ApOperationErrorCode secure_boot_ap_fw_authenticate_prepare();
// the secure boot will call this function to pass the ap auth result.
ApOperationErrorCode
secure_boot_ap_fw_authenticate_callback(const nv::spdm::crypto::CryptoStatus ap_auth_result);

// this will be called by pldm for ap fw update prepare, the calling task will be the pldm task.
// if pldm get the ApOperationErrorCode != success, the pldm update will be failed.
// Note this will only call for update ap, in other words, the active ap will not call this
// function.
ApOperationErrorCode pldm_update_ap_fw_prepare();
// the pldm will call this function to pass the ap fw update result.
ApOperationErrorCode
pldm_update_ap_fw_callback(const nv::spdm::crypto::CryptoStatus ap_auth_result);

// Write debug token notify bit to CPLD
ApOperationErrorCode modify_cpld_debug_status(bool unlock);

}  // namespace nv::ap_operation
