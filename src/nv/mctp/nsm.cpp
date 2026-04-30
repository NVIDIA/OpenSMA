/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nv/mctp/nsm.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <variant>

#include "corepdk/platforms/mcxn236/pldm-fd/src/pldm_wrap.h"

#include "nv/bootloader.h"
#include "nv/flash/flash.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/fw_parser/fw_parser_mcu.h"
#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/interface.h"
#include "nv/nv.h"
#include "nv/pldm/task.h"
#include NV_IPC_CONFIG_H
#include "nv/iox/task.h"
#include "sys/flash/flash_config.h"

using namespace nv;
using namespace mctp;
using namespace std::chrono_literals;
using namespace sys::flash::config;

extern "C" void
ada_populate_stamp(uint8_t minor, uint16_t patch, uint16_t build, uint32_t* stamp);

bool mctp::Nsm::can_revoke_otp(Rcode& reason_code)
{
    flash::Data state{};

    // Check PdsUpdateState
    if (flash::Flash::get_data(flash::Key::PdsUpdateState, state) != flash::Status::Ok
        || state != static_cast<flash::Data>(bootloader::Driver::State::Idle)) {
        reason_code = Rcode::ErrorPldmProcessing;
        return false;
    }

    // Check PdsBootableSlot0 and PdsBootableSlot1
    for (const auto& key : {flash::Key::PdsBootableSlot0, flash::Key::PdsBootableSlot1}) {
        if (flash::Flash::get_data(key, state) != flash::Status::Ok || state == 0) {
            reason_code = Rcode::Null;
            return false;
        }
    }

    // If background copy enabled, complete background copy
    nv::flash::ProgressPercent progress{};
    if (flash::Flash::background_copy_query(progress)
        == flash::Status::BackgroundCopyInprogress) {
        reason_code = Rcode::Null;
        return false;
    }

    return true;
}

bool mctp::Nsm::can_initiate_image_copy(Ccode& completion_code, Rcode& reason_code)
{
    flash::Data state{};
    auto        flash_status = flash::Status::Ok;
    // Check PdsUpdateState - check if pldm in processing
    if (flash::Flash::get_data(flash::Key::PdsUpdateState, state) != flash::Status::Ok
        || state == static_cast<flash::Data>(bootloader::Driver::State::InProgress)) {
        completion_code = Ccode::ErrorInvalidStateForCommand;
        reason_code     = Rcode::UpdateInProgress;
        return false;
    }

    // Check if boot complete
    const auto ActiveSlot      = bootloader::Driver::current_boot_index();
    auto       active_slot_pds = ActiveSlot == bootloader::Driver::ImageIndex::Image0
                                   ? flash::Key::PdsBootableSlot0
                                   : flash::Key::PdsBootableSlot1;
    if (flash::Flash::get_data(active_slot_pds, state) != flash::Status::Ok || state == 0) {
        completion_code = Ccode::ErrorInvalidStateForCommand;
        reason_code     = Rcode::NoBootComplete;
        return false;
    }

    // Check if image copy is allowed
    flash::Data allow_bg_copy{};
    flash_status = flash::Flash::get_data(flash::Key::NpdsAllowInitBackgroundCopy,
                                          allow_bg_copy);
    if (flash_status != flash::Status::Ok || allow_bg_copy == 0) {
        // Check bg copy status
        nv::flash::ProgressPercent progress{};
        flash_status = flash::Flash::background_copy_query(progress);
        if (flash_status == flash::Status::BackgroundCopyInprogress) {
            completion_code = Ccode::ErrorInvalidStateForCommand;
            reason_code     = Rcode::ImageCopyInProgress;
            return false;
        }
        else if (flash_status == flash::Status::BackgroundCopyDone) {
            completion_code = Ccode::ErrorInvalidStateForCommand;
            reason_code     = Rcode::ImageCopyCompleted;
            return false;
        }
        else if (flash_status == flash::Status::BackgroundCopyFailed) {
            completion_code = Ccode::ErrorGeneral;
            reason_code     = Rcode::Null;
            // allow bg copy to do again when failure
            return true;
        }
        // Should not happen
        completion_code = Ccode::ErrorInvalidStateForCommand;
        reason_code     = Rcode::ImageCopyInProgress;
        return false;
    }

    // TODO : Chek Flash wear mitigation
    return true;
}

void mctp::Nsm::get_redundancy_policy(NsmRedundancyPolicy& redundancy_policy_persistent,
                                      NsmRedundancyPolicy& redundancy_policy_current)
{
    flash::Data background_copy_policy{};
    flash::Data background_copy_policy_one_time{};
    auto        flash_status = flash::Flash::get_data(flash::Key::PdsBackgroundSetup,
                                               background_copy_policy);
    if (flash_status != flash::Status::Ok) {
        return;
    }

    flash_status = flash::Flash::get_data(flash::Key::PdsBackgroundSetupOneTime,
                                          background_copy_policy_one_time);
    if (flash_status != flash::Status::Ok) {
        return;
    }

    if (background_copy_policy == static_cast<flash::Data>(BackgroundCopyPolicy::Enable)) {
        redundancy_policy_persistent = NsmRedundancyPolicy::Automatic;
        if (background_copy_policy_one_time
            == static_cast<flash::Data>(BackgroundCopyPolicy::OnceDisable)) {
            redundancy_policy_current = NsmRedundancyPolicy::Manual;
        }
        else {
            redundancy_policy_current = NsmRedundancyPolicy::Automatic;
        }
    }
    else {
        redundancy_policy_persistent = NsmRedundancyPolicy::Manual;
        if (background_copy_policy_one_time
            == static_cast<flash::Data>(BackgroundCopyPolicy::OnceEnable)) {
            redundancy_policy_current = NsmRedundancyPolicy::Automatic;
        }
        else {
            redundancy_policy_current = NsmRedundancyPolicy::Manual;
        }
    }
}

Ccode mctp::Nsm::can_revoke_ap_otp()
{
    flash::Data state{};
    if (flash::Flash::get_data(flash::Key::NpdsAp0FwStatus, state) != flash::Status::Ok
        || state != static_cast<flash::Data>(fw_parser::ap::ApFwStatus::Sb_Auth_Success)) {
        return Ccode::ErrorGeneral;
    }
    return Ccode::Success;
}

bool mctp::Nsm::is_inactive_authenticate(uint8_t inactive_slot)
{
    const uint8_t  boot_source          = bootloader::Driver::get_boot_src();
    bool           inactive_auth_result = false;
    const uint32_t inactive_slot0       = 1;
    const uint32_t inactive_slot1       = 2;
    // WAR: Not all platform are boot with FMC in this phase
    if (boot_source == sys::bootloader::Driver::BootSourceFMC
        || boot_source == sys::bootloader::Driver::BootSourceInternalFlash) {
        flash::Data image_auth_result{};
        auto        flash_status = flash::Flash::get_data(flash::Key::NpdsFmcAuthResult,
                                                   image_auth_result);
        if (flash_status != flash::Status::Ok) {
            return false;
        }
        // Bit 0: 0: slot 0 authentication failed, 1: slot 0 authentication passed
        // Bit 1: 0: slot 1 authentication failed, 1: slot 1 authentication passed
        inactive_auth_result = inactive_slot == Slot0Id
                                 ? ((image_auth_result & inactive_slot0) > 0)
                                 : ((image_auth_result & inactive_slot1) > 0);
    }

    // inactive slot fail authenticate
    if (!inactive_auth_result) {
        // Check if bg copy done
        nv::flash::ProgressPercent progress{};
        auto                       flash_status = flash::Flash::background_copy_query(progress);
        if (flash_status != flash::Status::BackgroundCopyDone) {
            return false;
        }
    }
    // From active slot authenticate & bg copy done, assume inactive slot authenticated
    // inactive slot authenticate pass conidition :
    // 1. inactive slot pass authenticate  2. bg copy done
    return true;
}

uint32_t mctp::Nsm::array_to_u32(std::array<uint8_t, 4>& buffer)
{
    uint32_t result = 0;

    memcpy(&result, buffer.data(), sizeof(result));

    return result;
}

void mctp::Nsm::append_number(std::array<char, NvMctpVersionLength>& buf,
                              size_t&                                index,
                              uint32_t                               num,
                              size_t                                 width,
                              bool                                   add_dot)
{
    constexpr size_t   MaxWidth = 4;
    constexpr uint32_t Base     = 10;

    if (width > MaxWidth || index > buf.size() - width) {
        return;
    }

    std::array<char, MaxWidth> temp{};

    for (std::size_t i = width; i > 0; --i) {
        temp.at(i - 1)  = static_cast<char>('0' + (num % Base));
        num            /= Base;
    }

    std::copy(temp.data(), temp.data() + width, buf.begin() + index);
    index += width;

    if (add_dot && index + 1 < buf.size()) {
        buf.at(index++) = '.';
    }
}

void mctp::Nsm::generate_fw_version(std::array<char, NvMctpVersionLength>& buf,
                                    uint16_t                               major,
                                    uint8_t                                minor,
                                    uint16_t                               patch,
                                    uint16_t                               build)
{
    size_t index = 0;

    append_number(buf, index, major, 4, true);
    append_number(buf, index, minor, 2, true);
    append_number(buf, index, patch, 4, true);
    append_number(buf, index, build, 4, false);
}

uint8_t mctp::Nsm::get_active_slot()
{
    const auto ActiveSlot = bootloader::Driver::current_boot_index();
    if (ActiveSlot == bootloader::Driver::ImageIndex::Image0) {
        return Slot0Id;
    }
    else if (ActiveSlot == bootloader::Driver::ImageIndex::Image1) {
        return Slot1Id;
    }
    return InvalidSlot;
}

uint8_t mctp::Nsm::get_inactive_slot()
{
    const auto ActiveSlot = bootloader::Driver::current_boot_index();
    if (ActiveSlot == bootloader::Driver::ImageIndex::Image0) {
        return Slot1Id;
    }
    else if (ActiveSlot == bootloader::Driver::ImageIndex::Image1) {
        return Slot0Id;
    }
    return InvalidSlot;
}

uint8_t mctp::Nsm::get_inactive_fw_state()
{
    const bool is_inactive_authenticated = is_inactive_authenticate(get_inactive_slot());

    auto inactive_default_status = common::Inactive;
    if (!is_inactive_authenticated) {
        inactive_default_status = common::FailedAuthentication;
    }
    else if (nv::pldm::Task::is_background_copy_automatic(false)) {
        inactive_default_status = common::PendingImageCopy;
    }
    else {
        inactive_default_status = common::Inactive;
    }

    flash::Data update_state{};
    auto        flash_status = flash::Flash::get_data(flash::Key::PdsUpdateState, update_state);
    if (flash_status != flash::Status::Ok) {
        update_state = static_cast<flash::Data>(bootloader::Driver::State::Invalid);
    }

    switch (update_state) {
        case static_cast<flash::Data>(bootloader::Driver::State::InProgress):
            return common::WriteInProgress;
        case static_cast<flash::Data>(bootloader::Driver::State::Complete): {
            // If current slot is the update slot, we've switched to it, so check background
            // copy status
            flash::Data update_slot{};
            flash_status = flash::Flash::get_data(flash::Key::PdsUpdateSlot, update_slot);
            const auto current_slot = bootloader::Driver::current_boot_index();

            if (flash_status == flash::Status::Ok
                && static_cast<uint8_t>(current_slot) == update_slot) {
                nv::flash::ProgressPercent progress{};
                auto copy_status = flash::Flash::background_copy_query(progress);
                switch (copy_status) {
                    case flash::Status::BackgroundCopyIdle: return inactive_default_status;
                    case flash::Status::BackgroundCopyInprogress:
                        return common::ImageCopyInProgress;
                    case flash::Status::BackgroundCopyDone  : return common::Inactive;
                    case flash::Status::BackgroundCopyFailed: return common::FailedImageCopy;
                    default                                 : return inactive_default_status;
                }
            }
            // Update was to inactive slot, not yet activated
            return common::PendingActivation;
        }
        case static_cast<flash::Data>(bootloader::Driver::State::Idle): {
            nv::flash::ProgressPercent progress{};
            auto copy_status = flash::Flash::background_copy_query(progress);
            switch (copy_status) {
                case flash::Status::BackgroundCopyIdle: return inactive_default_status;
                case flash::Status::BackgroundCopyInprogress:
                    return common::ImageCopyInProgress;
                case flash::Status::BackgroundCopyDone  : return common::Inactive;
                case flash::Status::BackgroundCopyFailed: return common::FailedImageCopy;
                default                                 : return inactive_default_status;
            }
        }
        case static_cast<flash::Data>(bootloader::Driver::State::Stage): return common::Staged;
        default                                                        : return inactive_default_status;
    }
}

uint8_t mctp::Nsm::get_ap_state()
{
    flash::Data update_state{};
    auto flash_status = flash::Flash::get_data(flash::Key::NpdsAp0FwStatus, update_state);
    if (flash_status != flash::Status::Ok) {
        return common::Unknown;
    }

    switch (update_state) {
        case static_cast<flash::Data>(fw_parser::ap::ApFwStatus::Update_In_Progress):
            return common::WriteInProgress;
        case static_cast<flash::Data>(fw_parser::ap::ApFwStatus::Update_Complete):
            return common::PendingActivation;
        case static_cast<flash::Data>(fw_parser::ap::ApFwStatus::Sb_Auth_Success):
            return common::Activated;
        default: return common::FailedAuthentication;
    }
}

bool mctp::Nsm::is_active_slot(uint8_t slot)
{
    return slot == get_active_slot();
}

// Find the greatest value of (1 << N) - 1 but <= unary_value.
// return (N - 1)
uint32_t mctp::Nsm::convert_unary_to_key_index(uint32_t unary_value)
{
    uint32_t key_index = 0;

    if (unary_value == UINT32_MAX) {
        return KeyIndexMax;
    }

    while (key_index < KeyIndexMax && (1U << key_index) - 1 <= unary_value) {
        ++key_index;
    }

    return key_index ? key_index - 1 : 0;
}

// Check if n is in the form of 000...0111...1
// Allow no 1's in the end
bool mctp::Nsm::is_form_zeros_then_ones(uint32_t num)
{
    if (num == UINT32_MAX) {
        return true;
    }
    return ((num & (num + 1)) == 0);
}

Ccode mctp::Nsm::convert_key_index_to_key_permission(uint16_t  key_index,
                                                     uint32_t& key_permission)
{
    // return key_permission in the form of (1 << key_index) - 1
    if (key_index > KeyIndexMax) {
        key_permission = 0;
        return Ccode::ErrorGeneral;
    }
    else if (key_index == KeyIndexMax) {
        key_permission = UINT32_MAX;
        return Ccode::Success;
    }
    else {
        key_permission = (1U << key_index) - 1;
        return Ccode::Success;
    }
}

