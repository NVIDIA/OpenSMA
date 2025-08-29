/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
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

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>

#include "corepdk/platforms/mcxn236/pldm-fd/src/pldm_wrap.h"

#include "nv/bootloader.h"
#include "nv/flash/flash.h"
#include "nv/fw_parser/fw_parser.h"
#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/interface.h"
#include "nv/nv.h"
#include "sys/flash/flash_config.h"

using namespace nv;
using namespace mctp;
using namespace std::chrono_literals;
using namespace sys::flash::config;

extern "C" void
ada_populate_stamp(uint8_t minor, uint16_t patch, uint16_t build, uint32_t* stamp);

Ccode mctp::Nsm::can_revoke_otp()
{
    flash::Data state{};

    // Check PdsUpdateState
    if (flash::Flash::get_data(flash::Key::PdsUpdateState, state) != flash::Status::Ok
        || state != static_cast<flash::Data>(bootloader::Driver::State::Idle)) {
        return Ccode::ErrorPldmProcessing;
    }

    // Check PdsBootableSlot0 and PdsBootableSlot1
    for (const auto& key : {flash::Key::PdsBootableSlot0, flash::Key::PdsBootableSlot1}) {
        if (flash::Flash::get_data(key, state) != flash::Status::Ok || state == 0) {
            return Ccode::ErrorGeneral;
        }
    }

    // If background copy enabled, complete background copy
    nv::flash::ProgressPercent progress{};
    if (flash::Flash::background_copy_query(progress)
        == flash::Status::BackgroundCopyInprogress) {
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

uint8_t mctp::Nsm::get_actvie_slot()
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

uint8_t mctp::Nsm::get_inactive_fw_state()
{
    flash::Data update_state{};
    auto        flash_status = flash::Flash::get_data(flash::Key::PdsUpdateState, update_state);
    if (flash_status != flash::Status::Ok) {
        update_state = static_cast<flash::Data>(bootloader::Driver::State::Invalid);
    }

    switch (update_state) {
        case static_cast<flash::Data>(bootloader::Driver::State::InProgress):
            return common::WriteInProgress;
        case static_cast<flash::Data>(bootloader::Driver::State::Complete):
            return common::PendingActivation;
        case static_cast<flash::Data>(bootloader::Driver::State::Stage): return common::Staged;
        default                                                        : return common::Inactive;
    }
}

bool mctp::Nsm::is_active_slot(uint8_t slot)
{
    return slot == get_actvie_slot();
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
    const fw_parser::ParsingFwType parsing_type = (slot == Slot0Id)
                                                    ? fw_parser::ParsingFwType::Slot0
                                                    : fw_parser::ParsingFwType::Slot1;

    auto image_key_version = fw_parser::get_image_signing_key_version(parsing_type);

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

Ccode mctp::Nsm::fill_fmc_key_index(uint16_t& key_index)
{
    const fw_parser::ParsingFwType parsing_type = fw_parser::ParsingFwType::Fmc;

    auto image_key_version = fw_parser::get_image_signing_key_version(parsing_type);

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
    auto     flash_status    = nv::flash::Flash::read_key_revoke(key_index_unary);
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
    auto status = nv::flash::Flash::write_key_revoke(key_permission, 1s);
    if (status != flash::Status::Ok) {
        return Ccode::ErrorEfuseUpdateFailed;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_sec_ver_num(uint8_t slot, uint32_t& rollback_protection)
{
    const fw_parser::ParsingFwType parsing_type = (slot == Slot0Id)
                                                    ? fw_parser::ParsingFwType::Slot0
                                                    : fw_parser::ParsingFwType::Slot1;
    // Read secure fw version from firmware
    auto security_version = fw_parser::get_security_version(parsing_type);

    if (security_version.has_value()) {
        rollback_protection = (*security_version);
    }
    else {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::fill_fmc_sec_ver_num(uint32_t& rollback_protection)
{
    const fw_parser::ParsingFwType parsing_type = fw_parser::ParsingFwType::Fmc;
    // Read secure fw version from firmware
    auto security_version = fw_parser::get_security_version(parsing_type);

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
    auto status = nv::flash::Flash::read_secure_fw_version(rollback_protection);
    if (status != flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }

    return Ccode::Success;
}

Ccode mctp::Nsm::revoke_rollback_protection(uint32_t rollback_protection)
{
    auto status = nv::flash::Flash::write_secure_fw_version(rollback_protection, 1s);
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
    data    = get_actvie_slot();
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

bool mctp::Nsm::is_fw_comp_id_valid(const FwCompInfo& input_fw_comp_info)
{
    if (input_fw_comp_info.component_class != NvMctpFwComponentClass
        || input_fw_comp_info.component_id != McuComponentId
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
}

void mctp::Nsm::on_dcd_get_sup_nv_meg_type(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + NvMctpSupportedNum;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = NvMctpSupportedNum;

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
    tx.priv.packet_length       = sizeof(Header) + HeaderResponseSize + NvMctpSupportedNum;
    ntx.completion_code         = Ccode::Success;
    ntx.data_size               = NvMctpSupportedNum;

    if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::DeviceCapabilityDiscovery)) {
        memcpy(&ntx.data, SupType0Code.data(), SupType0Code.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::Diagnostics)) {
        if constexpr (nv::ipc::Enable_Nsm_type4) {
            memcpy(&ntx.data, type4_data.suppCmdCode.data(), type4_data.suppCmdCode.size());
        }
        else {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        }
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::Firmware)) {
        memcpy(&ntx.data, SupType6Code.data(), SupType6Code.size());
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::PlatformEnviromentals)) {
        if constexpr (nv::ipc::Enable_Nsm_type3) {
            memcpy(&ntx.data, type3_data.suppCmdCode.data(), type3_data.suppCmdCode.size());
        }
        else {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        }
    }
    else if (NvMessageType == static_cast<uint8_t>(mctp::NsmMsgType::DeviceConfiguration)) {
        if constexpr (nv::ipc::Enable_Nsm_type5) {
            memcpy(&ntx.data, type5_data.suppCmdCode.data(), type5_data.suppCmdCode.size());
        }
        else {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        }
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
    tx.priv.packet_length       = sizeof(Header) + HeaderResponseSize + NvMctpEventSupportedNum;
    ntx.completion_code         = Ccode::Success;
    ntx.data_size               = NvMctpEventSupportedNum;

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
    tx.priv.packet_length       = sizeof(Header) + HeaderResponseSize + NvMctpEventSupportedNum;
    ntx.completion_code         = Ccode::Success;
    ntx.data_size               = NvMctpEventSupportedNum;

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
        for (size_t i = 0; i < NvMctpEventSupportedNum; ++i) {
            type0_event_enable_bitmask.at(i) &= SupType0Event.at(i);
        }
        // Allow log when event_srcs not available
        log_nvmsg_event_bitmask.at(static_cast<uint8_t>(msg_with_bitmask.nv_msg_type)) = false;
    }
    else if (msg_with_bitmask.nv_msg_type == NsmMsgType::Firmware) {
        type6_event_enable_bitmask = msg_with_bitmask.bitmask;
        // & operation to avoid set unsupported event
        for (size_t i = 0; i < NvMctpEventSupportedNum; ++i) {
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
        && event_subscription.setting == NsmGlobalEventSetting::EventDisable) {
        // TODO - Check if the understanding of the following is correct
        // Resubscribing with this setting will cause the receiver to be unsubscribed from
        // further events.
        // Case : Double subscribe with disable setting -> unsubscribe
        event_subscription.setting = NsmGlobalEventSetting::EventNotSubscribe;
    }
    else if (input_event_subscription.setting == NsmGlobalEventSetting::EventDisable
             || input_event_subscription.setting == NsmGlobalEventSetting::EventPolling
             || input_event_subscription.setting == NsmGlobalEventSetting::EventPush) {
        event_subscription.setting = input_event_subscription.setting;
        // TODO - Check if it allowed to use 0x00 or 0xFF to be endpoint ID
        event_subscription.endpoint_id = input_event_subscription.endpoint_id;
        nsm_rx                         = rx;
        // Allow log when event_subscription.setting not PUSH
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

    if (event_subscription.setting != NsmGlobalEventSetting::EventNotSubscribe) {
        ntx.data[0] = event_subscription.endpoint_id;
    }
    else {
        // TODO - Decide which completion code to response
        // Error - No receiver is currently subscribed
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
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

bool mctp::Nsm::process_device_capability_discovery(const Packet& rx, Packet& tx)
{
    using cmd = mctp::NsmDcdCmdCode;
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    ntx.set_dcd_code(nrx.get_dcd_code());

    switch (nrx.get_dcd_code()) {
        case cmd::DcdPing                     : on_dcd_ping(rx, tx); break;
        case cmd::DcdGetSupNvMsgTypes         : on_dcd_get_sup_nv_meg_type(rx, tx); break;
        case cmd::DcdGetSupCmdCodes           : on_dcd_get_sup_cmd_codes(rx, tx); break;
        case cmd::DcdGetSupEventSrcs          : on_dcd_get_sup_event_srcs(rx, tx); break;
        case cmd::DcdGetCurrentEventSrcs      : on_dcd_get_current_event_srcs(rx, tx); break;
        case cmd::DcdSetCurrentEventSrcs      : on_dcd_set_current_event_srcs(rx, tx); break;
        case cmd::DcdSetEventSubscription     : on_dcd_set_event_subscription(rx, tx); break;
        case cmd::DcdGetEventSubscription     : on_dcd_get_event_subscription(rx, tx); break;
        case cmd::DcdQueryDeviceIdentification: on_dcd_query_device_id(rx, tx); break;
        case cmd::DcdGetGpio                  : on_dcd_get_gpio(rx, tx); break;
        case cmd::DcdSetGpio                  : on_dcd_set_gpio(rx, tx); break;
        default                               : fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx); return false;
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

    switch (nrx.get_fw_code()) {
        case cmd::GetRotStateInfo   : on_get_rot_state_info(rx, tx); break;
        case cmd::IrreversibleConf  : on_ctrl_irreversible_conf(rx, tx); break;
        case cmd::QueryCodeAuthKey  : on_query_auth_key(rx, tx); break;
        case cmd::UpdateCodeAuthKey : on_update_auth_key(rx, tx); break;
        case cmd::QuerySecVerNum    : on_query_sec_ver_num(rx, tx); break;
        case cmd::UpdateMinSecVerNum: on_update_min_sec_ver_num(rx, tx); break;
        case cmd::QueryFwCompId     : on_query_fw_comp_id(rx, tx); break;
        default                     : fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx); return false;
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

    switch (nrx.nv_msg_type) {
        case cmd::DeviceCapabilityDiscovery: process_device_capability_discovery(rx, tx); break;
        case cmd::Firmware                 : process_firmware(rx, tx); break;
        case cmd::Diagnostics:
            if constexpr (nv::ipc::Enable_Nsm_type4) {
                process_diagnostics(rx, tx);
            }
            else {
                fill_error_packet(Ccode::ErrorUnsupportedMsgType, rx, tx);
                return false;
            }
            break;
        case cmd::DeviceConfiguration:
            if constexpr (nv::ipc::Enable_Nsm_type5) {
                process_device_configuration(rx, tx);
            }
            else {
                fill_error_packet(Ccode::ErrorUnsupportedMsgType, rx, tx);
                return false;
            }
            break;
        case cmd::PlatformEnviromentals:
            if constexpr (nv::ipc::Enable_Nsm_type3) {
                process_platform_enviromentals(rx, tx);
            }
            else {
                fill_error_packet(Ccode::ErrorUnsupportedMsgType, rx, tx);
                return false;
            }
            break;
        default: fill_error_packet(Ccode::ErrorUnsupportedMsgType, rx, tx); return false;
    }
    return true;
}

void mctp::Nsm::get_rot_state_info(const Packet& rx, Packet& tx)
{
    uint8_t                               build_type           = 0;
    uint8_t                               active_slot          = 0;
    uint16_t                              major                = 0;
    uint16_t                              patch                = 0;
    uint16_t                              build                = 0;
    uint8_t                               minor                = 0;
    uint16_t                              key_index_slot0      = 0;
    uint16_t                              key_index_slot1      = 0;
    uint32_t                              svn_data             = 0;
    uint32_t                              min_svn_data         = 0;
    std::array<uint8_t, 8>                boot_status_code     = {0};
    Ccode                                 key_data_valid_slot0 = Ccode::Success;
    Ccode                                 key_data_valid_slot1 = Ccode::Success;
    uint8_t                               wp_state             = 0;
    std::array<char, NvMctpVersionLength> fw_version{};
    fill_packet_header_aggr(rx, tx);
    fill_nsm_msg_header_aggr(rx, tx);
    auto& ntx             = NsmPktRespAggr::from(tx);
    tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize
                          + sizeof(RespAggregate);
    ntx.completion_code = Ccode::Success;
    ntx.telemetry_count = TelemetryCount;

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

    // Tag 1, 2, 3, 13, 15, 16
    // Tag 5 (slot count : n) (The following tag repeat n times)
    // slot 0 : Tag 6, 7, 8, 9, 10, 4, 11, 12, 14
    // slot 1 : Tag 6, 7, 8, 9, 10, 4, 11, 12, 14

    key_data_valid_slot0 = fill_key_index(Slot0Id, key_index_slot0);
    key_data_valid_slot1 = fill_key_index(Slot1Id, key_index_slot1);
    active_slot          = get_actvie_slot();
    auto gpio_status     = nv::gpio::Status::Error;
    if (nv::ipc::GlobalWpPort != nv::gpio::InvalidGpioPort) {
        gpio_status = nv::gpio::Driver::read(
            nv::ipc::GlobalWpPort, nv::ipc::GlobalWpPin, wp_state);
    }
    // TAG 1,  0, BackgroundCopyEnabled
    ntx.resp_aggregate.tag_back_ground_copy_policy.tag     = TagBackgroundCopyPolicy;
    ntx.resp_aggregate.tag_back_ground_copy_policy.v       = 1;
    ntx.resp_aggregate.tag_back_ground_copy_policy.length  = TagBackgroundCopyPolicyLen;
    ntx.resp_aggregate.tag_back_ground_copy_policy.rsvd    = 0;
    ntx.resp_aggregate.tag_back_ground_copy_policy.b       = 0;
    ntx.resp_aggregate.tag_back_ground_copy_policy.data[0] = 1;  // enabled

    // TAG 2,  0, ActiveSlot
    ntx.resp_aggregate.tag_active_firmware_slot.tag     = TagActiveFirmwareSlot;
    ntx.resp_aggregate.tag_active_firmware_slot.v       = (active_slot != InvalidSlot) ? 1 : 0;
    ntx.resp_aggregate.tag_active_firmware_slot.length  = TagActiveFirmwareSlotLen;
    ntx.resp_aggregate.tag_active_firmware_slot.rsvd    = 0;
    ntx.resp_aggregate.tag_active_firmware_slot.b       = 0;
    ntx.resp_aggregate.tag_active_firmware_slot.data[0] = active_slot;

    // TAG 3,  0, ActiveKeySet
    ntx.resp_aggregate.tag_active_key_set.tag    = TagActiveKeySet;
    ntx.resp_aggregate.tag_active_key_set.v      = 1;
    ntx.resp_aggregate.tag_active_key_set.length = TagActiveKeySetLen;
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
    ntx.resp_aggregate.tag_min_security_ver_num.length = TagMinSecurityVerNumLen;
    ntx.resp_aggregate.tag_min_security_ver_num.rsvd   = 0;
    ntx.resp_aggregate.tag_min_security_ver_num.b      = 0;
    auto min_svn_data_short = static_cast<uint16_t>(min_svn_data & UINT16_MAX);
    memcpy(&ntx.resp_aggregate.tag_min_security_ver_num.data,
           &min_svn_data_short,
           sizeof(uint16_t));

    // TAG 15, 0, InbandUpdatePolicy
    ntx.resp_aggregate.tag_inband_update_policy.tag     = TagInbandUpdatePolicy;
    ntx.resp_aggregate.tag_inband_update_policy.v       = 1;
    ntx.resp_aggregate.tag_inband_update_policy.length  = TagInbandUpdatePolicyLen;
    ntx.resp_aggregate.tag_inband_update_policy.rsvd    = 0;
    ntx.resp_aggregate.tag_inband_update_policy.b       = 0;
    ntx.resp_aggregate.tag_inband_update_policy.data[0] = 0;  // disabled

    // TAG 16, 3, BootStatusCode
    ntx.resp_aggregate.tag_boot_status_code.tag    = TagBootStatusCode;
    ntx.resp_aggregate.tag_boot_status_code.v      = 1;
    ntx.resp_aggregate.tag_boot_status_code.length = TagBootStatusCodeLen;
    ntx.resp_aggregate.tag_boot_status_code.rsvd   = 0;
    ntx.resp_aggregate.tag_boot_status_code.b      = 0;

    fill_boot_status_code(boot_status_code);
    memcpy(&ntx.resp_aggregate.tag_boot_status_code.data,
           boot_status_code.data(),
           sizeof(boot_status_code));

    // TAG 5,  0, FirmwareSlotCount
    ntx.resp_aggregate.tag_firmware_slot_count.tag     = TagFirmwareSlotCount;
    ntx.resp_aggregate.tag_firmware_slot_count.v       = 1;
    ntx.resp_aggregate.tag_firmware_slot_count.length  = TagFirmwareSlotCountLen;
    ntx.resp_aggregate.tag_firmware_slot_count.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_slot_count.b       = 0;
    ntx.resp_aggregate.tag_firmware_slot_count.data[0] = 2;  // 2 slots

    // TAG 6,  0, FirmwareSlotID for slot 0
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.tag     = TagFirmwareSlotId;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.v       = 1;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.length  = TagFirmwareSlotIdLen;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.b       = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot0.data[0] = Slot0Id;

    // TAG 7,  5, Firmware version string for slot 0
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.tag    = TagFirmwareVerString;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.v      = 1;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.length = TagFirmwareVerStringLen;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.rsvd   = 0;
    ntx.resp_aggregate.tag_firmware_ver_string_slot0.b      = 0;

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
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.tag    = TagVerComparisonStamp;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.v      = 1;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.length = TagVerComparisonStampLen;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.rsvd   = 0;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot0.b      = 0;

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
    ntx.resp_aggregate.tag_build_type_slot0.length  = TagBuildTypeLen;
    ntx.resp_aggregate.tag_build_type_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_build_type_slot0.b       = 0;
    ntx.resp_aggregate.tag_build_type_slot0.data[0] = build_type;

    // TAG 10,  0, SigningType for slot 0
    ntx.resp_aggregate.tag_signing_type_slot0.tag = TagSigningType;
    ntx.resp_aggregate.tag_signing_type_slot0.v   = (key_data_valid_slot0 == Ccode::Success) ? 1
                                                                                             : 0;
    ntx.resp_aggregate.tag_signing_type_slot0.length  = TagSigningTypeLen;
    ntx.resp_aggregate.tag_signing_type_slot0.rsvd    = 0;
    ntx.resp_aggregate.tag_signing_type_slot0.b       = 0;
    ntx.resp_aggregate.tag_signing_type_slot0.data[0] = fill_signing_type(
        static_cast<uint8_t>(key_index_slot0 & UINT8_MAX));

    // TAG 4,   0, WriteProtectState for slot 0
    ntx.resp_aggregate.tag_write_protect_state_slot0.tag = TagWriteProtectState;
    ntx.resp_aggregate.tag_write_protect_state_slot0.v   = (gpio_status == nv::gpio::Status::Ok)
                                                             ? 1
                                                             : 0;
    ntx.resp_aggregate.tag_write_protect_state_slot0.length = TagWriteProtectStateLen;
    ntx.resp_aggregate.tag_write_protect_state_slot0.rsvd   = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot0.b      = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot0
        .data[0] = (wp_state == static_cast<uint8_t>(nv::gpio::GpioState::High)) ? 1 : 0;

    // TAG 11,  0, Firmware State for slot 0
    ntx.resp_aggregate.tag_firmware_state_slot0.tag     = TagFirmwareState;
    ntx.resp_aggregate.tag_firmware_state_slot0.v       = 1;
    ntx.resp_aggregate.tag_firmware_state_slot0.length  = TagFirmwareStateLen;
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
    ntx.resp_aggregate.tag_security_ver_num_slot0.length = TagSecurityVerNumLen;
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
    ntx.resp_aggregate.tag_signing_key_index_slot0.length = TagSigningKeyIndexLen;
    ntx.resp_aggregate.tag_signing_key_index_slot0.rsvd   = 0;
    ntx.resp_aggregate.tag_signing_key_index_slot0.b      = 0;
    memcpy(&ntx.resp_aggregate.tag_signing_key_index_slot0.data,
           &key_index_slot0,
           sizeof(uint16_t));

    // TAG 6,  0, FirmwareSlotID for slot 1
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.tag     = TagFirmwareSlotId;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.v       = 1;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.length  = TagFirmwareSlotIdLen;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.rsvd    = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.b       = 0;
    ntx.resp_aggregate.tag_firmware_slot_id_slot1.data[0] = Slot1Id;

    // TAG 7,  5, Firmware version string for slot 1
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.tag    = TagFirmwareVerString;
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.v      = 1;
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.length = TagFirmwareVerStringLen;
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.rsvd   = 0;
    ntx.resp_aggregate.tag_firmware_ver_string_slot1.b      = 0;

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
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.tag    = TagVerComparisonStamp;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.v      = 1;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.length = TagVerComparisonStampLen;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.rsvd   = 0;
    ntx.resp_aggregate.tag_ver_comparison_stamp_slot1.b      = 0;

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
    ntx.resp_aggregate.tag_build_type_slot1.length  = TagBuildTypeLen;
    ntx.resp_aggregate.tag_build_type_slot1.rsvd    = 0;
    ntx.resp_aggregate.tag_build_type_slot1.b       = 0;
    ntx.resp_aggregate.tag_build_type_slot1.data[0] = build_type;

    // TAG 10,  0, SigningType for slot 1
    ntx.resp_aggregate.tag_signing_type_slot1.tag = TagSigningType;
    ntx.resp_aggregate.tag_signing_type_slot1.v   = (key_data_valid_slot1 == Ccode::Success) ? 1
                                                                                             : 0;
    ntx.resp_aggregate.tag_signing_type_slot1.length  = TagSigningTypeLen;
    ntx.resp_aggregate.tag_signing_type_slot1.rsvd    = 0;
    ntx.resp_aggregate.tag_signing_type_slot1.b       = 0;
    ntx.resp_aggregate.tag_signing_type_slot1.data[0] = fill_signing_type(
        static_cast<uint8_t>(key_index_slot1 & UINT8_MAX));

    // TAG 4,   0, WriteProtectState for slot 1
    ntx.resp_aggregate.tag_write_protect_state_slot1.tag = TagWriteProtectState;
    ntx.resp_aggregate.tag_write_protect_state_slot1.v   = (gpio_status == nv::gpio::Status::Ok)
                                                             ? 1
                                                             : 0;
    ntx.resp_aggregate.tag_write_protect_state_slot1.length = TagWriteProtectStateLen;
    ntx.resp_aggregate.tag_write_protect_state_slot1.rsvd   = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot1.b      = 0;
    ntx.resp_aggregate.tag_write_protect_state_slot1
        .data[0] = (wp_state == static_cast<uint8_t>(nv::gpio::GpioState::High)) ? 1 : 0;

    // TAG 11,  0, Firmware State for slot 1
    ntx.resp_aggregate.tag_firmware_state_slot1.tag     = TagFirmwareState;
    ntx.resp_aggregate.tag_firmware_state_slot1.v       = 1;
    ntx.resp_aggregate.tag_firmware_state_slot1.length  = TagFirmwareStateLen;
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
    ntx.resp_aggregate.tag_security_ver_num_slot1.length = TagSecurityVerNumLen;
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
    ntx.resp_aggregate.tag_signing_key_index_slot1.length = TagSigningKeyIndexLen;
    ntx.resp_aggregate.tag_signing_key_index_slot1.rsvd   = 0;
    ntx.resp_aggregate.tag_signing_key_index_slot1.b      = 0;
    memcpy(&ntx.resp_aggregate.tag_signing_key_index_slot1.data,
           &key_index_slot1,
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
    if (!is_fw_comp_id_valid(input_fw_comp_info)) {
        fill_error_packet_aggr(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    get_rot_state_info(rx, tx);
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

    const uint8_t ActiveSlot     = get_actvie_slot();
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
    if (!is_fw_comp_id_valid(input_fw_comp_info)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    query_auth_key(rx, tx);
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

    const uint8_t ActiveSlot   = get_actvie_slot();
    const uint8_t InactiveSlot = (ActiveSlot == Slot0Id) ? Slot1Id : Slot0Id;

    // key_permission will be the form of "(1 << N) - 1"
    uint32_t active_key_permission    = 0;
    uint32_t inactive_key_permission  = 0;
    uint32_t efuse_key_permission     = 0;
    uint32_t fmc_key_permission       = 0;
    uint32_t permitted_key_permission = 0;
    // For input
    uint32_t input_key_permission = 0;

    auto can_revoke = can_revoke_otp();

    // bypassing the cases for PldmProcessActive and inactive slot and bg copy inprogress
    if (can_revoke != Ccode::Success) {
        fill_error_packet(can_revoke, rx, tx);
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
            fill_error_packet(revoke_ccode, rx, tx);
            return;
        }

        logger::info(logger::Event::MctpNsmRevokeKey,
                     logger::data_from_u32(input_key_permission));

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
                fill_error_packet(Ccode::ErrorNonceMismatch, rx, tx);
                return;
            }
            // check comp info(comp class, comp id, comp class idx)
            if (!is_fw_comp_id_valid(update_auth_key_req.fw_comp_info)) {
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return;
            }
            // Pass all check
            update_auth_key(rx, tx, update_auth_key_req);
        }
        // Undefined request type
        else {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }
    else {
        fill_error_packet(Ccode::ErrorIrreversibleConfDisable, rx, tx);
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

    const uint8_t ActiveSlot   = get_actvie_slot();
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
    if (!is_fw_comp_id_valid(input_fw_comp_info)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    query_sec_ver_num(rx, tx);
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

    const uint8_t ActiveSlot    = get_actvie_slot();
    const uint8_t InactiveSlot  = (ActiveSlot == 0) ? 1 : 0;
    uint32_t      active_svn    = 0;
    uint32_t      inactive_svn  = 0;
    uint16_t      input_min_svn = 0;
    uint16_t      permitted_svn = 0;
    uint32_t      min_svn_data  = 0;
    uint32_t      fmc_svn       = 0;

    auto can_revoke = can_revoke_otp();

    // bypassing the cases for PldmProcessActive and inactive slot and bg copy inprogress
    if (can_revoke != Ccode::Success) {
        fill_error_packet(can_revoke, rx, tx);
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
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
        logger::info(logger::Event::MctpNsmRevokeRollbackProtection,
                     logger::data_from_u32(static_cast<uint32_t>(input_min_svn)));

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
                fill_error_packet(Ccode::ErrorNonceMismatch, rx, tx);
                return;
            }
            // check comp info(comp class, comp id, comp class idx)
            if (!is_fw_comp_id_valid(update_min_svn_req.fw_comp_info)) {
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return;
            }
            // Pass all check
            update_min_sec_ver_num(rx, tx, update_min_svn_req);
        }
        // Undefined request type
        else {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
    }
    else {
        fill_error_packet(Ccode::ErrorIrreversibleConfDisable, rx, tx);
    }
}

void mctp::Nsm::on_query_fw_comp_id(const Packet& rx, Packet& tx)
{
    // onQueryFwCompId Response data 6 bytes
    // - 1 byte: Component Count 0
    // - 2 byte: Component Classification 1 ~ 2
    // - 2 byte: Component Identifier 3 ~ 4
    // - 1 byte: Component Classification Index 5

    constexpr uint8_t RespSize = 6;
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + RespSize;
    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = RespSize;

    constexpr uint8_t ComponentCount = 1;

    FwCompIds fw_comp_ids = FwCompIds<ComponentCount>{
        {NvMctpFwComponentClass, McuComponentId, 0}
    };

    memcpy(&ntx.data, &fw_comp_ids, sizeof(fw_comp_ids));

    return;
}

void mctp::Nsm::fill_error_packet(Ccode code, const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& ntx             = NsmPktResp::from(tx);
    ntx.completion_code   = code;
    ntx.data_size         = 0;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
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
    auto& nrx = NsmPktReq::from(nsm_rx);

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
    tx.hdr.src_eid   = nsm_rx.hdr.dst_eid;
    tx.hdr.msg_tag   = nsm_rx.hdr.msg_tag;
    tx.hdr.tag_owner = 1;
    tx.hdr.som       = 1;
    tx.hdr.eom       = 1;
    tx.hdr.pkt_seq   = 0;

    tx.priv                  = {};
    tx.priv.packet_interface = nsm_rx.priv.packet_interface;
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

    // Read GPIOs directly starting from offset
    for (uint16_t i = 0; i < length; i++) {
        const uint16_t gpio_index = offset + i;

        // Get GPIO configuration for this index
        const auto&              gpio_config = nv::ipc::GpioSetup.at(gpio_index);
        const nv::gpio::GpioPort port        = std::get<0>(gpio_config);
        const nv::gpio::GpioPin  pin         = std::get<1>(gpio_config);

        // Read GPIO value
        uint8_t pin_value = 0;
        if (nv::gpio::Driver::read(port, pin, pin_value) == nv::gpio::Status::Ok) {
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

        // Write the GPIO value
        nv::gpio::Driver::write(port, pin, requested_pin_state ? 1U : 0U);

        // Read back the GPIO value to confirm change
        uint8_t pin_value = 0;
        if (nv::gpio::Driver::read(port, pin, pin_value) == nv::gpio::Status::Ok) {
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
