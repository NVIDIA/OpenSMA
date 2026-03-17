#include "nv/ap_operation/ap_operation.h"
#include "nv/debugtoken/debugtoken.h"
#include "nv/i2c/lattice_driver.h"

#include <array>
#include <cstring>
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/logger/log.h"
#include "sys/i2c/utils.h"
#include NV_IPC_CONFIG_H

using namespace nv::ipc;
using namespace nv;

void clear_cpld_program_pin();

namespace nv::ap_operation {

ApOperationErrorCode read_metadata_from_ap([[maybe_unused]] const uint32_t     start_address,
                                           [[maybe_unused]] std::span<uint8_t> data)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        auto& cpld   = i2c::LatticeCpld::inst();
        auto  status = cpld.read_ufm(
            data.data(), static_cast<uint32_t>(data.size()), start_address);

        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        return ApOperationErrorCode::Success;
    }
}

ApOperationErrorCode read_image_from_ap([[maybe_unused]] const uint32_t     start_address,
                                        [[maybe_unused]] std::span<uint8_t> data)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        auto& cpld = i2c::LatticeCpld::inst();

        auto status = cpld.read_offset(
            data.data(), start_address, static_cast<uint32_t>(data.size()));

        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        return ApOperationErrorCode::Success;
    }
}

ApOperationErrorCode write_metadata_to_ap([[maybe_unused]] const uint32_t start_address,
                                          [[maybe_unused]] const std::span<const uint8_t>& data)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        auto& cpld = i2c::LatticeCpld::inst();
        // write ufm from start_address
        auto status = cpld.write_ufm(
            data.data(), static_cast<uint32_t>(data.size()), start_address, false);

        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        return ApOperationErrorCode::Success;
    }
}

ApOperationErrorCode write_image_to_ap([[maybe_unused]] const uint32_t start_address,
                                       [[maybe_unused]] const std::span<const uint8_t>& data)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        auto& cpld   = i2c::LatticeCpld::inst();
        auto  status = cpld.write_offset(
            data.data(), start_address, static_cast<uint32_t>(data.size()));

        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        return ApOperationErrorCode::Success;
    }
}

ApOperationErrorCode read_data_from_ap([[maybe_unused]] const uint32_t     start_address,
                                       [[maybe_unused]] std::span<uint8_t> data)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        const size_t size_metadata = sizeof(fw_parser::ap::ApFwMetadata);

        // metadata
        if (start_address < size_metadata) {
            return read_metadata_from_ap(start_address, data);
        }
        // image
        else {
            return read_image_from_ap(start_address - size_metadata, data);
        }
    }
}

ApOperationErrorCode write_data_to_ap([[maybe_unused]] const uint32_t start_address,
                                      [[maybe_unused]] const std::span<const uint8_t>& data)
{
    const size_t size_metadata = sizeof(fw_parser::ap::ApFwMetadata);

    // metadata
    if (start_address < size_metadata) {
        return write_metadata_to_ap(start_address, data);
    }
    // image
    else {
        return write_image_to_ap(start_address - size_metadata, data);
    }
}

ApOperationErrorCode secure_boot_ap_fw_authenticate_prepare()
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        auto& cpld = i2c::LatticeCpld::inst();

        auto status = cpld.enter_transparent_mode();

        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        return ApOperationErrorCode::Success;
    }
}

ApOperationErrorCode secure_boot_ap_fw_authenticate_callback(
    [[maybe_unused]] const nv::spdm::crypto::CryptoStatus ap_auth_result)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        nv::info("CPLD FW Auth Status at cold boot: %d\n", ap_auth_result);

        auto& cpld = i2c::LatticeCpld::inst();

        auto status = cpld.exit_transparent_mode();

        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        if constexpr (CPLD_ProgramN_Pin_Enabled) {
            if (ap_auth_result == nv::spdm::crypto::CryptoStatus::Success) {
                clear_cpld_program_pin();
            }
        }

        // Sync debug token status after AP FW is ready
        nv::debugtoken::sync_cpld_debug_token_on_boot();

        return ApOperationErrorCode::Success;
    }
}

ApOperationErrorCode pldm_update_ap_fw_prepare()
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        auto& cpld = i2c::LatticeCpld::inst();

        auto status = cpld.enter_transparent_mode();
        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        status = cpld.erase();
        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        return ApOperationErrorCode::Success;
    }
}

ApOperationErrorCode
pldm_update_ap_fw_callback([[maybe_unused]] const nv::spdm::crypto::CryptoStatus ap_auth_result)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }
    else {
        auto& cpld   = i2c::LatticeCpld::inst();
        auto  status = cpld.update_complete();

        if (status != i2c::I2cStatus::Ok) {
            return ApOperationErrorCode::Fail;
        }

        return ApOperationErrorCode::Success;
    }
}

ApOperationErrorCode modify_cpld_debug_status([[maybe_unused]] bool unlock)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOperationErrorCode::NotSupported;
    }

    const uint8_t value = (unlock) ? Cpld_User_Reg::MCU_UNLOCK_EN_UNLOCK
                                   : Cpld_User_Reg::MCU_UNLOCK_EN_LOCK;

    auto& cpld   = i2c::LatticeCpld::inst();
    auto  status = cpld.write_debug_bit(value);
    if (status != i2c::I2cStatus::Ok) {
        return ApOperationErrorCode::Fail;
    }

    return ApOperationErrorCode::Success;
}

}  // namespace nv::ap_operation