Ccode mctp::Nsm::fill_key_index(uint8_t slot, uint16_t& key_index)
{
    const fw_parser::mcu::ParsingFwType parsing_type = (slot == Slot0Id)
                                                         ? fw_parser::mcu::ParsingFwType::Slot0
                                                         : fw_parser::mcu::ParsingFwType::Slot1;

    auto image_key_version = fw_parser::mcu::get_image_signing_key_version(parsing_type);

    if (image_key_version.has_value()) {
        auto converted_image_key_version = convert_unary_to_key_index(*image_key_version);
        // Will not happen, fix for coverity
        if (converted_image_key_version > KeyIndexMax) {
            return Ccode::ErrorGeneral;
        }
        key_index = static_cast<uint16_t>(converted_image_key_version);
    }
    else {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_ap_key_index(uint16_t& key_index)
{
    const fw_parser::ap::ParsingApFwType
         parsing_ap_fw_type = fw_parser::ap::ParsingApFwType::UpdateSlot;
    auto image_key_index    = fw_parser::ap::get_ap_signing_key_index(parsing_ap_fw_type);

    if (image_key_index.has_value()) {
        if (std::to_underlying(*image_key_index) > KeyIndexMax) {
            return Ccode::ErrorGeneral;
        }

        if (std::to_underlying(*image_key_index) > std::numeric_limits<uint16_t>::max()) {
            return Ccode::ErrorGeneral;
        }
        else {
            // coverity[cert_int31_c_violation] - Already checked above
            key_index = static_cast<uint16_t>(std::to_underlying(*image_key_index));
        }
    }
    else {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_fmc_key_index(uint16_t& key_index)
{
    const fw_parser::mcu::ParsingFwType parsing_type = fw_parser::mcu::ParsingFwType::Fmc;

    auto image_key_version = fw_parser::mcu::get_image_signing_key_version(parsing_type);

    if (image_key_version.has_value()) {
        auto converted_image_key_version = convert_unary_to_key_index(*image_key_version);
        if (converted_image_key_version > KeyIndexMax) {
            return Ccode::ErrorGeneral;
        }
        key_index = static_cast<uint16_t>(converted_image_key_version);
    }
    else {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_key_permission(uint8_t slot, uint32_t& key_permission)
{
    uint16_t key_index = 0;
    auto     status    = fill_key_index(slot, key_index);
    if (status != Ccode::Success) {
        key_permission = 0;
        return status;
    }
    status = convert_key_index_to_key_permission(key_index, key_permission);
    if (status != Ccode::Success) {
        key_permission = 0;
        return status;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_ap_key_permission(uint32_t& key_permission)
{
    uint16_t key_index = 0;
    auto     status    = fill_ap_key_index(key_index);
    if (status != Ccode::Success) {
        key_permission = 0;
        return status;
    }
    status = convert_key_index_to_key_permission(key_index, key_permission);
    if (status != Ccode::Success) {
        key_permission = 0;
        return status;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_fmc_key_permission(uint32_t& key_permission)
{
    uint16_t key_index = 0;
    auto     status    = fill_fmc_key_index(key_index);
    if (status != Ccode::Success) {
        key_permission = 0;
        return status;
    }
    status = convert_key_index_to_key_permission(key_index, key_permission);
    if (status != Ccode::Success) {
        key_permission = 0;
        return status;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_fuse_key_permission(uint32_t& key_permission)
{
    // Read image key revoke from CFPA
    uint32_t key_index_unary = 0;
    auto     flash_status    = nv::flash::Flash::read_key_revoke(key_index_unary,
                                                          nv::flash::KeyRollbackSelect::Mcu);
    if (flash_status != flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }

    auto key_index = convert_unary_to_key_index(key_index_unary);
    auto status    = convert_key_index_to_key_permission(key_index, key_permission);
    if (status != Ccode::Success) {
        key_permission = 0;
        return status;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_ap_fuse_key_permission(uint32_t& key_permission)
{
    // Read image key revoke from CFPA
    uint32_t key_index_unary = 0;
    auto     flash_status    = nv::flash::Flash::read_key_revoke(key_index_unary,
                                                          nv::flash::KeyRollbackSelect::Ap0);
    if (flash_status != flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }

    auto key_index = convert_unary_to_key_index(key_index_unary);
    auto status    = convert_key_index_to_key_permission(key_index, key_permission);
    if (status != Ccode::Success) {
        key_permission = 0;
        return status;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::revoke_key_permission(uint32_t key_permission)
{
    auto status = nv::flash::Flash::write_key_revoke(
        key_permission, nv::flash::KeyRollbackSelect::Mcu, 1s);
    if (status != flash::Status::Ok) {
        return Ccode::ErrorEfuseUpdateFailed;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::revoke_ap_key_permission(uint32_t key_permission)
{
    auto key_index = convert_unary_to_key_index(key_permission);
    auto status    = nv::flash::Flash::write_key_revoke(
        key_index, nv::flash::KeyRollbackSelect::Ap0, 1s);
    if (status != flash::Status::Ok) {
        return Ccode::ErrorEfuseUpdateFailed;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_sec_ver_num(uint8_t slot, uint32_t& rollback_protection)
{
    const fw_parser::mcu::ParsingFwType parsing_type = (slot == Slot0Id)
                                                         ? fw_parser::mcu::ParsingFwType::Slot0
                                                         : fw_parser::mcu::ParsingFwType::Slot1;
    // Read secure fw version from firmware
    auto security_version = fw_parser::mcu::get_security_version(parsing_type);

    if (security_version.has_value()) {
        rollback_protection = (*security_version);
    }
    else {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_ap_sec_ver_num(uint32_t& rollback_protection)
{
    const fw_parser::ap::ParsingApFwType
        parsing_ap_fw_type = fw_parser::ap::ParsingApFwType::UpdateSlot;
    // Read ap firmware secure version
    auto security_version = fw_parser::ap::get_ap_sec_version(parsing_ap_fw_type);

    if (security_version.has_value()) {
        rollback_protection = static_cast<uint32_t>(*security_version);
    }
    else {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_fmc_sec_ver_num(uint32_t& rollback_protection)
{
    const fw_parser::mcu::ParsingFwType parsing_type = fw_parser::mcu::ParsingFwType::Fmc;
    // Read secure fw version from firmware
    auto security_version = fw_parser::mcu::get_security_version(parsing_type);

    if (security_version.has_value()) {
        rollback_protection = (*security_version);
    }
    else {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_fuse_mini_sec_ver_num(uint32_t& rollback_protection)
{
    // Read secure fw version from CFPA
    auto status = nv::flash::Flash::read_secure_fw_version(rollback_protection,
                                                           nv::flash::KeyRollbackSelect::Mcu);
    if (status != flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_ap_fuse_mini_sec_ver_num(uint32_t& rollback_protection)
{
    // Read secure fw version from CFPA
    auto status = nv::flash::Flash::read_secure_fw_version(rollback_protection,
                                                           nv::flash::KeyRollbackSelect::Ap0);
    if (status != flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::revoke_rollback_protection(uint32_t rollback_protection)
{
    auto status = nv::flash::Flash::write_secure_fw_version(
        rollback_protection, nv::flash::KeyRollbackSelect::Mcu, 1s);
    if (status != flash::Status::Ok) {
        return Ccode::ErrorEfuseUpdateFailed;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::revoke_ap_rollback_protection(uint32_t rollback_protection)
{
    auto status = nv::flash::Flash::write_secure_fw_version(
        rollback_protection, nv::flash::KeyRollbackSelect::Ap0, 1s);
    if (status != flash::Status::Ok) {
        return Ccode::ErrorEfuseUpdateFailed;
    }

    return Ccode::Success;
}

uint8_t mctp::Nsm::fill_signing_type(uint8_t index)
{
    if (index == 0) {
        return SigningTypeDebug;
    }
    else {
        return SigningTypeProd;
    }
}

bool mctp::Nsm::fill_build_type(uint8_t& build_type)
{
    const uint8_t BuildType = NV_BUILD_MODE;
    if (BuildType == MakefileBuildTypeDev) {
        build_type = NsmBuildTypeDev;
        return true;
    }
    else if (BuildType == MakefileBuildTypeRel) {
        build_type = NsmBuildTypeRel;
        return true;
    }
    else {
        build_type = NsmBuildTypeUndefined;
        return false;
    }
}

void mctp::Nsm::fill_boot_status_code(std::array<uint8_t, 8>& input)
{
    uint64_t status = 0;
    uint8_t  data   = 0;

    // Bit 5 - EC_RECEIVE_AP0_BOOT_COMPLETE
    auto& event  = ipc::Event::make(ipc::EventId::TaskBootStatus);
    auto  value  = event.bits().value();
    data         = (value == common::to_underlying(ipc::BootedEventBits::BootStatusMask));
    status      |= ((data & 1U) << BootStatusEcReceiveAp0BootComplete);

    // Bit 20 - AP0_ACTIVE_SLOT
    data    = get_active_slot();
    status |= ((data & 1U) << BootStatusApFwBootSlot);

    for (int i = 7; i >= 0; --i) {
        input.at(7 - i) = static_cast<uint8_t>((status >> uint8_t(i * 8)) & UINT8_MAX);
    }
}

bool mctp::Nsm::is_event_source_enable(NsmMsgType nv_msg_type, uint8_t event_id)
{
    if (nv_msg_type == NsmMsgType::DeviceCapabilityDiscovery) {
        const size_t ByteIndex           = event_id / 8;
        const size_t BitOffset           = event_id % 8;
        const bool   event_source_enable = (type0_event_enable_bitmask.at(ByteIndex)
                                          & (1U << BitOffset))
                                      != 0;

        // event_source not available
        if (event_source_enable == false) {
            // log only 1st when event_source not available
            if (log_nvmsg_event_bitmask.at(static_cast<uint8_t>(nv_msg_type)) == false) {
                logger::info(logger::Event::MctpNsmEventNotEnable,
                             {static_cast<uint8_t>(nv_msg_type), event_id});
                log_nvmsg_event_bitmask.at(static_cast<uint8_t>(nv_msg_type)) = true;
            }
        }
        return event_source_enable;
    }
    else if (nv_msg_type == NsmMsgType::Firmware) {
        const size_t ByteIndex           = event_id / 8;
        const size_t BitOffset           = event_id % 8;
        const bool   event_source_enable = (type6_event_enable_bitmask.at(ByteIndex)
                                          & (1U << BitOffset))
                                      != 0;
        // event_source not available
        if (event_source_enable == false) {
            // log only 1st when event_source not available
            if (log_nvmsg_event_bitmask.at(static_cast<uint8_t>(nv_msg_type)) == false) {
                logger::info(logger::Event::MctpNsmEventNotEnable,
                             {static_cast<uint8_t>(nv_msg_type), event_id});
                log_nvmsg_event_bitmask.at(static_cast<uint8_t>(nv_msg_type)) = true;
            }
        }
        return event_source_enable;
    }
    else {
        return false;
    }
}

bool mctp::Nsm::is_global_event_setting_push()
{
    if (event_subscription.setting == NsmGlobalEventSetting::EventPush) {
        return true;
    }
    else {
        // log only 1st when event_subscription.setting not PUSH
        if (log_event_subscription == false) {
            logger::info(logger::Event::MctpNsmEventSettingNotMatch,
                         {static_cast<uint8_t>(event_subscription.setting),
                          static_cast<uint8_t>(NsmGlobalEventSetting::EventPush)});
            log_event_subscription = true;
        }
        return false;
    }
}

bool mctp::Nsm::is_fw_comp_id_valid(const FwCompInfo& input_fw_comp_info, uint16_t component_id)
{
    if (input_fw_comp_info.component_class != NvMctpFwComponentClass
        || input_fw_comp_info.component_id != component_id
        || input_fw_comp_info.component_index != 0x00) {
        return false;
    }
    return true;
}

bool mctp::Nsm::is_nonce_match(const Nonce& input_nonce)
{
    return std::memcmp(&input_nonce, &_nonce, NvMctpNsmNonceSize) == 0;
}

bool mctp::Nsm::is_input_length_valid(const Packet& rx, uint8_t size)
{
    uint16_t input_data_size = 0;

    if (rx.priv.packet_length >= sizeof(Header) + HeaderRequestSize) {
        input_data_size = rx.priv.packet_length - sizeof(Header) - HeaderRequestSize;
    }
    else {
        input_data_size = 0;
    }

    if (input_data_size < size) {
        return false;
    }

    return true;
}

void mctp::Nsm::on_dcd_ping(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = 0;

#if TEST_NSM_EVENT
    Driver::mctp_send_cmd(Driver::CmdCode::NsmEventCmd,
                          static_cast<uint8_t>(NsmMsgType::DeviceCapabilityDiscovery),
                          static_cast<uint8_t>(NsmDcdEvent::GpioEvent));
#endif
}

void mctp::Nsm::on_dcd_get_sup_nv_meg_type(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + nsm_msg::NvMctpSupportedNum;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = nsm_msg::NvMctpSupportedNum;

    memcpy(&ntx.data, SupNvMegType.data(), SupNvMegType.size());
}

void mctp::Nsm::on_dcd_get_sup_cmd_codes(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto&         nrx           = NsmPktReq::from(rx);
    auto&         ntx           = NsmPktResp::from(tx);
    const uint8_t NvMessageType = nrx.data[0];
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + nsm_msg::NvMctpSupportedNum;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = nsm_msg::NvMctpSupportedNum;

    if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::DeviceCapabilityDiscovery)) {
        memcpy(&ntx.data, SupType0Code.data(), SupType0Code.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::PciLinks)) {
        memcpy(&ntx.data, SupType2Code.data(), SupType2Code.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::Diagnostics)) {
        memcpy(&ntx.data, SupType4Code.data(), SupType4Code.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::Firmware)) {
        memcpy(&ntx.data, SupType6Code.data(), SupType6Code.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::PlatformEnviromentals)) {
        memcpy(&ntx.data, SupType3Code.data(), SupType3Code.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::DeviceConfiguration)) {
        memcpy(&ntx.data, SupType5Code.data(), SupType5Code.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::NvInternal)) {
        memcpy(&ntx.data, SupTypeFFCode.data(), SupTypeFFCode.size());
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
}

void mctp::Nsm::on_dcd_get_sup_event_srcs(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto&         nrx           = NsmPktReq::from(rx);
    auto&         ntx           = NsmPktResp::from(tx);
    const uint8_t NvMessageType = nrx.data[0];
    tx.priv.packet_length       = sizeof(Header) + HeaderResponseSize
                          + nsm_msg::NvMctpEventSupportedNum;
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = nsm_msg::NvMctpEventSupportedNum;

    if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::DeviceCapabilityDiscovery)) {
        memcpy(&ntx.data, SupType0Event.data(), SupType0Event.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::Firmware)) {
        memcpy(&ntx.data, SupType6Event.data(), SupType6Event.size());
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
}

void mctp::Nsm::on_dcd_get_current_event_srcs(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto&         nrx           = NsmPktReq::from(rx);
    auto&         ntx           = NsmPktResp::from(tx);
    const uint8_t NvMessageType = nrx.data[0];
    tx.priv.packet_length       = sizeof(Header) + HeaderResponseSize
                          + nsm_msg::NvMctpEventSupportedNum;
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = nsm_msg::NvMctpEventSupportedNum;

    // Copy event bitmask to the output
    if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::DeviceCapabilityDiscovery)) {
        memcpy(&ntx.data, type0_event_enable_bitmask.data(), type0_event_enable_bitmask.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::Firmware)) {
        memcpy(&ntx.data, type6_event_enable_bitmask.data(), type6_event_enable_bitmask.size());
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
}

void mctp::Nsm::on_dcd_set_current_event_srcs(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 9;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    NvMsgTypeWithEventBitmask msg_with_bitmask{};
    memcpy(&msg_with_bitmask, &nrx.data, RequestSize);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = 0;

    if (msg_with_bitmask.nv_msg_type == NsmMsgType::DeviceCapabilityDiscovery) {
        type0_event_enable_bitmask = msg_with_bitmask.bitmask;
        // & operation to avoid set unsupported event
        for (size_t i = 0; i < nsm_msg::NvMctpEventSupportedNum; ++i) {
            type0_event_enable_bitmask.at(i) &= SupType0Event.at(i);
        }
        // Allow log when event_srcs not available
        log_nvmsg_event_bitmask.at(static_cast<uint8_t>(msg_with_bitmask.nv_msg_type)) = false;
    }
    else if (msg_with_bitmask.nv_msg_type == NsmMsgType::Firmware) {
        type6_event_enable_bitmask = msg_with_bitmask.bitmask;
        // & operation to avoid set unsupported event
        for (size_t i = 0; i < nsm_msg::NvMctpEventSupportedNum; ++i) {
            type6_event_enable_bitmask.at(i) &= SupType6Event.at(i);
        }
        // Allow log when event_srcs not available
        log_nvmsg_event_bitmask.at(static_cast<uint8_t>(msg_with_bitmask.nv_msg_type)) = false;
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
}

void mctp::Nsm::on_dcd_set_event_subscription(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 2;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto&             nrx = NsmPktReq::from(rx);
    auto&             ntx = NsmPktResp::from(tx);
    EventSubscription input_event_subscription{};
    memcpy(&input_event_subscription, &nrx.data, RequestSize);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = 0;

    if (input_event_subscription.setting == NsmGlobalEventSetting::EventDisable
        || input_event_subscription.setting == NsmGlobalEventSetting::EventPolling
        || input_event_subscription.setting == NsmGlobalEventSetting::EventPush) {
        event_subscription.setting     = input_event_subscription.setting;
        event_subscription.endpoint_id = input_event_subscription.endpoint_id;
        nsm_event_clients              = rx;
        if (event_subscription.setting != NsmGlobalEventSetting::EventPush) {
            log_event_subscription = false;
        }
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
}

void mctp::Nsm::on_dcd_get_event_subscription(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RespSize = 1;

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;

    if (event_subscription.setting == NsmGlobalEventSetting::EventDisable) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
    }
    else {
        ntx.data[0] = event_subscription.endpoint_id;
    }
}

bool mctp::Nsm::is_event_ack_enable(NsmMsgType nv_msg_type, uint8_t event_id)
{
    if (nv_msg_type == NsmMsgType::Firmware) {
        const size_t ByteIndex = event_id / 8;
        const size_t BitOffset = event_id % 8;
        // Test if the specific bit is set
        return (type6_event_ack_bitmask.at(ByteIndex) & (1u << BitOffset)) != 0;
    }
    else if (nv_msg_type == NsmMsgType::DeviceCapabilityDiscovery) {
        const size_t ByteIndex = event_id / 8;
        const size_t BitOffset = event_id % 8;
        // Test if the specific bit is set
        return (type0_event_ack_bitmask.at(ByteIndex) & (1u << BitOffset)) != 0;
    }
    else {
        return false;
    }
}

void mctp::Nsm::on_dcd_configure_event_ack(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 9;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    NvMsgTypeWithEventBitmask msg_with_bitmask{};
    memcpy(&msg_with_bitmask, &nrx.data, RequestSize);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = 0;

    nv::info("configure_event_ack: MsgType %d, 1st bytebitmap 0x%x\n",
             static_cast<uint8_t>(msg_with_bitmask.nv_msg_type),
             msg_with_bitmask.bitmask[0]);

    if (msg_with_bitmask.nv_msg_type == NsmMsgType::Firmware) {
        type6_event_ack_bitmask = msg_with_bitmask.bitmask;
        // & operation to avoid set unsupported event
        for (size_t i = 0; i < nsm_msg::NvMctpEventSupportedNum; ++i) {
            type6_event_ack_bitmask.at(i) &= SupType6Event.at(i);
        }
    }
    else if (msg_with_bitmask.nv_msg_type == NsmMsgType::DeviceCapabilityDiscovery) {
        type0_event_ack_bitmask = msg_with_bitmask.bitmask;
        // & operation to avoid set unsupported event
        for (size_t i = 0; i < nsm_msg::NvMctpEventSupportedNum; ++i) {
            type0_event_ack_bitmask.at(i) &= SupType0Event.at(i);
        }
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
}

void mctp::Nsm::on_dcd_query_device_id(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RespSize = 2;
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;
    // 0xFF for "unknown" instance ID
    std::array<uint8_t, RespSize> initial_data = {NvMctpDeviceMcuId, UINT8_MAX};
    memcpy(&ntx.data, initial_data.data(), initial_data.size());
}

void mctp::Nsm::on_dcd_get_device_capabilities_v2(const Packet& rx, Packet& tx)
{
    if (!is_input_length_valid(rx, 0)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx           = NsmPktBulkResp::from(tx);
    ntx.completion_code = Ccode::Success;

    // Use TelemetryRecordArray to build response
    // NOLINTNEXTLINE(misc-const-correctness)
    TelemetryRecordArray capabilities_array(
        ntx.data,  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
        TelemetryRecordArray::MaxNsmBulkResponseSize);

    // TAG 0: Timestamp Generation
    if (!capabilities_array.addRecordNvU8(TagTimestampGeneration,
                                          static_cast<uint8_t>(TimestampNotSupported))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // TAG 1: Maximum Input Buffer Size
    if (!capabilities_array.addRecordNvU32(TagMaxInputBufferSize,
                                           static_cast<uint32_t>(MaxInputBufferSize))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // Set final response size
    const uint16_t size_response = capabilities_array.arraySize();
    ntx.telemetry_count          = capabilities_array.elements();

    // Calculate packet length with overflow protection
    const uint32_t calculated_length = sizeof(Header) + AggregateHeaderResponseSize
                                     + size_response;
    if (calculated_length > UINT16_MAX) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    tx.priv.packet_length = static_cast<uint16_t>(calculated_length);
}

bool mctp::Nsm::process_device_capability_discovery(const Packet& rx, Packet& tx)
{
    using cmd = mctp::NsmDcdCmdCode;
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    ntx.set_dcd_code(nrx.get_dcd_code());

    // Helper to handle unsupported commands
    [[maybe_unused]] auto unsupported_command = [&]() {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return false;
    };

    switch (nrx.get_dcd_code()) {
        case cmd::DcdPing:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery, cmd::DcdPing)) {
                on_dcd_ping(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdGetSupNvMsgTypes:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdGetSupNvMsgTypes)) {
                on_dcd_get_sup_nv_meg_type(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdGetSupCmdCodes:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdGetSupCmdCodes)) {
                on_dcd_get_sup_cmd_codes(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdGetSupEventSrcs:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdGetSupEventSrcs)) {
                on_dcd_get_sup_event_srcs(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdGetCurrentEventSrcs:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdGetCurrentEventSrcs)) {
                on_dcd_get_current_event_srcs(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdSetCurrentEventSrcs:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdSetCurrentEventSrcs)) {
                on_dcd_set_current_event_srcs(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdSetEventSubscription:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdSetEventSubscription)) {
                on_dcd_set_event_subscription(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdGetEventSubscription:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdGetEventSubscription)) {
                on_dcd_get_event_subscription(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdQueryDeviceIdentification:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdQueryDeviceIdentification)) {
                on_dcd_query_device_id(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdGetDeviceCapabilitiesV2:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery,
                                     cmd::DcdGetDeviceCapabilitiesV2)) {
                on_dcd_get_device_capabilities_v2(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdGetGpio:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery, cmd::DcdGetGpio)) {
                on_dcd_get_gpio(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::DcdSetGpio:
            if constexpr (is_cmd_set(NsmMsgType::DeviceCapabilityDiscovery, cmd::DcdSetGpio)) {
                on_dcd_set_gpio(rx, tx);
                break;
            }
            return unsupported_command();

        default: return unsupported_command();
    }
    return true;
}

bool mctp::Nsm::process_firmware(const Packet& rx, Packet& tx)
{
    using cmd = mctp::NsmFWCmdCode;
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    ntx.nv_msg_type = nrx.nv_msg_type;
    ntx.set_fw_code(nrx.get_fw_code());

    // Helper to handle unsupported commands
    [[maybe_unused]] auto unsupported_command = [&]() {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return false;
    };

    switch (nrx.get_fw_code()) {
        case cmd::GetRotStateInfo:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::GetRotStateInfo)) {
                on_get_rot_state_info(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::IrreversibleConf:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::IrreversibleConf)) {
                on_ctrl_irreversible_conf(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::QueryCodeAuthKey:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::QueryCodeAuthKey)) {
                on_query_auth_key(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::UpdateCodeAuthKey:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::UpdateCodeAuthKey)) {
                on_update_auth_key(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::QuerySecVerNum:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::QuerySecVerNum)) {
                on_query_sec_ver_num(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::UpdateMinSecVerNum:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::UpdateMinSecVerNum)) {
                on_update_min_sec_ver_num(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::QueryFwCompId:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::QueryFwCompId)) {
                on_query_fw_comp_id(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetRotProperty:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::SetRotProperty)) {
                on_set_rot_property(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::ImageCopyControl:
            if constexpr (is_cmd_set(NsmMsgType::Firmware, cmd::ImageCopyControl)) {
                on_image_copy_control(rx, tx);
                break;
            }
            return unsupported_command();

        default: return unsupported_command();
    }
    return true;
}

bool mctp::Nsm::process(const Packet& rx, Packet& tx)
{
    using cmd       = mctp::NsmMsgType;
    auto& nrx       = NsmPktReq::from(rx);
    auto  vendor_id = nrx.pci_vendor_id;

    if (vendor_id != NvMctpPciVendorId) {  // ensure this is an NVIDIA NSM
        // TODO - current version doesn't support reason code
        // Could use reason code:  ERR_INVALID_PCI
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return false;
    }

    // Helper to handle unsupported message types
    [[maybe_unused]] auto unsupported_msg_type = [&]() {
        auto& ntx       = NsmPktResp::from(tx);
        ntx.nv_msg_type = nrx.nv_msg_type;
        ntx.set_dcd_code(nrx.get_dcd_code());
        fill_error_packet(Ccode::ErrorUnsupportedMsgType, rx, tx);
        return false;
    };

    switch (nrx.nv_msg_type) {
        case cmd::DeviceCapabilityDiscovery:
            if constexpr (is_msg_set(cmd::DeviceCapabilityDiscovery)) {
                process_device_capability_discovery(rx, tx);
                break;
            }
            return unsupported_msg_type();

        case cmd::PciLinks:
            if constexpr (is_msg_set(cmd::PciLinks)) {
                process_pci_links(rx, tx);
                break;
            }
            return unsupported_msg_type();

        case cmd::Firmware:
            if constexpr (is_msg_set(cmd::Firmware)) {
                process_firmware(rx, tx);
                break;
            }
            return unsupported_msg_type();

        case cmd::Diagnostics:
            // here checking if is NOT supported
            if constexpr (false == is_msg_set(cmd::Diagnostics)) {
                return unsupported_msg_type();
            }
            if constexpr (nv::ipc::DebugTokenEnabled) {
                if (process_debugtoken_diagnostics(rx, tx)) {
                    break;
                }
            }
            process_diagnostics(rx, tx);
            break;

        case cmd::DeviceConfiguration:
            if constexpr (is_msg_set(cmd::DeviceConfiguration)) {
                process_device_configuration(rx, tx);
                break;
            }
            return unsupported_msg_type();

        case cmd::PlatformEnviromentals:
            if constexpr (is_msg_set(cmd::PlatformEnviromentals)) {
                process_platform_enviromentals(rx, tx);
                break;
            }
            return unsupported_msg_type();

        case cmd::NvInternal:
            if constexpr (is_msg_set(cmd::NvInternal)) {
                process_nv_internal(rx, tx);
                break;
            }
            return unsupported_msg_type();

        default: return unsupported_msg_type();
    }
    return true;
}

void mctp::Nsm::get_rot_state_info(const Packet& rx, Packet& tx)
{
    uint8_t                build_type                   = 0;
    uint8_t                active_slot                  = 0;
    uint16_t               major                        = 0;
    uint16_t               patch                        = 0;
    uint16_t               build                        = 0;
    uint8_t                minor                        = 0;
    uint16_t               key_index_slot0              = 0;
    uint16_t               key_index_slot1              = 0;
    uint32_t               svn_data                     = 0;
    uint32_t               min_svn_data                 = 0;
    const uint32_t         ap_sku_id                    = MCU_AP_SKU;
    std::array<uint8_t, 8> boot_status_code             = {0};
    Ccode                  key_data_valid_slot0         = Ccode::Success;
    Ccode                  key_data_valid_slot1         = Ccode::Success;
    uint8_t                wp_state                     = 0;
    NsmRedundancyPolicy    redundancy_policy_persistent = NsmRedundancyPolicy::NotApplicable;
    NsmRedundancyPolicy    redundancy_policy_current    = NsmRedundancyPolicy::NotApplicable;
    std::array<char, NvMctpVersionLength> fw_version{};
    fill_packet_header_aggr(rx, tx);
    fill_nsm_msg_header_aggr(rx, tx);
    auto& ntx             = NsmPktRespAggr::from(tx);
    tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize
                          + sizeof(RespAggregate);
    ntx.completion_code = Ccode::Success;
    ntx.telemetry_count = TelemetryCount;

    // TAG   LENG FIELD
    // TAG 1,  0, RedundancyPolicyPersistent
    // TAG 2,  0, ActiveFirmwareSlot
    // TAG 3,  0, ActiveKeySet
    // TAG 4,  0, WriteProtectState
    // TAG 5,  0, FirmwareSlotCount
    // TAG 6,  0, FirmwareSlotID
    // TAG 7,  5, FirmwareVersionString
    // TAG 8,  2, VersionComparisonStamp
    // TAG 9,  0, BuildType
    // TAG 10, 0, SigningType
    // TAG 11, 0, FirmwareState
    // TAG 12, 1, SecurityVersionNumber
    // TAG 13, 1, MinimumSecurityVersionNumber
    // TAG 14, 1, SigningKeyIndex
    // TAG 15, 0, InbandUpdatePolicyPersistent
    // TAG 16, 3, BootStatusCode
    // TAG 17, 0, InbandUpdatePolicyCurrent
    // TAG 18, 0, RedundancyPolicyCurrent
    // TAG 19, 2, ApSkuId
    // TAG 20, 0, GlobalFailoverPolicy

    // Tag 1, 2, 3, 13, 15, 16, 17, 18, 19, 20
    // Tag 5 (slot count : n) (The following tag repeat n times)
    // slot 0 : Tag 6, 7, 8, 9, 10, 4, 11, 12, 14
    // slot 1 : Tag 6, 7, 8, 9, 10, 4, 11, 12, 14

    key_data_valid_slot0 = fill_key_index(Slot0Id, key_index_slot0);
    key_data_valid_slot1 = fill_key_index(Slot1Id, key_index_slot1);
    active_slot          = get_active_slot();
    get_redundancy_policy(redundancy_policy_persistent, redundancy_policy_current);
    auto gpio_status = nv::gpio::Status::Error;
    if (nv::ipc::GlobalWpPort != nv::gpio::InvalidGpioPort) {
        gpio_status = nv::gpio::Driver::read(
            nv::ipc::GlobalWpPort, nv::ipc::GlobalWpPin, wp_state);
    }
    // TAG 1,  0, RedundancyPolicyPersistent
    ntx.resp_aggregate.tag_redundancy_policy_persistent.tag = TagRedundancyPolicyPersistent;
    ntx.resp_aggregate.tag_redundancy_policy_persistent.v   = 1;
    ntx.resp_aggregate.tag_redundancy_policy_persistent
        .length = RotTagLength::TagRedundancyPolicyPersistentLen;
    ntx.resp_aggregate.tag_redundancy_policy_persistent.rsvd    = 0;
    ntx.resp_aggregate.tag_redundancy_policy_persistent.b       = 0;
    ntx.resp_aggregate.tag_redundancy_policy_persistent.data[0] = static_cast<uint8_t>(
        redundancy_policy_persistent);

    // TAG 2,  0, ActiveSlot
    ntx.resp_aggregate.tag_active_firmware_slot.tag    = TagActiveFirmwareSlot;
    ntx.resp_aggregate.tag_active_firmware_slot.v      = (active_slot != InvalidSlot) ? 1 : 0;
    ntx.resp_aggregate.tag_active_firmware_slot.length = RotTagLength::TagActiveFirmwareSlotLen;
    ntx.resp_aggregate.tag_active_firmware_slot.rsvd   = 0;
    ntx.resp_aggregate.tag_active_firmware_slot.b      = 0;
    ntx.resp_aggregate.tag_active_firmware_slot.data[0] = active_slot;

    // TAG 3,  0, ActiveKeySet
    ntx.resp_aggregate.tag_active_key_set.tag    = TagActiveKeySet;
    ntx.resp_aggregate.tag_active_key_set.v      = 1;
    ntx.resp_aggregate.tag_active_key_set.length = RotTagLength::TagActiveKeySetLen;
    ntx.resp_aggregate.tag_active_key_set.rsvd   = 0;
    ntx.resp_aggregate.tag_active_key_set.b      = 0;

    const uint8_t KeySet                          = 0;
    ntx.resp_aggregate.tag_active_key_set.data[0] = KeySet;

    // TAG 13, 1, MinimumSecurityVersionNumber
    ntx.resp_aggregate.tag_min_security_ver_num.tag = TagMinSecurityVerNum;
    ntx.resp_aggregate.tag_min_security_ver_num.v   = (fill_fuse_mini_sec_ver_num(min_svn_data)
                                                     == Ccode::Success)
                                                        ? 1
                                                        : 0;
    ntx.resp_aggregate.tag_min_security_ver_num.length = RotTagLength::TagMinSecurityVerNumLen;
    ntx.resp_aggregate.tag_min_security_ver_num.rsvd   = 0;
    ntx.resp_aggregate.tag_min_security_ver_num.b      = 0;
    auto min_svn_data_short = static_cast<uint16_t>(min_svn_data & UINT16_MAX);
    memcpy(&ntx.resp_aggregate.tag_min_security_ver_num.data,
           &min_svn_data_short,
           sizeof(uint16_t));

    // TAG 15, 0, InbandUpdatePolicyPersistent
    ntx.resp_aggregate.tag_inband_update_policy_persistent
        .tag                                                 = TagInbandUpdatePolicyPersistent;
    ntx.resp_aggregate.tag_inband_update_policy_persistent.v = 1;
    ntx.resp_aggregate.tag_inband_update_policy_persistent
        .length = RotTagLength::TagInbandUpdatePolicyPersistentLen;
    ntx.resp_aggregate.tag_inband_update_policy_persistent.rsvd    = 0;
    ntx.resp_aggregate.tag_inband_update_policy_persistent.b       = 0;
    ntx.resp_aggregate.tag_inband_update_policy_persistent.data[0] = static_cast<uint8_t>(
        NsmInBandUpdatePolicy::NotApplicable);

    // TAG 16, 3, BootStatusCode
    ntx.resp_aggregate.tag_boot_status_code.tag    = TagBootStatusCode;
    ntx.resp_aggregate.tag_boot_status_code.v      = 1;
    ntx.resp_aggregate.tag_boot_status_code.length = RotTagLength::TagBootStatusCodeLen;
    ntx.resp_aggregate.tag_boot_status_code.rsvd   = 0;
    ntx.resp_aggregate.tag_boot_status_code.b      = 0;

    fill_boot_status_code(boot_status_code);
    memcpy(&ntx.resp_aggregate.tag_boot_status_code.data,
           boot_status_code.data(),
           sizeof(boot_status_code));

    // TAG 17, 0, InbandUpdatePolicyCurrent
    ntx.resp_aggregate.tag_inband_update_policy_current.tag = TagInbandUpdatePolicyCurrent;
    ntx.resp_aggregate.tag_inband_update_policy_current.v   = 1;
    ntx.resp_aggregate.tag_inband_update_policy_current
        .length = RotTagLength::TagInbandUpdatePolicyCurrentLen;
    ntx.resp_aggregate.tag_inband_update_policy_current.rsvd    = 0;
    ntx.resp_aggregate.tag_inband_update_policy_current.b       = 0;
    ntx.resp_aggregate.tag_inband_update_policy_current.data[0] = static_cast<uint8_t>(
        NsmInBandUpdatePolicy::NotApplicable);

    // TAG 18, 0, RedundancyPolicyCurrent
    ntx.resp_aggregate.tag_redundancy_policy_current.tag = TagRedundancyPolicyCurrent;
    ntx.resp_aggregate.tag_redundancy_policy_current.v   = 1;
    ntx.resp_aggregate.tag_redundancy_policy_current
        .length = RotTagLength::TagRedundancyPolicyCurrentLen;
    ntx.resp_aggregate.tag_redundancy_policy_current.rsvd    = 0;
    ntx.resp_aggregate.tag_redundancy_policy_current.b       = 0;
    ntx.resp_aggregate.tag_redundancy_policy_current.data[0] = static_cast<uint8_t>(
        redundancy_policy_current);

    // TAG 19, 2, ApSkuId
    ntx.resp_aggregate.tag_ap_sku_id.tag    = TagApSkuId;
    ntx.resp_aggregate.tag_ap_sku_id.v      = 1;
    ntx.resp_aggregate.tag_ap_sku_id.length = RotTagLength::TagApSkuIdLen;
    ntx.resp_aggregate.tag_ap_sku_id.rsvd   = 0;
    ntx.resp_aggregate.tag_ap_sku_id.b      = 0;
    // WAR: Return SKU in big endian order (Bug-5855594)
    ntx.resp_aggregate.tag_ap_sku_id.data[0] = static_cast<uint8_t>((ap_sku_id >> ByteShift3)
                                                                    & UINT8_MAX);
    ntx.resp_aggregate.tag_ap_sku_id.data[1] = static_cast<uint8_t>((ap_sku_id >> ByteShift2)
                                                                    & UINT8_MAX);
    ntx.resp_aggregate.tag_ap_sku_id.data[2] = static_cast<uint8_t>((ap_sku_id >> ByteShift1)
                                                                    & UINT8_MAX);
    ntx.resp_aggregate.tag_ap_sku_id.data[3] = static_cast<uint8_t>(ap_sku_id & UINT8_MAX);

    // TAG 20, 0, GlobalFailoverPolicy
    ntx.resp_aggregate.tag_global_failover_policy.tag = TagGlobalFailoverPolicy;
    ntx.resp_aggregate.tag_global_failover_policy.v   = 1;
    ntx.resp_aggregate.tag_global_failover_policy
        .length = RotTagLength::TagGlobalFailoverPolicyLen;
    ntx.resp_aggregate.tag_global_failover_policy.rsvd    = 0;
    ntx.resp_aggregate.tag_global_failover_policy.b       = 0;
    ntx.resp_aggregate.tag_global_failover_policy.data[0] = static_cast<uint8_t>(
        NsmGlobalFailoverPolicy::NotApplicable);

    // TAG 5,  0, FirmwareSlotCount
    ntx.resp_aggregate.tag_firmware_slot_count.tag     = TagFirmwareSlotCount;
    ntx.resp_aggregate.tag_firmware_slot_count.v       = 1;
    ntx.resp_aggregate.tag_firmware_slot_count.length  = RotTagLength::TagFirmwareSlotCountLen;
    ntx.resp_aggregate.tag_firmware_slot_count.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_slot_count.b       = 0;
    ntx.resp_aggregate.tag_firmware_slot_count.data[0] = 2;  // 2 slots

    // TAG 6,  0, FirmwareSlotID for slot 0
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.tag     = TagFirmwareSlotId;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.v       = 1;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.length  = RotTagLength::TagFirmwareSlotIdLen;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.b       = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.data[0] = Slot0Id;

    // TAG 7,  5, Firmware version string for slot 0
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.tag = TagFirmwareVerString;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.v   = 1;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0
        .length = RotTagLength::TagFirmwareVerStringLen;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.rsvd = 0;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.b    = 0;

    if (active_slot == Slot0Id) {
        pldm::pldm_get_active_version(major, minor, patch, build);
    }
    else {
        pldm::pldm_get_pending_version(major, minor, patch, build);
    }

    if (ntx.resp_aggregate.tag_firmware_ver_string_slot0.v == 1) {
        generate_fw_version(fw_version, major, minor, patch, build);
        memcpy(&ntx.resp_aggregate.tag_firmware_ver_string_slot0.data,
               fw_version.data(),
               sizeof(fw_version));
    }

    // TAG 8,  2, VersionComparisonStamp for slot 0
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.tag = TagVerComparisonStamp;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.v   = 1;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0
        .length = RotTagLength::TagVerComparisonStampLen;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.rsvd = 0;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.b    = 0;

    if (ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.v == 1) {
        uint32_t compare_stamp = 0;
        ada_populate_stamp(minor, patch, build, &compare_stamp);
        memcpy(&ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.data,
               &compare_stamp,
               sizeof(uint32_t));
    }

    // TAG 9,  0, BuildType for slot 0
    ntx.resp_aggregate.tag_build_type_slot0.tag     = TagBuildType;
    ntx.resp_aggregate.tag_build_type_slot0.v       = (fill_build_type(build_type)) ? 1 : 0;
    ntx.resp_aggregate.tag_build_type_slot0.length  = RotTagLength::TagBuildTypeLen;
    ntx.resp_aggregate.tag_build_type_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_build_type_slot0.b       = 0;
    ntx.resp_aggregate.tag_build_type_slot0.data[0] = build_type;

    // TAG 10,  0, SigningType for slot 0
    ntx.resp_aggregate.tag_signing_type_slot0.tag = TagSigningType;
    ntx.resp_aggregate.tag_signing_type_slot0.v   = (key_data_valid_slot0 == Ccode::Success) ? 1
                                                                                             : 0;
    ntx.resp_aggregate.tag_signing_type_slot0.length  = RotTagLength::TagSigningTypeLen;
    ntx.resp_aggregate.tag_signing_type_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_signing_type_slot0.b       = 0;
    ntx.resp_aggregate.tag_signing_type_slot0.data[0] = fill_signing_type(
        static_cast<uint8_t>(key_index_slot0 & UINT8_MAX));

    // TAG 4,   0, WriteProtectState for slot 0
    ntx.resp_aggregate.tag_write_protect_state_slot0.tag = TagWriteProtectState;
    ntx.resp_aggregate.tag_write_protect_state_slot0.v   = (gpio_status == nv::gpio::Status::Ok)
                                                             ? 1
                                                             : 0;
    ntx.resp_aggregate.tag_write_protect_state_slot0
        .length = RotTagLength::TagWriteProtectStateLen;
    ntx.resp_aggregate.tag_write_protect_state_slot0.rsvd = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot0.b    = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot0
        .data[0] = (wp_state == static_cast<uint8_t>(nv::gpio::GpioState::High)) ? 1 : 0;

    // TAG 11,  0, Firmware State for slot 0
    ntx.resp_aggregate.tag_firmware_state_slot0.tag     = TagFirmwareState;
    ntx.resp_aggregate.tag_firmware_state_slot0.v       = 1;
    ntx.resp_aggregate.tag_firmware_state_slot0.length  = RotTagLength::TagFirmwareStateLen;
    ntx.resp_aggregate.tag_firmware_state_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_state_slot0.b       = 0;
    ntx.resp_aggregate.tag_firmware_state_slot0.data[0] = (active_slot == Slot0Id)
                                                            ? static_cast<uint8_t>(
                                                                  common::Activated)
                                                            : get_inactive_fw_state();

    // TAG 12, 1, SecurityVersionNumber for slot 0
    ntx.resp_aggregate.tag_security_ver_num_slot0.tag    = TagSecurityVerNum;
    ntx.resp_aggregate.tag_security_ver_num_slot0.v      = (fill_sec_ver_num(Slot0Id, svn_data)
                                                       == Ccode::Success)
                                                             ? 1
                                                             : 0;
    ntx.resp_aggregate.tag_security_ver_num_slot0.length = RotTagLength::TagSecurityVerNumLen;
    ntx.resp_aggregate.tag_security_ver_num_slot0.rsvd   = 0;
    ntx.resp_aggregate.tag_security_ver_num_slot0.b      = 0;
    auto svn_data_short = static_cast<uint16_t>(svn_data & UINT16_MAX);
    memcpy(
        &ntx.resp_aggregate.tag_security_ver_num_slot0.data, &svn_data_short, sizeof(uint16_t));

    // TAG 14, 1, SigningKeyIndex for slot 0
    ntx.resp_aggregate.tag_signing_key_index_slot0.tag = TagSigningKeyIndex;
    ntx.resp_aggregate.tag_signing_key_index_slot0.v = (key_data_valid_slot0 == Ccode::Success)
                                                         ? 1
                                                         : 0;
    ntx.resp_aggregate.tag_signing_key_index_slot0.length = RotTagLength::TagSigningKeyIndexLen;
    ntx.resp_aggregate.tag_signing_key_index_slot0.rsvd   = 0;
    ntx.resp_aggregate.tag_signing_key_index_slot0.b      = 0;
    memcpy(&ntx.resp_aggregate.tag_signing_key_index_slot0.data,
           &key_index_slot0,
           sizeof(uint16_t));

    // TAG 6,  0, FirmwareSlotID for slot 1
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.tag     = TagFirmwareSlotId;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.v       = 1;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.length  = RotTagLength::TagFirmwareSlotIdLen;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.b       = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.data[0] = Slot1Id;

    // TAG 7,  5, Firmware version string for slot 1
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.tag = TagFirmwareVerString;
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.v   = 1;
    ntx.resp_aggregate.tag_firmware_ver_string_slot1
        .length = RotTagLength::TagFirmwareVerStringLen;
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.rsvd = 0;
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.b    = 0;

    if (active_slot == Slot1Id) {
        pldm::pldm_get_active_version(major, minor, patch, build);
    }
    else {
        pldm::pldm_get_pending_version(major, minor, patch, build);
    }

    if (ntx.resp_aggregate.tag_firmware_ver_string_slot1.v == 1) {
        generate_fw_version(fw_version, major, minor, patch, build);
        memcpy(&ntx.resp_aggregate.tag_firmware_ver_string_slot1.data,
               fw_version.data(),
               sizeof(fw_version));
    }

    // TAG 8,  2, VersionComparisonStamp for slot 1
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.tag = TagVerComparisonStamp;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.v   = 1;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1
        .length = RotTagLength::TagVerComparisonStampLen;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.rsvd = 0;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.b    = 0;

    if (ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.v == 1) {
        uint32_t compare_stamp = 0;
        ada_populate_stamp(minor, patch, build, &compare_stamp);
        memcpy(&ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.data,
               &compare_stamp,
               sizeof(uint32_t));
    }

    // TAG 9,  0, BuildType for slot 1
    ntx.resp_aggregate.tag_build_type_slot1.tag     = TagBuildType;
    ntx.resp_aggregate.tag_build_type_slot1.v       = (fill_build_type(build_type)) ? 1 : 0;
    ntx.resp_aggregate.tag_build_type_slot1.length  = RotTagLength::TagBuildTypeLen;
    ntx.resp_aggregate.tag_build_type_slot1.rsvd    = 0;
    ntx.resp_aggregate.tag_build_type_slot1.b       = 0;
    ntx.resp_aggregate.tag_build_type_slot1.data[0] = build_type;

    // TAG 10,  0, SigningType for slot 1
    ntx.resp_aggregate.tag_signing_type_slot1.tag = TagSigningType;
    ntx.resp_aggregate.tag_signing_type_slot1.v   = (key_data_valid_slot1 == Ccode::Success) ? 1
                                                                                             : 0;
    ntx.resp_aggregate.tag_signing_type_slot1.length  = RotTagLength::TagSigningTypeLen;
    ntx.resp_aggregate.tag_signing_type_slot1.rsvd    = 0;
    ntx.resp_aggregate.tag_signing_type_slot1.b       = 0;
    ntx.resp_aggregate.tag_signing_type_slot1.data[0] = fill_signing_type(
        static_cast<uint8_t>(key_index_slot1 & UINT8_MAX));

    // TAG 4,   0, WriteProtectState for slot 1
    ntx.resp_aggregate.tag_write_protect_state_slot1.tag = TagWriteProtectState;
    ntx.resp_aggregate.tag_write_protect_state_slot1.v   = (gpio_status == nv::gpio::Status::Ok)
                                                             ? 1
                                                             : 0;
    ntx.resp_aggregate.tag_write_protect_state_slot1
        .length = RotTagLength::TagWriteProtectStateLen;
    ntx.resp_aggregate.tag_write_protect_state_slot1.rsvd = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot1.b    = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot1
        .data[0] = (wp_state == static_cast<uint8_t>(nv::gpio::GpioState::High)) ? 1 : 0;

    // TAG 11,  0, Firmware State for slot 1
    ntx.resp_aggregate.tag_firmware_state_slot1.tag     = TagFirmwareState;
    ntx.resp_aggregate.tag_firmware_state_slot1.v       = 1;
    ntx.resp_aggregate.tag_firmware_state_slot1.length  = RotTagLength::TagFirmwareStateLen;
    ntx.resp_aggregate.tag_firmware_state_slot1.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_state_slot1.b       = 0;
    ntx.resp_aggregate.tag_firmware_state_slot1.data[0] = (active_slot == Slot1Id)
                                                            ? static_cast<uint8_t>(
                                                                  common::Activated)
                                                            : get_inactive_fw_state();

    // TAG 12, 1, SecurityVersionNumber for slot 1
    ntx.resp_aggregate.tag_security_ver_num_slot1.tag    = TagSecurityVerNum;
    ntx.resp_aggregate.tag_security_ver_num_slot1.v      = (fill_sec_ver_num(Slot1Id, svn_data)
                                                       == Ccode::Success)
                                                             ? 1
                                                             : 0;
    ntx.resp_aggregate.tag_security_ver_num_slot1.length = RotTagLength::TagSecurityVerNumLen;
    ntx.resp_aggregate.tag_security_ver_num_slot1.rsvd   = 0;
    ntx.resp_aggregate.tag_security_ver_num_slot1.b      = 0;
    svn_data_short = static_cast<uint16_t>(svn_data & UINT16_MAX);
    memcpy(
        &ntx.resp_aggregate.tag_security_ver_num_slot1.data, &svn_data_short, sizeof(uint16_t));

    // TAG 14, 1, SigningKeyIndex for slot 1
    ntx.resp_aggregate.tag_signing_key_index_slot1.tag = TagSigningKeyIndex;
    ntx.resp_aggregate.tag_signing_key_index_slot1.v = (key_data_valid_slot1 == Ccode::Success)
                                                         ? 1
                                                         : 0;
    ntx.resp_aggregate.tag_signing_key_index_slot1.length = RotTagLength::TagSigningKeyIndexLen;
    ntx.resp_aggregate.tag_signing_key_index_slot1.rsvd   = 0;
    ntx.resp_aggregate.tag_signing_key_index_slot1.b      = 0;
    memcpy(&ntx.resp_aggregate.tag_signing_key_index_slot1.data,
           &key_index_slot1,
           sizeof(uint16_t));
}

void mctp::Nsm::get_ap_rot_state_info(const Packet&   rx,
                                      Packet&         tx,
                                      const uint16_t& component_id)
{
    uint16_t                key_index_update      = 0;
    uint32_t                svn_data              = 0;
    uint32_t                min_svn_data          = 0;
    uint32_t                ap_sku_id             = 0;
    Ccode                   key_data_valid_update = Ccode::Success;
    std::array<uint8_t, 16> comp_version_str{};
    fill_packet_header_aggr(rx, tx);
    fill_nsm_msg_header_aggr(rx, tx);
    auto& ntx             = NsmPktRespAggr::from(tx);
    tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize + sizeof(RespAggregate)
                          - sizeof(SlotSpecificSegment);
    ntx.completion_code = Ccode::Success;
    ntx.telemetry_count = TelemetryCount - SlotSegmentTelemetryCount;

    // TAG   LENG FIELD
    // TAG 1,  0, BackgroundCopyEnabled
    // TAG 2,  0, ActiveFirmwareSlot
    // TAG 3,  0, ActiveKeySet
    // TAG 4,  0, WriteProtectState
    // TAG 5,  0, FirmwareSlotCount
    // TAG 6,  0, FirmwareSlotID
    // TAG 7,  5, FirmwareVersionString
    // TAG 8,  2, VersionComparisonStamp
    // TAG 9,  0, BuildType
    // TAG 10, 0, SigningType
    // TAG 11, 0, FirmwareState
    // TAG 12, 1, SecurityVersionNumber
    // TAG 13, 1, MinimumSecurityVersionNumber
    // TAG 14, 1, SigningKeyIndex
    // TAG 15, 0, InbandUpdatePolicy
    // TAG 16, 3, BootStatusCode
    // TAG 17, 0, InbandUpdatePolicyCurrent
    // TAG 18, 0, RedundancyPolicyCurrent
    // TAG 19, 2, ApSkuId
    // TAG 20, 0, GlobalFailoverPolicy

    // Tag 1, 2, 3, 13, 15, 16, 17, 18, 19, 20
    // Tag 5 (slot count : n) (The following tag repeat n times)
    // slot 0 : Tag 6, 7, 8, 9, 10, 4, 11, 12, 14

    key_data_valid_update = fill_ap_key_index(key_index_update);
    // TAG 1,  0, RedundancyPolicyPersistent
    ntx.resp_aggregate.tag_redundancy_policy_persistent.tag = TagRedundancyPolicyPersistent;
    ntx.resp_aggregate.tag_redundancy_policy_persistent.v   = 1;
    ntx.resp_aggregate.tag_redundancy_policy_persistent
        .length = RotTagLength::TagRedundancyPolicyPersistentLen;
    ntx.resp_aggregate.tag_redundancy_policy_persistent.rsvd    = 0;
    ntx.resp_aggregate.tag_redundancy_policy_persistent.b       = 0;
    ntx.resp_aggregate.tag_redundancy_policy_persistent.data[0] = static_cast<uint8_t>(
        NsmRedundancyPolicy::NotApplicable);

    // TAG 2,  0, ActiveSlot
    ntx.resp_aggregate.tag_active_firmware_slot.tag    = TagActiveFirmwareSlot;
    ntx.resp_aggregate.tag_active_firmware_slot.v      = 1;
    ntx.resp_aggregate.tag_active_firmware_slot.length = RotTagLength::TagActiveFirmwareSlotLen;
    ntx.resp_aggregate.tag_active_firmware_slot.rsvd   = 0;
    ntx.resp_aggregate.tag_active_firmware_slot.b      = 0;
    ntx.resp_aggregate.tag_active_firmware_slot.data[0] = 0;

    // TAG 3,  0, ActiveKeySet
    ntx.resp_aggregate.tag_active_key_set.tag    = TagActiveKeySet;
    ntx.resp_aggregate.tag_active_key_set.v      = 1;
    ntx.resp_aggregate.tag_active_key_set.length = RotTagLength::TagActiveKeySetLen;
    ntx.resp_aggregate.tag_active_key_set.rsvd   = 0;
    ntx.resp_aggregate.tag_active_key_set.b      = 0;

    const uint8_t KeySet                          = 0;
    ntx.resp_aggregate.tag_active_key_set.data[0] = KeySet;

    // TAG 13, 1, MinimumSecurityVersionNumber
    fill_ap_fuse_mini_sec_ver_num(min_svn_data);
    ntx.resp_aggregate.tag_min_security_ver_num.tag    = TagMinSecurityVerNum;
    ntx.resp_aggregate.tag_min_security_ver_num.v      = 1;
    ntx.resp_aggregate.tag_min_security_ver_num.length = RotTagLength::TagMinSecurityVerNumLen;
    ntx.resp_aggregate.tag_min_security_ver_num.rsvd   = 0;
    ntx.resp_aggregate.tag_min_security_ver_num.b      = 0;
    auto min_svn_data_short = static_cast<uint16_t>(min_svn_data & UINT16_MAX);
    memcpy(&ntx.resp_aggregate.tag_min_security_ver_num.data,
           &min_svn_data_short,
           sizeof(uint16_t));

    // TAG 15, 0, InbandUpdatePolicyPersistent
    ntx.resp_aggregate.tag_inband_update_policy_persistent
        .tag                                                 = TagInbandUpdatePolicyPersistent;
    ntx.resp_aggregate.tag_inband_update_policy_persistent.v = 1;
    ntx.resp_aggregate.tag_inband_update_policy_persistent
        .length = RotTagLength::TagInbandUpdatePolicyPersistentLen;
    ntx.resp_aggregate.tag_inband_update_policy_persistent.rsvd    = 0;
    ntx.resp_aggregate.tag_inband_update_policy_persistent.b       = 0;
    ntx.resp_aggregate.tag_inband_update_policy_persistent.data[0] = static_cast<uint8_t>(
        NsmInBandUpdatePolicy::NotApplicable);

    // TAG 16, 3, BootStatusCode
    ntx.resp_aggregate.tag_boot_status_code.tag    = TagBootStatusCode;
    ntx.resp_aggregate.tag_boot_status_code.v      = 0;
    ntx.resp_aggregate.tag_boot_status_code.length = RotTagLength::TagBootStatusCodeLen;
    ntx.resp_aggregate.tag_boot_status_code.rsvd   = 0;
    ntx.resp_aggregate.tag_boot_status_code.b      = 0;

    // TAG 17, 0, InbandUpdatePolicyCurrent
    ntx.resp_aggregate.tag_inband_update_policy_current.tag = TagInbandUpdatePolicyCurrent;
    ntx.resp_aggregate.tag_inband_update_policy_current.v   = 1;
    ntx.resp_aggregate.tag_inband_update_policy_current
        .length = RotTagLength::TagInbandUpdatePolicyCurrentLen;
    ntx.resp_aggregate.tag_inband_update_policy_current.rsvd    = 0;
    ntx.resp_aggregate.tag_inband_update_policy_current.b       = 0;
    ntx.resp_aggregate.tag_inband_update_policy_current.data[0] = static_cast<uint8_t>(
        NsmInBandUpdatePolicy::NotApplicable);

    // TAG 18, 0, RedundancyPolicyCurrent
    ntx.resp_aggregate.tag_redundancy_policy_current.tag = TagRedundancyPolicyCurrent;
    ntx.resp_aggregate.tag_redundancy_policy_current.v   = 1;
    ntx.resp_aggregate.tag_redundancy_policy_current
        .length = RotTagLength::TagRedundancyPolicyCurrentLen;
    ntx.resp_aggregate.tag_redundancy_policy_current.rsvd    = 0;
    ntx.resp_aggregate.tag_redundancy_policy_current.b       = 0;
    ntx.resp_aggregate.tag_redundancy_policy_current.data[0] = static_cast<uint8_t>(
        NsmInBandUpdatePolicy::NotApplicable);

    // TAG 19, 2, ApSkuId
    if constexpr (pldm::ApNum > 0) {
        for (uint8_t i = 0; i < pldm::ApNum; ++i) {
            if (pldm::AllApComponentId.at(i) == component_id) {
                ap_sku_id = pldm::FwInfoList.at(i).ap_sku_id;
                break;
            }
        }
    }
    ntx.resp_aggregate.tag_ap_sku_id.tag    = TagApSkuId;
    ntx.resp_aggregate.tag_ap_sku_id.v      = 1;
    ntx.resp_aggregate.tag_ap_sku_id.length = RotTagLength::TagApSkuIdLen;
    ntx.resp_aggregate.tag_ap_sku_id.rsvd   = 0;
    ntx.resp_aggregate.tag_ap_sku_id.b      = 0;
    // WAR: Return SKU in big endian order (Bug-5855594)
    ntx.resp_aggregate.tag_ap_sku_id.data[0] = static_cast<uint8_t>((ap_sku_id >> ByteShift3)
                                                                    & UINT8_MAX);
    ntx.resp_aggregate.tag_ap_sku_id.data[1] = static_cast<uint8_t>((ap_sku_id >> ByteShift2)
                                                                    & UINT8_MAX);
    ntx.resp_aggregate.tag_ap_sku_id.data[2] = static_cast<uint8_t>((ap_sku_id >> ByteShift1)
                                                                    & UINT8_MAX);
    ntx.resp_aggregate.tag_ap_sku_id.data[3] = static_cast<uint8_t>(ap_sku_id & UINT8_MAX);

    // TAG 20, 0, GlobalFailoverPolicy
    ntx.resp_aggregate.tag_global_failover_policy.tag = TagGlobalFailoverPolicy;
    ntx.resp_aggregate.tag_global_failover_policy.v   = 1;
    ntx.resp_aggregate.tag_global_failover_policy
        .length = RotTagLength::TagGlobalFailoverPolicyLen;
    ntx.resp_aggregate.tag_global_failover_policy.rsvd    = 0;
    ntx.resp_aggregate.tag_global_failover_policy.b       = 0;
    ntx.resp_aggregate.tag_global_failover_policy.data[0] = static_cast<uint8_t>(
        NsmGlobalFailoverPolicy::NotApplicable);

    // TAG 5,  0, FirmwareSlotCount
    ntx.resp_aggregate.tag_firmware_slot_count.tag     = TagFirmwareSlotCount;
    ntx.resp_aggregate.tag_firmware_slot_count.v       = 1;
    ntx.resp_aggregate.tag_firmware_slot_count.length  = RotTagLength::TagFirmwareSlotCountLen;
    ntx.resp_aggregate.tag_firmware_slot_count.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_slot_count.b       = 0;
    ntx.resp_aggregate.tag_firmware_slot_count.data[0] = 1;  // 1 slots

    // TAG 6,  0, FirmwareSlotID for slot 0
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.tag     = TagFirmwareSlotId;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.v       = 1;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.length  = RotTagLength::TagFirmwareSlotIdLen;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.b       = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.data[0] = 0;

    // TAG 7,  5, Firmware version string for slot 0
    auto comp_version_str_status = get_ap_comp_version_str(
        fw_parser::ap::ParsingApFwType::UpdateSlot);
    if (comp_version_str_status.has_value()) {
        comp_version_str = *comp_version_str_status;
    }

    ntx.resp_aggregate.tag_firmware_ver_string_slot0.tag = TagFirmwareVerString;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.v   = (comp_version_str_status.has_value())
                                                             ? 1
                                                             : 0;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0
        .length = RotTagLength::TagFirmwareVerStringLen;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.rsvd = 0;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.b    = 0;

    if (ntx.resp_aggregate.tag_firmware_ver_string_slot0.v) {
        memcpy(&ntx.resp_aggregate.tag_firmware_ver_string_slot0.data,
               comp_version_str.data(),
               sizeof(comp_version_str));
    }

    // TAG 8,  2, VersionComparisonStamp for slot 0
    auto ap_fw_version = get_ap_fw_version(fw_parser::ap::ParsingApFwType::UpdateSlot);
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.tag = TagVerComparisonStamp;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.v   = (key_data_valid_update
                                                           == Ccode::Success)
                                                              ? 1
                                                              : 0;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0
        .length = RotTagLength::TagVerComparisonStampLen;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.rsvd = 0;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.b    = 0;

    if (ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.v) {
        memcpy(&ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.data,
               &ap_fw_version,
               sizeof(uint32_t));
    }

    // TAG 9,  0, BuildType for slot 0
    auto build_type = get_ap_build_type(fw_parser::ap::ParsingApFwType::UpdateSlot);
    ntx.resp_aggregate.tag_build_type_slot0.tag     = TagBuildType;
    ntx.resp_aggregate.tag_build_type_slot0.v       = (build_type.has_value()) ? 1 : 0;
    ntx.resp_aggregate.tag_build_type_slot0.length  = RotTagLength::TagBuildTypeLen;
    ntx.resp_aggregate.tag_build_type_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_build_type_slot0.b       = 0;
    ntx.resp_aggregate.tag_build_type_slot0.data[0] = *build_type;

    // TAG 10,  0, SigningType for slot 0
    ntx.resp_aggregate.tag_signing_type_slot0.tag = TagSigningType;
    ntx.resp_aggregate.tag_signing_type_slot0.v = (key_data_valid_update == Ccode::Success) ? 1
                                                                                            : 0;
    ntx.resp_aggregate.tag_signing_type_slot0.length  = RotTagLength::TagSigningTypeLen;
    ntx.resp_aggregate.tag_signing_type_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_signing_type_slot0.b       = 0;
    ntx.resp_aggregate.tag_signing_type_slot0.data[0] = fill_signing_type(
        static_cast<uint8_t>(key_index_update & UINT8_MAX));

    // TAG 4,   0, WriteProtectState for slot 0
    ntx.resp_aggregate.tag_write_protect_state_slot0.tag = TagWriteProtectState;
    ntx.resp_aggregate.tag_write_protect_state_slot0.v   = 1;
    ntx.resp_aggregate.tag_write_protect_state_slot0
        .length = RotTagLength::TagWriteProtectStateLen;
    ntx.resp_aggregate.tag_write_protect_state_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot0.b       = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot0.data[0] = 0;  // disabled

    // TAG 11,  0, Firmware State for slot 0
    ntx.resp_aggregate.tag_firmware_state_slot0.tag     = TagFirmwareState;
    ntx.resp_aggregate.tag_firmware_state_slot0.v       = 1;
    ntx.resp_aggregate.tag_firmware_state_slot0.length  = RotTagLength::TagFirmwareStateLen;
    ntx.resp_aggregate.tag_firmware_state_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_state_slot0.b       = 0;
    ntx.resp_aggregate.tag_firmware_state_slot0.data[0] = get_ap_state();

    // TAG 12, 1, SecurityVersionNumber for slot 0
    ntx.resp_aggregate.tag_security_ver_num_slot0.tag    = TagSecurityVerNum;
    ntx.resp_aggregate.tag_security_ver_num_slot0.v      = (fill_ap_sec_ver_num(svn_data)
                                                       == Ccode::Success)
                                                             ? 1
                                                             : 0;
    ntx.resp_aggregate.tag_security_ver_num_slot0.length = RotTagLength::TagSecurityVerNumLen;
    ntx.resp_aggregate.tag_security_ver_num_slot0.rsvd   = 0;
    ntx.resp_aggregate.tag_security_ver_num_slot0.b      = 0;
    auto svn_data_short = static_cast<uint16_t>(svn_data & UINT16_MAX);
    memcpy(
        &ntx.resp_aggregate.tag_security_ver_num_slot0.data, &svn_data_short, sizeof(uint16_t));

    // TAG 14, 1, SigningKeyIndex for slot 0
    ntx.resp_aggregate.tag_signing_key_index_slot0.tag = TagSigningKeyIndex;
    ntx.resp_aggregate.tag_signing_key_index_slot0.v = (key_data_valid_update == Ccode::Success)
                                                         ? 1
                                                         : 0;
    ntx.resp_aggregate.tag_signing_key_index_slot0.length = RotTagLength::TagSigningKeyIndexLen;
    ntx.resp_aggregate.tag_signing_key_index_slot0.rsvd   = 0;
    ntx.resp_aggregate.tag_signing_key_index_slot0.b      = 0;
    memcpy(&ntx.resp_aggregate.tag_signing_key_index_slot0.data,
           &key_index_update,
           sizeof(uint16_t));
}

void mctp::Nsm::on_get_rot_state_info(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 5;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet_aggr(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    auto&      nrx = NsmPktReq::from(rx);
    FwCompInfo input_fw_comp_info{};
    memcpy(&input_fw_comp_info, &nrx.data, sizeof(FwCompInfo));

    // input check
    if (is_fw_comp_id_valid(input_fw_comp_info, McuComponentId)) {
        get_rot_state_info(rx, tx);
        return;
    }
    if constexpr (pldm::ApNum > 0) {
        for (uint8_t i = 0; i < pldm::ApNum; ++i) {
            if (is_fw_comp_id_valid(input_fw_comp_info, pldm::AllApComponentId.at(i))) {
                get_ap_rot_state_info(rx, tx, pldm::AllApComponentId.at(i));
                return;
            }
        }
    }
    fill_error_packet_aggr(Ccode::ErrorInvalidData, rx, tx);
}

void mctp::Nsm::query_auth_key(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RespSize = sizeof(QueryAuthKeyResp);
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;

    QueryAuthKeyResp query_auth_key_resp{};

    const uint8_t ActiveSlot     = get_active_slot();
    const uint8_t InactiveSlot   = (ActiveSlot == 0) ? 1 : 0;
    uint16_t      key_index      = 0;
    uint32_t      key_permission = 0;

    // Permission Bitmap Length 4
    query_auth_key_resp.bitmap_len = 4;
    // Active
    if (fill_key_index(ActiveSlot, key_index) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_auth_key_resp.active_comp_key = key_index;
    if (fill_key_permission(ActiveSlot, key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_auth_key_resp.active_permission = key_permission;

    if (common::PendingActivation == get_inactive_fw_state()) {
        // Pending
        if (fill_key_index(InactiveSlot, key_index) != Ccode::Success) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
        query_auth_key_resp.pending_comp_key = key_index;
        if (fill_key_permission(InactiveSlot, key_permission) != Ccode::Success) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
        query_auth_key_resp.pending_permission = key_permission;
    }
    else {
        // no pending -> 0xFFFF
        query_auth_key_resp.pending_comp_key   = UINT16_MAX;
        query_auth_key_resp.pending_permission = 0;
    }

    // EFUSE Key Permission Bitmap & Pending EFUSE Key Permission Bitmap
    if (fill_fuse_key_permission(key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_auth_key_resp.active_efuse_permission  = key_permission;
    query_auth_key_resp.pending_efuse_permission = key_permission;

    memcpy(&ntx.data, &query_auth_key_resp, sizeof(QueryAuthKeyResp));
}

void mctp::Nsm::query_ap_auth_key(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RespSize = sizeof(QueryAuthKeyResp);
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;

    QueryAuthKeyResp query_auth_key_resp{};

    uint16_t key_index      = 0;
    uint32_t key_permission = 0;

    // Permission Bitmap Length 4
    query_auth_key_resp.bitmap_len = 4;
    // Active
    if (fill_ap_key_index(key_index) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_auth_key_resp.active_comp_key = key_index;
    if (fill_ap_key_permission(key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_auth_key_resp.active_permission = key_permission;

    // no pending -> 0xFFFF
    query_auth_key_resp.pending_comp_key   = UINT16_MAX;
    query_auth_key_resp.pending_permission = 0;

    // EFUSE Key Permission Bitmap & Pending EFUSE Key Permission Bitmap
    if (fill_ap_fuse_key_permission(key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_auth_key_resp.active_efuse_permission  = key_permission;
    query_auth_key_resp.pending_efuse_permission = key_permission;

    memcpy(&ntx.data, &query_auth_key_resp, sizeof(QueryAuthKeyResp));
}

void mctp::Nsm::on_query_auth_key(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 5;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    auto&      nrx = NsmPktReq::from(rx);
    FwCompInfo input_fw_comp_info{};
    memcpy(&input_fw_comp_info, &nrx.data, sizeof(FwCompInfo));

    // input check
    if (is_fw_comp_id_valid(input_fw_comp_info, McuComponentId)) {
        query_auth_key(rx, tx);
        return;
    }
    if constexpr (pldm::ApNum > 0) {
        for (uint8_t i = 0; i < pldm::ApNum; ++i) {
            if (is_fw_comp_id_valid(input_fw_comp_info, pldm::AllApComponentId.at(i))) {
                query_ap_auth_key(rx, tx);
                return;
            }
        }
    }
    fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
}

void mctp::Nsm::update_auth_key(const Packet&           rx,
                                Packet&                 tx,
                                const UpdateAuthKeyReq& update_struct)
{
    constexpr uint8_t RespSize = 4;

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx                 = NsmPktResp::from(tx);
    tx.priv.packet_length     = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code       = Ccode::Success;
    ntx.data_size             = RespSize;
    uint32_t      bitfield    = 0;
    const uint8_t RequestType = update_struct.request_type;

    const uint8_t ActiveSlot   = get_active_slot();
    const uint8_t InactiveSlot = (ActiveSlot == Slot0Id) ? Slot1Id : Slot0Id;

    // key_permission will be the form of "(1 << N) - 1"
    uint32_t active_key_permission    = 0;
    uint32_t inactive_key_permission  = 0;
    uint32_t efuse_key_permission     = 0;
    uint32_t fmc_key_permission       = 0;
    uint32_t permitted_key_permission = 0;
    // For input
    uint32_t input_key_permission = 0;

    Rcode reason_code{};

    // bypassing the cases for PldmProcessActive and inactive slot and bg copy inprogress
    if (!can_revoke_otp(reason_code)) {
        fill_error_packet_v2(Ccode::ErrorGeneral, reason_code, rx, tx);
        return;
    }

    // Assume active slot is always autheticated
    // Active Component Key Permission Bitmap
    if (fill_key_permission(ActiveSlot, active_key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Check if inactive slot is authenticate
    if (!is_inactive_authenticate(InactiveSlot)) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Inactive Component Key Permission Bitmap
    if (fill_key_permission(InactiveSlot, inactive_key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // EFUSE Key Permission Bitmap
    if (fill_fuse_key_permission(efuse_key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Get the request Key Permission Bitmap
    if (RequestType == UpdateKeySpecifiedValue) {
        memcpy(&input_key_permission,
               &update_struct.permission_bitmap,
               sizeof(input_key_permission));
    }

    permitted_key_permission = active_key_permission & inactive_key_permission;

    if (RequestType == UpdateKeyPermittedValue) {
        input_key_permission = permitted_key_permission;
    }
    else {
        input_key_permission |= efuse_key_permission;

        // Check if the input key permission is valid
        // input_key_permission should be the form of "(1 << N) - 1"
        if (!is_form_zeros_then_ones(input_key_permission)) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }

    const uint8_t boot_source = bootloader::Driver::get_boot_src();
    // FMC used
    if (boot_source == sys::bootloader::Driver::BootSourceFMC) {
        // FMC Security Version Number
        if (fill_fmc_key_permission(fmc_key_permission) != Ccode::Success) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }

        permitted_key_permission &= fmc_key_permission;
        // Assign again since permitted_key_permission may change
        if (RequestType == UpdateKeyPermittedValue) {
            input_key_permission = permitted_key_permission;
        }

        if (input_key_permission > permitted_key_permission) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }
    else {
        if (RequestType == UpdateKeyPermittedValue) {
            input_key_permission = permitted_key_permission;
        }
        if (input_key_permission > permitted_key_permission) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }

    // revoke key permission only if input_key_permission > efuse_key_permission
    // If input_key_permission <= efuse_key_permission, will not revoke key permission but also
    // doesn't mean failed
    if (input_key_permission > efuse_key_permission) {
        auto revoke_ccode = revoke_key_permission(input_key_permission);
        if (revoke_ccode != Ccode::Success) {
            fill_error_packet_v2(Ccode::ErrorGeneral, Rcode::ErrorEfuseUpdateFailed, rx, tx);
            return;
        }

        logger::info(logger::Event::MctpNsmRevokeKey,
                     logger::data_from_u32(input_key_permission));

        Driver::mctp_send_cmd(Driver::CmdCode::RotStateInfoChange);
    }

    bitfield |= (1U << uint8_t(0));  // [0] – Automatic. EFUSE updated at completion of request.
    memcpy(&ntx.data, &bitfield, RespSize);
}

void mctp::Nsm::update_ap_auth_key(const Packet&           rx,
                                   Packet&                 tx,
                                   const UpdateAuthKeyReq& update_struct)
{
    constexpr uint8_t RespSize = 4;

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx                 = NsmPktResp::from(tx);
    tx.priv.packet_length     = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code       = Ccode::Success;
    ntx.data_size             = RespSize;
    uint32_t      bitfield    = 0;
    const uint8_t RequestType = update_struct.request_type;

    // key_permission will be the form of "(1 << N) - 1"
    uint32_t active_key_permission    = 0;
    uint32_t efuse_key_permission     = 0;
    uint32_t permitted_key_permission = 0;
    // For input
    uint32_t input_key_permission = 0;

    auto can_revoke = can_revoke_ap_otp();

    if (can_revoke != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Key Permission Bitmap
    if (fill_ap_key_permission(active_key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // EFUSE Key Permission Bitmap
    if (fill_ap_fuse_key_permission(efuse_key_permission) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Get the request Key Permission Bitmap
    if (RequestType == UpdateKeySpecifiedValue) {
        memcpy(&input_key_permission,
               &update_struct.permission_bitmap,
               sizeof(input_key_permission));
    }

    permitted_key_permission = active_key_permission;

    if (RequestType == UpdateKeyPermittedValue) {
        input_key_permission = permitted_key_permission;
    }
    else {
        input_key_permission |= efuse_key_permission;

        // Check if the input key permission is valid
        // input_key_permission should be the form of "(1 << N) - 1"
        if (!is_form_zeros_then_ones(input_key_permission)) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }

    if (input_key_permission > permitted_key_permission) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // revoke key permission only if input_key_permission > efuse_key_permission
    // If input_key_permission <= efuse_key_permission, will not revoke key permission but also
    // doesn't mean failed
    if (input_key_permission > efuse_key_permission) {
        auto revoke_ccode = revoke_ap_key_permission(input_key_permission);
        if (revoke_ccode != Ccode::Success) {
            fill_error_packet_v2(Ccode::ErrorGeneral, Rcode::ErrorEfuseUpdateFailed, rx, tx);
            return;
        }

        const uint32_t
            comp_info = static_cast<uint32_t>(update_struct.fw_comp_info.component_index) << 16
                      | static_cast<uint32_t>(update_struct.fw_comp_info.component_id);
        logger::info(logger::Event::MctpNsmRevokeApKey,
                     logger::data_from_two_u32(input_key_permission, comp_info));

        Driver::mctp_send_cmd(Driver::CmdCode::RotStateInfoChange);
    }

    bitfield |= (1U << uint8_t(0));  // [0] – Automatic. EFUSE updated at completion of request.
    memcpy(&ntx.data, &bitfield, RespSize);
}

void mctp::Nsm::on_update_auth_key(const Packet& rx, Packet& tx)
{
    auto&             nrx                 = NsmPktReq::from(rx);
    uint8_t           addition_size       = 4;
    constexpr uint8_t MinRequestSize      = 15;
    constexpr uint8_t PermissionBitmapLen = 4;
    if (rx.priv.packet_length < sizeof(Header) + HeaderRequestSize) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    auto input_data_size = rx.priv.packet_length - sizeof(Header) - HeaderRequestSize;
    if (input_data_size > static_cast<uint8_t>(MinRequestSize + addition_size)) {
        input_data_size = MinRequestSize + addition_size;
    }

    UpdateAuthKeyReq update_auth_key_req{};
    // Read data
    memcpy(&update_auth_key_req, &nrx.data, input_data_size);

    if (is_irreversible_ctrl_enabled()) {
        // disable once this command (update auth key) was request
        disable_irreversible_ctrl();
        if (update_auth_key_req.request_type == UpdateKeySpecifiedValue
            || update_auth_key_req.request_type == UpdateKeyPermittedValue) {
            addition_size = (update_auth_key_req.request_type == UpdateKeySpecifiedValue)
                              ? PermissionBitmapLen
                              : 0;
            // check input length
            if (!is_input_length_valid(rx, MinRequestSize + addition_size)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }
            // check if request bitmap len match PermissionBitmapLen
            if (update_auth_key_req.request_type == UpdateKeySpecifiedValue
                && update_auth_key_req.bitmap_len != PermissionBitmapLen) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }
            // check nonce bytes
            if (!is_nonce_match(update_auth_key_req.nonce)) {
                fill_error_packet_v2(Ccode::ErrorGeneral, Rcode::ErrorNonceMismatch, rx, tx);
                return;
            }
            // check comp info(comp class, comp id, comp class idx)
            if (is_fw_comp_id_valid(update_auth_key_req.fw_comp_info, McuComponentId)) {
                update_auth_key(rx, tx, update_auth_key_req);
                return;
            }
            if constexpr (pldm::ApNum > 0) {
                for (uint8_t i = 0; i < pldm::ApNum; ++i) {
                    if (is_fw_comp_id_valid(update_auth_key_req.fw_comp_info,
                                            pldm::AllApComponentId.at(i))) {
                        update_ap_auth_key(rx, tx, update_auth_key_req);
                        return;
                    }
                }
            }
        }
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    }
    else {
        fill_error_packet_v2(Ccode::ErrorGeneral, Rcode::ErrorIrreversibleConfDisable, rx, tx);
    }
}

bool mctp::Nsm::is_irreversible_ctrl_enabled()
{
    // TODO - Ensure the timeout is 120 second - GFWLYNT1-374
    if (!irreversible_input_received) {
        return false;
    }

    const uint32_t CurrentTimeStamp = sys::ipc::get_os_ticks();
    const uint32_t TimeDiff         = CurrentTimeStamp - irreversible_last_time_stamp;

    if (TimeDiff < NvMctpIrreversibleCtrlTimeout) {
        return true;
    }
    else {
        return false;
    }
}

void mctp::Nsm::disable_irreversible_ctrl()
{
    irreversible_input_received = false;
}

void mctp::Nsm::on_ctrl_irreversible_conf(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx           = NsmPktReq::from(rx);
    auto& ntx           = NsmPktResp::from(tx);
    ntx.completion_code = Ccode::Success;
    ntx.data_size       = 0;

    switch (nrx.data[0]) {
        case IrreversibleCtrlQuery:
            tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + 1;
            ntx.data_size         = 1;
            ntx.data[0]           = is_irreversible_ctrl_enabled();
            break;
        case IrreversibleCtrlDisable:
            tx.priv.packet_length       = sizeof(Header) + HeaderResponseSize;
            irreversible_input_received = false;
            break;
        case IrreversibleCtrlEnable: {
            tx.priv.packet_length        = sizeof(Header) + HeaderResponseSize + sizeof(_nonce);
            ntx.data_size                = sizeof(_nonce);
            irreversible_input_received  = true;
            irreversible_last_time_stamp = sys::ipc::get_os_ticks();

            // Random number generator
            mbedtls_ctr_drbg_context ctr_drbg;
            mbedtls_ctr_drbg_init(&ctr_drbg);
            const int Ret = mbedtls_ctr_drbg_random(
                &ctr_drbg, static_cast<uint8_t*>(_nonce.nonce), sizeof(_nonce));
            mbedtls_ctr_drbg_free(&ctr_drbg);
            if (Ret != 0) {
                irreversible_input_received = false;
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            memcpy(&ntx.data, &_nonce, NvMctpNsmNonceSize);
        } break;
        default: fill_error_packet(Ccode::ErrorInvalidData, rx, tx); break;
    }
}

void mctp::Nsm::query_sec_ver_num(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RespSize = sizeof(QuerySvnResp);
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;

    QuerySvnResp query_svn_resp{};

    const uint8_t ActiveSlot   = get_active_slot();
    const uint8_t InactiveSlot = (ActiveSlot == Slot0Id) ? Slot1Id : Slot0Id;
    uint32_t      svn_data     = 0;
    uint32_t      min_svn_data = 0;

    // Active Component Security Version Number, index 0
    if (fill_sec_ver_num(ActiveSlot, svn_data) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_svn_resp.active_svn = static_cast<uint16_t>(svn_data & UINT16_MAX);

    // Minimum Security Version Number, MIN_SVN stored in EFUSE, index 4
    if (fill_fuse_mini_sec_ver_num(min_svn_data) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_svn_resp.min_svn = static_cast<uint16_t>(min_svn_data & UINT16_MAX);

    if (common::PendingActivation == get_inactive_fw_state()) {
        // Pending Component Security Version Number, index 2
        if (fill_sec_ver_num(InactiveSlot, svn_data) != Ccode::Success) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
        query_svn_resp.pending_svn = static_cast<uint16_t>(svn_data & UINT16_MAX);

        // Pending Minimum Security Version Number, If no pending, set to 0.
        query_svn_resp.pending_min_svn = static_cast<uint16_t>(min_svn_data & UINT16_MAX);
    }
    else {
        // no pending -> 0
        query_svn_resp.pending_svn     = 0;
        query_svn_resp.pending_min_svn = 0;
    }

    memcpy(&ntx.data, &query_svn_resp, sizeof(QuerySvnResp));
}

void mctp::Nsm::query_ap_sec_ver_num(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RespSize = sizeof(QuerySvnResp);
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;

    QuerySvnResp query_svn_resp{};

    uint32_t svn_data     = 0;
    uint32_t min_svn_data = 0;

    // Security Version Number, index 0
    if (fill_ap_sec_ver_num(svn_data) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_svn_resp.active_svn = static_cast<uint16_t>(svn_data & UINT16_MAX);

    // Minimum Security Version Number, MIN_SVN stored in EFUSE, index 4
    if (fill_ap_fuse_mini_sec_ver_num(min_svn_data) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    query_svn_resp.min_svn = static_cast<uint16_t>(min_svn_data & UINT16_MAX);

    // no pending -> 0
    query_svn_resp.pending_svn     = 0;
    query_svn_resp.pending_min_svn = 0;

    memcpy(&ntx.data, &query_svn_resp, sizeof(QuerySvnResp));
}

void mctp::Nsm::on_query_sec_ver_num(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 5;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto&      nrx = NsmPktReq::from(rx);
    FwCompInfo input_fw_comp_info{};
    memcpy(&input_fw_comp_info, &nrx.data, sizeof(FwCompInfo));

    // input check
    if (is_fw_comp_id_valid(input_fw_comp_info, McuComponentId)) {
        query_sec_ver_num(rx, tx);
        return;
    }
    if constexpr (pldm::ApNum > 0) {
        for (uint8_t i = 0; i < pldm::ApNum; ++i) {
            if (is_fw_comp_id_valid(input_fw_comp_info, pldm::AllApComponentId.at(i))) {
                query_ap_sec_ver_num(rx, tx);
                return;
            }
        }
    }
    fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
}

void mctp::Nsm::update_min_sec_ver_num(const Packet&          rx,
                                       Packet&                tx,
                                       const UpdateMinSvnReq& update_struct)
{
    constexpr uint8_t RespSize = 4;

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx                 = NsmPktResp::from(tx);
    tx.priv.packet_length     = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code       = Ccode::Success;
    ntx.data_size             = RespSize;
    uint32_t      bitfield    = 0;
    const uint8_t RequestType = update_struct.request_type;

    const uint8_t ActiveSlot    = get_active_slot();
    const uint8_t InactiveSlot  = (ActiveSlot == 0) ? 1 : 0;
    uint32_t      active_svn    = 0;
    uint32_t      inactive_svn  = 0;
    uint16_t      input_min_svn = 0;
    uint16_t      permitted_svn = 0;
    uint32_t      min_svn_data  = 0;
    uint32_t      fmc_svn       = 0;

    Rcode reason_code{};

    // bypassing the cases for PldmProcessActive and inactive slot and bg copy inprogress
    if (!can_revoke_otp(reason_code)) {
        fill_error_packet_v2(Ccode::ErrorGeneral, reason_code, rx, tx);
        return;
    }

    // Active Component Security Version Number
    if (fill_sec_ver_num(ActiveSlot, active_svn) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Check if inactive slot is authenticate
    if (!is_inactive_authenticate(InactiveSlot)) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Inactive Component Security Version Number, index 2
    if (fill_sec_ver_num(InactiveSlot, inactive_svn) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Minimum Security Version Number, MIN_SVN stored in EFUSE
    if (fill_fuse_mini_sec_ver_num(min_svn_data) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    if (RequestType == UpdateKeySpecifiedValue) {
        // Requested Minimum Security Version Number 14
        memcpy(&input_min_svn, &update_struct.min_svn, sizeof(input_min_svn));
    }

    // find the minimum svn of active, inactive
    permitted_svn = (active_svn < inactive_svn) ? active_svn : inactive_svn;

    // Use the most restrictive value
    if (RequestType == UpdateKeyPermittedValue) {
        input_min_svn = permitted_svn;
    }
    const uint8_t boot_source = bootloader::Driver::get_boot_src();
    // FMC used
    if (boot_source == sys::bootloader::Driver::BootSourceFMC) {
        // FMC Security Version Number
        if (fill_fmc_sec_ver_num(fmc_svn) != Ccode::Success) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
        // find the minimum svn of active, inactive, fmc
        permitted_svn = (permitted_svn < fmc_svn) ? permitted_svn : fmc_svn;
        // Assign again since permitted_svn may change
        if (RequestType == UpdateKeyPermittedValue) {
            input_min_svn = permitted_svn;
        }

        if (input_min_svn > permitted_svn) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }
    else {
        if (input_min_svn > permitted_svn) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }

    // revoke rollback otp only if input_min_svn > min_svn_data
    // If input_min_svn <= min_svn_data, will not revoke rollback otp but also doesn't mean
    // failed
    if (input_min_svn > min_svn_data) {
        if (revoke_rollback_protection((uint32_t)input_min_svn) != Ccode::Success) {
            fill_error_packet_v2(Ccode::ErrorGeneral, Rcode::ErrorEfuseUpdateFailed, rx, tx);
            return;
        }
        logger::info(logger::Event::MctpNsmRevokeRollbackProtection,
                     logger::data_from_u32(static_cast<uint32_t>(input_min_svn)));

        Driver::mctp_send_cmd(Driver::CmdCode::RotStateInfoChange);
    }

    bitfield |= (1U << uint8_t(0));  // [0] – Automatic. EFUSE updated at completion of request.
    memcpy(&ntx.data, &bitfield, RespSize);
}

void mctp::Nsm::update_ap_min_sec_ver_num(const Packet&          rx,
                                          Packet&                tx,
                                          const UpdateMinSvnReq& update_struct)
{
    constexpr uint8_t RespSize = 4;

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx                 = NsmPktResp::from(tx);
    tx.priv.packet_length     = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code       = Ccode::Success;
    ntx.data_size             = RespSize;
    uint32_t      bitfield    = 0;
    const uint8_t RequestType = update_struct.request_type;

    uint32_t active_svn    = 0;
    uint16_t input_min_svn = 0;
    uint16_t permitted_svn = 0;
    uint32_t min_svn_data  = 0;

    auto can_revoke = can_revoke_ap_otp();

    if (can_revoke != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Security Version Number
    if (fill_ap_sec_ver_num(active_svn) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Minimum Security Version Number, MIN_SVN stored in EFUSE
    if (fill_ap_fuse_mini_sec_ver_num(min_svn_data) != Ccode::Success) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    if (RequestType == UpdateKeySpecifiedValue) {
        // Requested Minimum Security Version Number 14
        memcpy(&input_min_svn, &update_struct.min_svn, sizeof(input_min_svn));
    }

    permitted_svn = active_svn;

    // Use the most restrictive value
    if (RequestType == UpdateKeyPermittedValue) {
        input_min_svn = permitted_svn;
    }

    if (input_min_svn > permitted_svn) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // revoke rollback otp only if input_min_svn > min_svn_data
    // If input_min_svn <= min_svn_data, will not revoke rollback otp but also doesn't mean
    // failed
    if (input_min_svn > min_svn_data) {
        if (revoke_ap_rollback_protection((uint32_t)input_min_svn) != Ccode::Success) {
            fill_error_packet_v2(Ccode::ErrorGeneral, Rcode::ErrorEfuseUpdateFailed, rx, tx);
            return;
        }
        const uint32_t
            comp_info = static_cast<uint32_t>(update_struct.fw_comp_info.component_index) << 16
                      | static_cast<uint32_t>(update_struct.fw_comp_info.component_id);
        logger::info(
            logger::Event::MctpNsmRevokeApRollbackProtection,
            logger::data_from_two_u32(static_cast<uint32_t>(input_min_svn), comp_info));

        Driver::mctp_send_cmd(Driver::CmdCode::RotStateInfoChange);
    }

    bitfield |= (1U << uint8_t(0));  // [0] – Automatic. EFUSE updated at completion of request.
    memcpy(&ntx.data, &bitfield, RespSize);
}

void mctp::Nsm::on_update_min_sec_ver_num(const Packet& rx, Packet& tx)
{
    auto&             nrx            = NsmPktReq::from(rx);
    uint8_t           addition_size  = 2;
    constexpr uint8_t MinRequestSize = 14;

    if (rx.priv.packet_length < sizeof(Header) + HeaderRequestSize) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto input_data_size = rx.priv.packet_length - sizeof(Header) - HeaderRequestSize;
    if (input_data_size > static_cast<uint8_t>(MinRequestSize + addition_size)) {
        input_data_size = MinRequestSize + addition_size;
    }

    UpdateMinSvnReq update_min_svn_req{};
    // Read data
    memcpy(&update_min_svn_req, &nrx.data, input_data_size);

    if (is_irreversible_ctrl_enabled()) {
        // disable once this command (update min_svn) was request
        disable_irreversible_ctrl();
        if (update_min_svn_req.request_type == UpdateKeySpecifiedValue
            || update_min_svn_req.request_type == UpdateKeyPermittedValue) {
            addition_size = (update_min_svn_req.request_type == UpdateKeySpecifiedValue) ? 2
                                                                                         : 0;
            // check input length
            if (!is_input_length_valid(rx, MinRequestSize + addition_size)) {
                fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
                return;
            }
            // check nonce bytes
            if (!is_nonce_match(update_min_svn_req.nonce)) {
                fill_error_packet_v2(Ccode::ErrorGeneral, Rcode::ErrorNonceMismatch, rx, tx);
                return;
            }
            // check comp info(comp class, comp id, comp class idx)
            if (is_fw_comp_id_valid(update_min_svn_req.fw_comp_info, McuComponentId)) {
                update_min_sec_ver_num(rx, tx, update_min_svn_req);
                return;
            }
            if constexpr (pldm::ApNum > 0) {
                for (uint8_t i = 0; i < pldm::ApNum; ++i) {
                    if (is_fw_comp_id_valid(update_min_svn_req.fw_comp_info,
                                            pldm::AllApComponentId.at(i))) {
                        update_ap_min_sec_ver_num(rx, tx, update_min_svn_req);
                        return;
                    }
                }
            }
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        }
        // Undefined request type
        else {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }
    else {
        fill_error_packet_v2(Ccode::ErrorGeneral, Rcode::ErrorIrreversibleConfDisable, rx, tx);
    }
}

void mctp::Nsm::on_query_fw_comp_id(const Packet& rx, Packet& tx)
{
    // onQueryFwCompId Response data 6 bytes
    // - 1 byte: Component Count 0
    // - 2 byte: Component Classification 1 ~ 2
    // - 2 byte: Component Identifier 3 ~ 4
    // - 1 byte: Component Classification Index 5

    constexpr uint8_t ComponentCount = 1 + pldm::ApNum;
    constexpr uint8_t RespSize       = 1 + 5 * ComponentCount;
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;

    std::array<FwCompInfo, ComponentCount> components{{}};
    components.at(0) = {NvMctpFwComponentClass, McuComponentId, 0};
    if constexpr (pldm::ApNum > 0) {
        for (uint8_t i = 0; i < pldm::ApNum; ++i) {
            components.at(1 + i) = {NvMctpFwComponentClass, pldm::AllApComponentId.at(i), 0};
        }
    }
    FwCompIds<ComponentCount> fw_comp_ids{components};
    memcpy(&ntx.data, &fw_comp_ids, sizeof(fw_comp_ids));

    return;
}

void mctp::Nsm::on_set_rot_property(const Packet& rx, Packet& tx)
{
    // Component Info : 5 bytes
    // Property : 1 byte
    constexpr uint8_t RequestSize = 6;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto&             nrx = NsmPktReq::from(rx);
    SetRotPropertyReq set_rot_property_req{};
    memcpy(&set_rot_property_req, &nrx.data, sizeof(SetRotPropertyReq));

    // MCU & CPLD do not support any property
    if (set_rot_property_req.property == NsmSetRotPropertyRequest::SetRedundancyPolicy
        || set_rot_property_req.property == NsmSetRotPropertyRequest::SetInbandUpdatePolicy
        || set_rot_property_req.property == NsmSetRotPropertyRequest::SetApSkuId
        || set_rot_property_req.property == NsmSetRotPropertyRequest::SetGlobalFailoverPolicy) {
        fill_error_packet_v2(
            Ccode::ErrorUnsupportedArgument, Rcode::PropertyNotSupported, rx, tx);
        return;
    }

    fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
    return;
}

void mctp::Nsm::query_image_copy_progress(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RespSize = 2;
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;

    QueryImageCopyProgressResp query_image_copy_progress_resp{};

    nv::flash::ProgressPercent progress{};
    auto                       flash_status = flash::Flash::background_copy_query(progress);

    if (flash_status == flash::Status::BackgroundCopyIdle) {
        query_image_copy_progress_resp.status   = NsmImageCopyStatus::NotTriggered;
        query_image_copy_progress_resp.progress = 0;
    }
    else if (flash_status == flash::Status::BackgroundCopyFailed) {
        query_image_copy_progress_resp.status   = NsmImageCopyStatus::UndefinedFailed;
        query_image_copy_progress_resp.progress = 0;
    }
    else if (flash_status == flash::Status::BackgroundCopyDone) {
        query_image_copy_progress_resp.status   = NsmImageCopyStatus::Completed;
        query_image_copy_progress_resp.progress = progress;
    }
    else if (flash_status == flash::Status::BackgroundCopyInprogress) {
        query_image_copy_progress_resp.status   = NsmImageCopyStatus::InProgress;
        query_image_copy_progress_resp.progress = progress;
    }
    else {
        // Cannot determine the status
        fill_error_packet(Ccode::ErrorNotReady, rx, tx);
        return;
    }

    memcpy(&ntx.data, &query_image_copy_progress_resp, sizeof(query_image_copy_progress_resp));
    return;
}

void mctp::Nsm::initiate_image_copy(const Packet&              rx,
                                    Packet&                    tx,
                                    const ImageCopyControlReq& image_copy_control_req)
{
    // request type (1 byte) + component count (1 byte)
    if (!is_input_length_valid(rx, sizeof(ImageCopyControlReq))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // Only support MCU for now
    auto max_component_count = 1;
    if (image_copy_control_req.component_count != max_component_count) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Check full request size
    if (!is_input_length_valid(rx,
                               sizeof(ImageCopyControlReq)
                                   + image_copy_control_req.component_count
                                         * sizeof(FwCompInfo))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // input check
    if (!is_fw_comp_id_valid(image_copy_control_req.fw_comp_info[0], McuComponentId)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = 0;

    Ccode      completion_code = Ccode::Success;
    Rcode      reason_code     = Rcode::Null;
    const bool can_initiate    = can_initiate_image_copy(completion_code, reason_code);
    if (!can_initiate) {
        fill_error_packet_v2(completion_code, reason_code, rx, tx);
        return;
    }
    else {
        /* trigger BG */
        pldm::Task::pldm_bg_start();
    }
    return;
}

void mctp::Nsm::on_image_copy_control(const Packet& rx, Packet& tx)
{
    // Request Type : 1 byte
    // Component Count : 1 byte
    // Following repeat "Component Count" times:
    // Component Classification : 2 bytes
    // Component Identifier : 2 bytes
    // Component Classification Index : 1 byte

    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& nrx                    = NsmPktReq::from(rx);
    auto& image_copy_control_req = *std::bit_cast<ImageCopyControlReq*>(&nrx.data[0]);

    if (image_copy_control_req.request == NsmImageCopyControlRequest::QueryImageCopyProgress) {
        query_image_copy_progress(rx, tx);
        return;
    }
    else if (image_copy_control_req.request == NsmImageCopyControlRequest::InitiateImageCopy) {
        initiate_image_copy(rx, tx, image_copy_control_req);
        return;
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
}

// Build error response with completion code only (NsmPktResp).
void mctp::Nsm::fill_error_packet(Ccode code, const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    ntx.completion_code   = code;
    ntx.data_size         = 0;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

// Build error response with completion code + reason code (NsmPktRespV2).
void mctp::Nsm::fill_error_packet_v2(Ccode         completion_code,
                                     Rcode         reason_code,
                                     const Packet& rx,
                                     Packet&       tx) const
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktRespV2::from(tx);
    ntx.completion_code   = completion_code;
    ntx.reason_code       = reason_code;
    tx.priv.packet_length = sizeof(Header) + HeaderReasonResponseSize;
}

void mctp::Nsm::fill_packet_header(const Packet& rx, Packet& tx) const
{
    tx.hdr.rsvd    = 0;
    tx.hdr.hdr_ver = 1;
    tx.hdr.dst_eid = rx.hdr.src_eid;
    tx.hdr.src_eid = rx.hdr.dst_eid;
    tx.hdr.som     = 1;
    tx.hdr.eom     = 1;
    tx.hdr.pkt_seq = 0;

    // for response packet
    auto& vdr        = NsmPktResp::from(tx);
    tx.hdr.tag_owner = (vdr.rq) ? 1 : 0;
    tx.hdr.msg_tag   = rx.hdr.msg_tag;

    tx.priv                  = {};
    tx.priv.packet_interface = rx.priv.packet_interface;
    tx.priv.packet_length    = sizeof(Header) + HeaderResponseSize;

    _ctl.update_eid(tx, rx.priv.packet_interface);
}

void mctp::Nsm::fill_nsm_msg_header(const Packet& rx, Packet& tx) const
{
    auto& ntx = NsmPktResp::from(tx);
    auto& nrx = NsmPktReq::from(rx);

    ntx.msg_type = MsgType::VendorPci;

    ntx.pci_vendor_id = nrx.pci_vendor_id;
    ntx.instance_id   = nrx.instance_id;
    ntx.rsvd0         = 0;
    ntx.d             = 0;
    ntx.rq            = 0;  // 0 for response messages.
    ntx.ocp_version   = nrx.ocp_version;
    ntx.ocp_type      = nrx.ocp_type;
    ntx.ocp           = nrx.ocp;
}

void mctp::Nsm::fill_error_packet_aggr(Ccode code, const Packet& rx, Packet& tx) const
{
    fill_packet_header_aggr(rx, tx);
    fill_nsm_msg_header_aggr(rx, tx);
    auto& ntx             = NsmPktRespAggr::from(tx);
    ntx.completion_code   = code;
    ntx.telemetry_count   = 0;
    tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize;
}

void mctp::Nsm::fill_packet_header_aggr(const Packet& rx, Packet& tx) const
{
    tx.hdr.rsvd    = 0;
    tx.hdr.hdr_ver = 1;
    tx.hdr.dst_eid = rx.hdr.src_eid;
    tx.hdr.src_eid = rx.hdr.dst_eid;
    tx.hdr.som     = 1;
    tx.hdr.eom     = 1;
    tx.hdr.pkt_seq = 0;

    // for response packet
    auto& vdr        = NsmPktRespAggr::from(tx);
    tx.hdr.tag_owner = (vdr.rq) ? 1 : 0;
    tx.hdr.msg_tag   = rx.hdr.msg_tag;

    tx.priv                  = {};
    tx.priv.packet_interface = rx.priv.packet_interface;
    tx.priv.packet_length    = sizeof(Header) + AggregateHeaderResponseSize;

    _ctl.update_eid(tx, rx.priv.packet_interface);
}

void mctp::Nsm::fill_nsm_msg_header_aggr(const Packet& rx, Packet& tx) const
{
    auto& ntx = NsmPktRespAggr::from(tx);
    auto& nrx = NsmPktReq::from(rx);

    ntx.msg_type = MsgType::VendorPci;

    ntx.pci_vendor_id = nrx.pci_vendor_id;
    ntx.instance_id   = nrx.instance_id;
    ntx.rsvd0         = 0;
    ntx.d             = 0;
    ntx.rq            = 0;  // 0 for response messages.
    ntx.ocp_version   = 1;
    ntx.ocp_type      = 1;
    ntx.ocp           = 1;
}

void mctp::Nsm::fill_event_msg(const EventLog& event_log, Packet& tx) const
{
    auto& ntx = NsmEventMsg::from(tx);
    auto& nrx = NsmPktReq::from(nsm_event_clients);

    ntx.msg_type = MsgType::VendorPci;

    ntx.pci_vendor_id = nrx.pci_vendor_id;
    ntx.instance_id   = nrx.instance_id;  // TODO - Decide if needed to define a new instance id
    ntx.rsvd0         = 0;
    ntx.d             = 1;  // 1 for asychronous notifications(events)
    ntx.rq            = 1;  // 1 for event messages.
    ntx.ocp_version   = 1;
    ntx.ocp_type      = 1;
    ntx.ocp           = 1;

    ntx.nv_msg_type = event_log.nv_msg_type;

    ntx.rsvd1         = 0;
    ntx.ackr          = 0;  // 0 for ack msg not required
    ntx.event_version = event_log.event_version;

    ntx.event_id    = event_log.event_id;
    ntx.event_class = event_log.event_class;
    ntx.event_state = event_log.event_state;
    ntx.data_size   = event_log.data_size;
    // TODO - Temporarily the event don't have data

    tx.hdr.rsvd      = 0;
    tx.hdr.hdr_ver   = 1;
    tx.hdr.dst_eid   = event_subscription.endpoint_id;
    tx.hdr.src_eid   = nsm_event_clients.hdr.dst_eid;
    tx.hdr.msg_tag   = nsm_event_clients.hdr.msg_tag;
    tx.hdr.tag_owner = 1;
    tx.hdr.som       = 1;
    tx.hdr.eom       = 1;
    tx.hdr.pkt_seq   = 0;

    tx.priv                  = {};
    tx.priv.packet_interface = nsm_event_clients.priv.packet_interface;
    tx.priv.packet_length    = sizeof(Header) + HeaderEventMsgSize;
}

void mctp::Nsm::on_dcd_get_gpio(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = sizeof(Type0GetGpioReq);

    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& nrx = NsmPktReqV2::from(rx);
    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    Type0GetGpioReq gpio_req{};
    memcpy(&gpio_req, &nrx.data[0], sizeof(Type0GetGpioReq));
    const uint16_t offset = gpio_req.offset;
    uint16_t       length = gpio_req.length;

    nv::info("DCD Get GPIO request: offset = %d, length = %d\n", offset, length);

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto&         ntx = NsmPktResp::from(tx);
    Type0GpioResp gpio_resp{};

    // Check if offset and length exceed the total number of GPIOs
    if ((offset + length) > nv::ipc::GpioNum) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Special case: discover all supported GPIOs
    if (offset == 0 && length == 0) {
        length = nv::ipc::GpioNum;
    }

    // Set response header
    gpio_resp.offset = offset;
    gpio_resp.length = length;

    // Calculate required byte length
    const uint16_t byte_length = (length + 7) / 8;

    // Clear GPIO response data
    memset(gpio_resp.gpio.data(), 0, gpio_resp.gpio.size());

    // Check if GPIO spoofing error injection is active
    const bool gpio_spoofing_active = is_gpio_spoofing_activate();

    // Virtual and physical GPIOs use the same spoofing path (get_gpi_spoofing_value) when EI
    // on. Read GPIOs directly starting from offset
    for (uint16_t i = 0; i < length; i++) {
        const uint16_t gpio_index = offset + i;

        // Get GPIO configuration for this index
        const auto&              gpio_config = nv::ipc::GpioSetup.at(gpio_index);
        const nv::gpio::GpioPort port        = std::get<0>(gpio_config);
        const nv::gpio::GpioPin  pin         = std::get<1>(gpio_config);

        // Read GPIO value
        uint8_t pin_value = 0;
        if ((gpio_spoofing_active && get_gpi_spoofing_value(gpio_index, pin_value))
            || (nv::gpio::Driver::read_virtual_physical_gpio(port, pin, pin_value)
                == nv::gpio::Status::Ok)) {
            // Calculate position in response array
            const uint16_t byte_index = i / 8;
            const uint16_t bit_pos    = i % 8;

            // Set corresponding bit if GPIO is high
            if (pin_value) {
                gpio_resp.gpio.at(byte_index) |= (1U << bit_pos);
            }
        }
    }

    nv::info("DCD Get GPIO response: offset = %d, length = %d, gpio[x] = ", offset, length);
    for (uint16_t i = 0; i < byte_length; i++) {
        nv::info("%x ", gpio_resp.gpio.at(i));
    }
    nv::info("\n");

    // Set response size
    ntx.data_size = byte_length + 4;  // gpio array length + offset(2) + length(2)

    tx.priv.packet_length = static_cast<uint16_t>(sizeof(Header) + HeaderResponseSize
                                                  + ntx.data_size);
    ntx.completion_code   = Ccode::Success;

    memcpy(&ntx.data[0], &gpio_resp, sizeof(Type0GpioResp));
}

void mctp::Nsm::on_dcd_set_gpio(const Packet& rx, Packet& tx)
{
    auto&           nrx = NsmPktReqV2::from(rx);
    Type0SetGpioReq gpio_req{};
    memcpy(&gpio_req, &nrx.data[0], sizeof(Type0SetGpioReq));
    const uint16_t offset = gpio_req.offset;
    const uint16_t length = gpio_req.length;

    // Calculate required byte length
    const uint16_t byte_length = (length + 7) / 8;

    // Calculate request size: offset(2) + length(2) + gpio array bytes
    const uint16_t RequestSize = 4 + byte_length;

    if (RequestSize > UINT8_MAX
        || !is_input_length_valid(rx, static_cast<uint8_t>(RequestSize))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    if (nrx.ocp_version != 2) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto&         ntx = NsmPktResp::from(tx);
    Type0GpioResp gpio_resp{};

    // Check if offset and length exceed the total number of GPIOs
    if ((offset + length) > nv::ipc::GpioNum) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Set response header
    gpio_resp.offset = offset;
    gpio_resp.length = length;

    nv::info("DCD Set GPIO request: offset = %d, length = %d, gpio[x] = ", offset, length);
    for (uint16_t i = 0; i < byte_length; i++) {
        nv::info("%x ", gpio_req.gpio.at(i));
    }
    nv::info("\n");

    // Clear GPIO response data
    memset(gpio_resp.gpio.data(), 0, gpio_resp.gpio.size());

    // Set GPIOs directly starting from offset
    for (uint16_t i = 0; i < length; i++) {
        // Calculate position in request array
        const uint16_t byte_index = i / 8;
        const uint16_t bit_pos    = i % 8;

        // Get GPIO index
        const uint16_t gpio_index = offset + i;

        // Get GPIO configuration for this index
        const auto&              gpio_config = nv::ipc::GpioSetup.at(gpio_index);
        const nv::gpio::GpioPort port        = std::get<0>(gpio_config);
        const nv::gpio::GpioPin  pin         = std::get<1>(gpio_config);

        // Get requested state for this GPIO
        const bool requested_pin_state = (gpio_req.gpio.at(byte_index) & (1U << bit_pos)) != 0;

        // Write the GPIO value, invalid GPIO Port/Pin will make it fail
        if (nv::gpio::Driver::write(port, pin, requested_pin_state ? 1U : 0U)
            != nv::gpio::Status::Ok) {
            continue;
        }

        // Read back the GPIO value to confirm change
        uint8_t pin_value = 0;
        if (nv::gpio::Driver::read_virtual_physical_gpio(port, pin, pin_value)
            == nv::gpio::Status::Ok) {
            // Set corresponding bit if GPIO is high
            if (pin_value) {
                gpio_resp.gpio.at(byte_index) |= (1U << bit_pos);
            }
        }
    }

    nv::info("DCD Set GPIO response: offset = %d, length = %d, gpio[x] = ", offset, length);
    for (uint16_t i = 0; i < byte_length; i++) {
        nv::info("%x ", gpio_resp.gpio.at(i));
    }
    nv::info("\n");

    // Set response size
    ntx.data_size = byte_length + 4;  // gpio array length + offset(2) + length(2)

    tx.priv.packet_length = static_cast<uint16_t>(sizeof(Header) + HeaderResponseSize
                                                  + ntx.data_size);
    ntx.completion_code   = Ccode::Success;

    memcpy(&ntx.data[0], &gpio_resp, sizeof(Type0GpioResp));
}

bool mctp::Nsm::cache_event_msg(Packet& eventMsg)
{
    // Iterate through all cache slots to find the first available slot
    for (auto& event_cache_slot : event_cache) {
        if (event_cache_slot.eventState == NsmEventState::available) {
            // Copy the event message to the available cache slot
            event_cache_slot.eventMsg = eventMsg;

            // Mark the slot as pending (no longer available)
            event_cache_slot.eventState = NsmEventState::pending;

            return true;  // Successfully cached the event message
        }
    }

    // No available slot found
    return false;
}
bool mctp::Nsm::is_gpio_spoofing_activate()
{
    return type5_data.isErrorInjectionModeEnabled()
        && type5_data.isErrorTypeEnabled(static_cast<uint8_t>(ErrorInjectionID::GpioSpoofing));
}

bool mctp::Nsm::get_gpio_default_value(uint16_t gpio_index, uint8_t& pin_value)
{
    // Check if gpio_index is valid
    if (gpio_index >= nv::ipc::GpioSetup.size()) {
        return false;
    }

    // Get port and pin from GpioSetup array
    const auto& gpio_entry = nv::ipc::GpioSetup.at(gpio_index);
    const auto  port       = std::get<0>(gpio_entry);
    const auto  pin        = std::get<1>(gpio_entry);

    // Search through IoxConfigs to find the matching PinConfig
    for (const auto& iox_config : nv::ipc::IoxConfigs) {
        for (const auto& pin_config : iox_config.pinConfig) {
            if (pin_config.port == port && pin_config.pin == pin) {
                // Set pin_value based on GpioState and return true
                pin_value = (pin_config.val == nv::gpio::GpioState::High) ? 1 : 0;
                return true;
            }
        }
    }

    // Default to 0 if not found, but still return true (valid gpio_index)
    pin_value = 0;
    return true;
}

bool mctp::Nsm::get_gpi_spoofing_value(uint16_t gpio_index, uint8_t& pin_value)
{
    // Check if gpio_index is valid
    if (gpio_index >= nv::ipc::GpioSetup.size()) {
        return false;
    }

    // Get port and pin from GpioSetup array
    const auto& gpio_entry = nv::ipc::GpioSetup.at(gpio_index);
    const auto  port       = std::get<0>(gpio_entry);
    const auto  pin        = std::get<1>(gpio_entry);

    // Virtual GPIO has no hardware direction; treat as input for spoofing eligibility.
    if (port != nv::iox::vrPort) {
        nv::gpio::Direction dir = gpio::Direction::Input;
        if (nv::gpio::Driver::getDirection(port, pin, dir) != nv::gpio::Status::Ok) {
            return false;
        }

        if (dir != nv::gpio::Direction::Input) {
            return false;
        }
    }

    // Search through GPIO spoofing entries
    const auto& gpio_spoofing = type5_data.gpioSpoofingResp;
    const auto  num_entries   = gpio_spoofing.gpio_spoofing_header.ei_gpio_number;

    for (uint16_t i = 0; i < num_entries && i < MaxGPIOSpoofingEntries; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto& entry = gpio_spoofing.gpio_ei_entries[i];

        // Check if this entry matches the gpio_index and is activated
        if (entry.gpioIndex == gpio_index && entry.activated == 1) {
            // Set the spoofed GPIO value and return true (spoofing is active)
            pin_value = entry.polarity;
            return true;
        }
    }

    // No matching spoofing entry found, get default value and return true (isolating physical
    // gpio state from spoofing)
    get_gpio_default_value(gpio_index, pin_value);
    return true;
}

void mctp::Nsm::notifyIoxSpoofingState(bool spoofingActive)
{
    if constexpr (nv::ipc::EnableIoxEmulation) {
        // Send to IOX task
        nv::iox::Task::send_gpio_spoofing_update(spoofingActive, type5_data.gpioSpoofingResp);
    }
}
