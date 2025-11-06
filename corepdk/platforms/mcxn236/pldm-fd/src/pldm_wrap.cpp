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
#include "pldm_wrap.h"

#include <chrono>
#include <cstring>

#include "nv/flash/flash.h"
#include "nv/flash/task.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include "nv/mctp/composer.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"
#include "nv/pldm/task.h"
#include "nv/watchdog/boot.h"
#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/fw_parser/fw_parser_mcu.h"
#include "nv/ipchandler/ipchandler.h"
#include "sys/flash/flash_config.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/ap_operation/ap_operation.h"

using namespace nv::mctp;
using namespace std::chrono_literals;

namespace nv::spdm::crypto {

template<>
void send_authenticate_mcu_firmware_result<nv::ipc::TaskId::Pldm>(
    AuthenticateMcuFirmwareResponseParameter response)
{
    // send the auth result to pldm
    pldm::Request request{
        .type   = nv::pldm::RequestType::AuthMcuFinish,
        .length = 1,
        .rsv1   = 0,
        .rsv2   = 0,
    };

    request.buffer[0] = static_cast<uint8_t>(response.auth_result);

    if (pldm::Task::to_pldm(ipchandler::Id::Spdm, request) == false) {
        nv::info("to_pldm fail\n");
    }
}

template<>
void send_authenticate_ap_firmware_result<nv::ipc::TaskId::Pldm>(
    AuthenticateApFirmwareResponseParameter response)
{
    // send the auth result to pldm
    pldm::Request request{
        .type   = nv::pldm::RequestType::AuthApFinish,
        .length = 1,
        .rsv1   = 0,
        .rsv2   = 0,
    };

    request.buffer[0] = static_cast<uint8_t>(response.auth_result);

    if (pldm::Task::to_pldm(ipchandler::Id::Spdm, request) == false) {
        nv::info("to_pldm fail\n");
    }
}

}  // namespace nv::spdm::crypto
namespace nv::pldm {

extern "C" {
void send_pldm_msg_to_mctp(NvU8* data,
                           NvU8  client,
                           NvU8  src_eid,
                           NvU8  header_version,
                           NvU8  message_tag,
                           NvU32 size,
                           NvU8  is_request)
{
    const NvU32 LocalSize = size > (NV_PLDM_MAX_TX_SIZE - NV_PLDM_SHIFT_BYTES)
                              ? (NV_PLDM_MAX_TX_SIZE - NV_PLDM_SHIFT_BYTES)
                              : size;
    // nv::info("pldm: size %d\n", size);
    std::array<NvU8, NV_PLDM_MAX_TX_SIZE> buffer = {};

    // construct mctp hdr
    mctp::Packet packet        = {};
    uint16_t     packet_length = 0;
    if (size + NV_PLDM_MCTP_HDR_SIZE + NV_PLDM_MSG_TYPE_SIZE
        > std::numeric_limits<uint16_t>::max()) {
        packet_length = std::numeric_limits<uint16_t>::max();
    }
    else {
        packet_length = size + NV_PLDM_MCTP_HDR_SIZE + NV_PLDM_MSG_TYPE_SIZE;
    }
    Composer::construct_mctp_header(
        packet, packet_length, !is_request, src_eid, client, message_tag, header_version);
    // Copy priv and hdr to the buffer
    memcpy(buffer.data(), &packet.priv, sizeof(packet.priv));
    memcpy(buffer.data() + sizeof(packet.priv), &packet.hdr, sizeof(packet.hdr));

    // Fill pldm type
    buffer[NV_PLDM_PRIV_MCTP_HDR_SIZE + NV_PLDM_MCTP_HDR_SIZE] = 1;

    // copy payload
    std::copy(data, data + LocalSize, buffer.data() + NV_PLDM_SHIFT_BYTES);

#if 0
    for (int i = 0; i < 16; ++i) {
        nv::info("i %d v 0x%x\n", i, buffer[i]);
    }
#endif

    std::span<uint8_t> span_item(buffer.data(), NV_PLDM_MAX_TX_SIZE);

    auto status = Driver::mctp_send_from_pldm(span_item);
    if (status != nv::mctp::Status::Ok) {
        nv::info("pldm:mctp_send_from_pldm status %d\n", status);
    }
}

void pldm_write(NvU16                                          comp_id,
                std::array<uint8_t, NV_PLDM_MAX_PAYLOAD_SIZE>& buffer,
                NvU32                                          offset,
                NvU32                                          size,
                NvU16&                                         err)
{
    if (comp_id == sys::flash::config::McuComponentId) {
        constexpr static uint32_t Alignment = NV_PLDM_ERASE_ALIGNMENT;
        constexpr static uint32_t PageSize  = NV_PLDM_FLASH_PAGE_SIZE;
        uint32_t                  LoopSize  = size / NV_PLDM_FLASH_PAGE_SIZE;
        if (size % NV_PLDM_FLASH_PAGE_SIZE != 0) {
            LoopSize += 1;  // if not multiple of page size, write one more page
        }

        // @WAR disable wp for ES bringup
#if 0
        // read wp
        uint8_t data = 0;
        if (nv::ipc::GlobalWpPort != nv::gpio::InvalidGpioPort) {
            nv::gpio::Driver::read(nv::ipc::GlobalWpPort, nv::ipc::GlobalWpPin, data);
            nv::info("pldm::wp = %d\n", data);
            if (data == static_cast<uint8_t>(nv::gpio::GpioState::High)) {
                err = NV_PLDM_FW_RET_WP_ON;
                return;
            }
        }
#endif

        // erase unit size 8KB
        if ((offset & Alignment) == 0) {
            auto status = flash::Flash::erase(offset, 1s);
            if (status != flash::Status::Ok) {
                err = NV_PLDM_RET_IMAGE_WRITE_FAIL;
                return;
            }
        }
        // write 256B each time until 4KB
        for (uint32_t i = 0; i < LoopSize; i++) {
            const ipc::Queue::Item Item(buffer.begin() + PageSize * i,
                                        buffer.begin() + PageSize * (i + 1));
            auto status = flash::Flash::write(offset + i * PageSize, Item, 1s);
            if (status != flash::Status::Ok) {
                err = NV_PLDM_RET_IMAGE_WRITE_FAIL;
                return;
            }
        }
        err = NV_PLDM_RET_SUCCUSS;
    }
    else {
        // TODO: support other component id
        if (offset == 0) {
            nv::ap_operation::pldm_update_ap_fw_prepare();
        }
        nv::ap_operation::write_data_to_ap(offset, buffer);
        err = NV_PLDM_RET_SUCCUSS;
    }
}

void pldm_update_pds(NvU16 component_id, NvU8 status, NvU16& err)
{
    if (component_id == sys::flash::config::McuComponentId) {
        bootloader::Driver::ImageIndex inactive_slot{};
        flash::Data                    update_state{};
        bool                           bootable     = false;
        auto                           active_index = bootloader::Driver::current_boot_index();

        if (active_index == bootloader::Driver::ImageIndex::Image0) {
            inactive_slot = bootloader::Driver::ImageIndex::Image1;
        }
        else if (active_index == bootloader::Driver::ImageIndex::Image1) {
            inactive_slot = bootloader::Driver::ImageIndex::Image0;
        }
        else {
            err = NV_PLDM_RET_BOOTFSM_LEDGER_PROGRAM_FAIL;
            return;
        }

        if (status == static_cast<flash::Data>(PldmApFwStatus::Update_In_Progress)) {
            update_state = static_cast<flash::Data>(bootloader::Driver::State::InProgress);
            bootable     = false;
        }
        else if (status == static_cast<flash::Data>(PldmApFwStatus::Update_Complete)) {
            update_state = static_cast<flash::Data>(bootloader::Driver::State::Complete);
            bootable     = true;
        }
        else if (status == static_cast<flash::Data>(PldmApFwStatus::Staged)) {
            update_state = static_cast<flash::Data>(bootloader::Driver::State::Stage);
            bootable     = false;
        }
        else {
            update_state = static_cast<flash::Data>(bootloader::Driver::State::Invalid);
        }

        if (!bootable) {
            auto bootloader_status = bootloader::Driver::set_inactive_bootable(bootable);
            if (bootloader_status != true) {
                err = NV_PLDM_RET_BOOTFSM_LEDGER_PROGRAM_FAIL;
                return;
            }
        }

        auto flash_status = flash::Flash::set_data(flash::Key::PdsUpdateSlot,
                                                   static_cast<flash::Data>(inactive_slot));
        if (flash_status != flash::Status::Ok) {
            err = NV_PLDM_RET_BOOTFSM_LEDGER_PROGRAM_FAIL;
            return;
        }

        flash_status = flash::Flash::set_data(flash::Key::PdsUpdateState, update_state);
        if (flash_status != flash::Status::Ok) {
            err = NV_PLDM_RET_BOOTFSM_LEDGER_PROGRAM_FAIL;
            return;
        }
        // Rot state info change due to pldm update
        Driver::mctp_send_cmd(Driver::CmdCode::RotStateInfoChange);
    }
    else {
        NvU8 is_valid = 0;
        get_fw_info_from_list(component_id, is_valid, FwInfoList);
        if (is_valid) {
            flash::Data update_state{};
            if (status == static_cast<flash::Data>(PldmApFwStatus::Update_In_Progress)) {
                update_state = static_cast<flash::Data>(
                    nv::fw_parser::ap::ApFwStatus::Update_In_Progress);
            }
            else if (status == static_cast<flash::Data>(PldmApFwStatus::Update_Complete)) {
                update_state = static_cast<flash::Data>(
                    nv::fw_parser::ap::ApFwStatus::Update_Complete);
            }
            else {
                update_state = static_cast<flash::Data>(
                    nv::fw_parser::ap::ApFwStatus::Update_Complete_But_Not_Activate);
            }

            auto flash_status = flash::Flash::set_data(flash::Key::NpdsAp0FwStatus,
                                                       update_state);
            if (flash_status != flash::Status::Ok) {
                err = NV_PLDM_RET_BOOTFSM_LEDGER_PROGRAM_FAIL;
                return;
            }
        }
    }

    err = NV_PLDM_RET_SUCCUSS;
}

void pldm_get_update_state_pds(NvU8& status)
{
    flash::Data update_state{};
    status            = 0;
    auto flash_status = flash::Flash::get_data(flash::Key::PdsUpdateState, update_state);
    if (flash_status != flash::Status::Ok) {
        update_state = static_cast<flash::Data>(bootloader::Driver::State::Invalid);
    }
    status = static_cast<NvU8>(update_state & UINT8_MAX);
}

void pldm_get_active_version(NvU16& major, NvU8& minor, NvU16& patch, NvU16& build)
{
    major = MCU_FW_MAJOR;
    minor = MCU_FW_MINOR;
    patch = MCU_FW_PATCH;
    build = MCU_FW_BUILD;
}

void pldm_get_ap_active_version(NvU16& major, NvU8& minor, NvU16& patch, NvU16& build)
{
    auto tbs_data = nv::fw_parser::ap::get_ap_fw_version(
        nv::fw_parser::ap::ParsingApFwType::ActiveSlot);
    if (tbs_data.has_value()) {
        major = tbs_data->major;
        minor = tbs_data->minor;
        patch = tbs_data->patch;
        build = tbs_data->build;
    }
    else {
        major = 0;
        minor = 0;
        patch = 0;
        build = 0;
    }
}

void pldm_get_pending_version(NvU16& major, NvU8& minor, NvU16& patch, NvU16& build)
{
    constexpr static uint32_t FwOffset = sys::flash::config::Slot1FwAddress
                                       + NV_PLDM_NV_HEADER_OFFSET;
    std::array<NvU8, 16> buffer  = {};
    NvU16                major_0 = 0, major_1 = 0;
    NvU16                patch_0 = 0, patch_1 = 0;
    NvU16                build_0 = 0, build_1 = 0;

    // init
    major = 0;
    minor = 0;
    patch = 0;
    build = 0;

    const ipc::Queue::Item Item(buffer.begin(), buffer.begin() + 16);
    auto                   status = flash::Flash::read(FwOffset, Item, 1s);

    if (status == flash::Status::Ok) {
        uint64_t check_wrap_value   = 0;
        major_0                     = buffer[NV_PLDM_MAJOR_OFFSET];
        major_1                     = buffer[NV_PLDM_MAJOR_OFFSET + 1];
        constexpr uint8_t ByteShift = 8u;
        check_wrap_value            = major_0 + (major_1 << ByteShift);
        if (check_wrap_value
            <= std::numeric_limits<std::remove_reference_t<decltype(major)> >::max()) {
            major = check_wrap_value;
        }

        minor = buffer[NV_PLDM_MINOR_OFFSET];

        patch_0 = buffer[NV_PLDM_PATCH_OFFSET];
        patch_1 = buffer[NV_PLDM_PATCH_OFFSET + 1];

        check_wrap_value = patch_0 + (patch_1 << ByteShift);
        if (check_wrap_value
            <= std::numeric_limits<std::remove_reference_t<decltype(patch)> >::max()) {
            patch = check_wrap_value;
        }

        build_0          = buffer[NV_PLDM_BUILD_OFFSET];
        build_1          = buffer[NV_PLDM_BUILD_OFFSET + 1];
        check_wrap_value = build_0 + (build_1 << ByteShift);
        if (check_wrap_value
            <= std::numeric_limits<std::remove_reference_t<decltype(build)> >::max()) {
            build = check_wrap_value;
        }
    }
}

void pldm_get_ap_pending_version(NvU16& major, NvU8& minor, NvU16& patch, NvU16& build)
{
    flash::Data status{};
    major             = 0;
    minor             = 0;
    patch             = 0;
    build             = 0;
    auto flash_status = flash::Flash::get_data(flash::Key::NpdsAp0FwStatus, status);
    if (flash_status != flash::Status::Ok
        || status != static_cast<flash::Data>(nv::fw_parser::ap::ApFwStatus::Update_Complete)) {
        return;
    }

    auto tbs_data = nv::fw_parser::ap::get_ap_fw_version(
        nv::fw_parser::ap::ParsingApFwType::UpdateSlot);
    if (tbs_data.has_value()) {
        major = tbs_data->major;
        minor = tbs_data->minor;
        patch = tbs_data->patch;
        build = tbs_data->build;
    }
}

void set_timer(NvU64 ns)
{
    const std::chrono::nanoseconds Ns(ns);
    pldm::Task::set_timer(std::chrono::duration_cast<std::chrono::microseconds>(Ns));
}

void clear_timer()
{
    pldm::Task::clear_timer();
}

void get_ticks(NvU32& ticks)
{
    ticks = pldm::Task::get_ticks();
}

void set_log(NvU16 event, NvU8 info0, NvU8 info1, NvU8 info2, NvU8 info3)
{
    const nv::logger::EventData Data{static_cast<uint8_t>(info0),
                                     static_cast<uint8_t>(info1),
                                     static_cast<uint8_t>(info2),
                                     static_cast<uint8_t>(info3)};
    nv::logger::info(nv::logger::EventStructItem{event, nv::logger::Level::Info}, Data);
}

void get_fw_info(NvU16  input_component_id,
                 NvU16& output_component_id,
                 NvU32& fw_size,
                 NvU32& fw_offset,
                 NvU32& ap_sku_id,
                 NvU8&  build_mode,
                 NvU8&  fw_state,
                 NvU8&  is_valid)
{
    if (input_component_id == sys::flash::config::McuComponentId) {
        is_valid            = true;
        output_component_id = sys::flash::config::McuComponentId;
        fw_size             = sys::flash::config::FirmwareMaxSize;
        fw_offset           = sys::flash::config::Slot1FwAddress;
        ap_sku_id           = MCU_AP_SKU;
        build_mode          = NV_BUILD_MODE;
        pldm_get_update_state_pds(fw_state);
    }
    else {
        const FwInfo fw_info = get_fw_info_from_list(input_component_id, is_valid, FwInfoList);
        if (is_valid) {
            output_component_id = fw_info.component_id;
            fw_size             = fw_info.fw_size;
            fw_offset           = fw_info.fw_offset;
            ap_sku_id           = fw_info.ap_sku_id;
            build_mode          = fw_info.build_mode;
            pldm_get_update_state_pds(fw_state);
        }
    }
}

void get_fw_info_from_metadata(
    Metadata* metadata, NvU8& minor, NvU16& patch, NvU16& build, NvU32& ap_sku_id)
{
    minor     = metadata->minor;
    patch     = metadata->patch;
    build     = metadata->build;
    ap_sku_id = metadata->ap_sku_id;
}

bool is_in_range(NvU32 offset, NvU32 size, NvU32 start, NvU32 end)
{
    if (size > std::numeric_limits<NvU32>::max() - offset) {
        return false;  // prevent wrap
    }
    if (start >= offset && end < (offset + size)) {
        return true;
    }
    return false;
}

void check_offset_within_transfer_size(NvU32 offset,
                                       NvU8  data_size_in_bytes,
                                       NvU32 transfer_size)
{
    // Prevent divide zero or unsigned overflow
    if (transfer_size == 0 || data_size_in_bytes == 0
        || offset > std::numeric_limits<NvU32>::max() - data_size_in_bytes + 1) {
        nv::logger::info(nv::logger::Event::PldmMetadataCrossTransferSize,
                         nv::logger::data_from_two_u32(offset, transfer_size));
        return;
    }
    if (offset / transfer_size != (offset + data_size_in_bytes - 1) / transfer_size) {
        nv::logger::info(nv::logger::Event::PldmMetadataCrossTransferSize,
                         nv::logger::data_from_two_u32(offset, transfer_size));
    }
}

void parse_header(NvU32     cur_fw_offset,
                  NvU32     transfer_size,
                  NvU8*     data,
                  Metadata* metadata,
                  NvU8&     is_recv_all_metadata)
{
    constexpr unsigned OffsetFirstByte  = 8u;
    constexpr unsigned OffsetSecondByte = 16u;
    constexpr unsigned OffsetThirdByte  = 24u;

    // major
    constexpr static uint32_t MajorOffset = NV_PLDM_NV_HEADER_OFFSET + NV_PLDM_MAJOR_OFFSET;
    if (is_in_range(cur_fw_offset, transfer_size, MajorOffset, MajorOffset + 1)) {
        metadata->major = ((unsigned)data[MajorOffset - cur_fw_offset]
                           | (unsigned)data[MajorOffset + 1 - cur_fw_offset] << OffsetFirstByte)
                        & UINT16_MAX;
    }

    // minor
    constexpr static uint32_t MinorOffset = NV_PLDM_NV_HEADER_OFFSET + NV_PLDM_MINOR_OFFSET;
    if (is_in_range(cur_fw_offset, transfer_size, MinorOffset, MinorOffset)) {
        metadata->minor = data[MinorOffset - cur_fw_offset];
    }

    // patch
    constexpr static uint32_t PatchOffset = NV_PLDM_NV_HEADER_OFFSET + NV_PLDM_PATCH_OFFSET;
    if (is_in_range(cur_fw_offset, transfer_size, PatchOffset, PatchOffset + 1)) {
        metadata->patch = ((unsigned)data[PatchOffset - cur_fw_offset]
                           | (unsigned)data[PatchOffset + 1 - cur_fw_offset] << OffsetFirstByte)
                        & UINT16_MAX;
    }

    // build
    constexpr static uint32_t BuildOffset = NV_PLDM_NV_HEADER_OFFSET + NV_PLDM_BUILD_OFFSET;
    if (is_in_range(cur_fw_offset, transfer_size, BuildOffset, BuildOffset + 1)) {
        metadata->build = ((unsigned)data[BuildOffset - cur_fw_offset]
                           | (unsigned)data[BuildOffset + 1 - cur_fw_offset] << OffsetFirstByte)
                        & UINT16_MAX;
    }

    // query apsku
    constexpr static uint32_t ApSkuOffset = NV_PLDM_NV_HEADER_OFFSET + NV_PLDM_APSKU_OFFSET;
    if (is_in_range(cur_fw_offset, transfer_size, ApSkuOffset, ApSkuOffset + 3)) {
        // the first download chunk is done
        is_recv_all_metadata = 1;
        metadata->ap_sku_id  = (NvU32)data[ApSkuOffset - cur_fw_offset]
                            | ((NvU32)data[ApSkuOffset + 1 - cur_fw_offset] << OffsetFirstByte)
                            | ((NvU32)data[ApSkuOffset + 2 - cur_fw_offset] << OffsetSecondByte)
                            | ((NvU32)data[ApSkuOffset + 3 - cur_fw_offset] << OffsetThirdByte);
    }

    // check offset of all metadata when receive first download chunk
    if (cur_fw_offset == 0) {
        check_offset_within_transfer_size(MajorOffset, 2, transfer_size);
        check_offset_within_transfer_size(MinorOffset, 1, transfer_size);
        check_offset_within_transfer_size(PatchOffset, 2, transfer_size);
        check_offset_within_transfer_size(BuildOffset, 2, transfer_size);
        check_offset_within_transfer_size(ApSkuOffset, 4, transfer_size);
    }
}

void get_pcie_info(NvU16& vid, NvU16& did, NvU16& ssvid, NvU16& ssdid)
{
    // https://confluence.nvidia.com/display/GFWBC/MCU+SMA+PLDM+T5+Inventory
    constexpr NvU16 PcieVendorId          = 0x10de;
    constexpr NvU16 PcieDeviceId          = 0x2fad;
    constexpr NvU16 PcieSubsystemVendorId = 0x10de;
    vid                                   = PcieVendorId;
    did                                   = PcieDeviceId;
    ssvid                                 = PcieSubsystemVendorId;
    ssdid                                 = MCU_SSDID;
}

void get_uuid_info(NvU8* data)
{
    constexpr std::array<NvU8, 16> Uuid = {0xb5,
                                           0xf8,
                                           0xee,
                                           0x87,
                                           0xf7,
                                           0xaa,
                                           0x4f,
                                           0x1d,
                                           0x9d,
                                           0x9a,
                                           0xb1,
                                           0x7b,
                                           0x34,
                                           0x9f,
                                           0x88,
                                           0xc1};

    memcpy(data, Uuid.data(), Uuid.size());
}

void request_authentication(NvU16 comp_id, NvU32& auth_request_id)
{
    // get current tick
    get_ticks(auth_request_id);

    if (comp_id == sys::flash::config::McuComponentId) {
        auto status = nv::spdm::crypto::authenticate_mcu_firmware(
            nv::fw_parser::mcu::ParsingFwType::InactiveSlot);
        if (status != nv::spdm::crypto::CryptoStatus::Success) {
            // need error handling
        }
    }
    else {
        // TODO: support other component id
        auto status = nv::spdm::crypto::authenticate_ap_firmware(
            nv::fw_parser::ap::ParsingApFwType::UpdateSlot);
        if (status != nv::spdm::crypto::CryptoStatus::Success) {
            // need error handling
        }
    }
}

void get_mcu_authentication_result(uint8_t result, uint8_t& verify_cc)
{
#ifdef NV_UNITTEST
    verify_cc = NV_PLDM_VERIFY_CC_SUCCESS;
#else

    // need to replace end
    nv::logger::info(nv::logger::Event::PldmAuthInactiveCryptoStatus, {result});
    switch (result) {
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::Success):
            verify_cc = NV_PLDM_VERIFY_CC_SUCCESS;
            break;
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::FailSecurityVersionRollBack):
            verify_cc = NV_PLDM_VERIFY_CC_ROLLBACK_FAIL;
            break;
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::FailImageSigningKeyRevoke):
            verify_cc = NV_PLDM_VERIFY_CC_KEY_FAIL;
            break;
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::FailNbootImageAuthenticate):
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::FailParsingFirmware):
            verify_cc = NV_PLDM_VERIFY_CC_IMAGE_AUTH_FAIL;
            break;
        default: verify_cc = NV_PLDM_VERIFY_CC_ERROR; break;
    }
#endif
}

void get_ap_authentication_result(uint8_t result, uint8_t& verify_cc)
{
    nv::logger::info(nv::logger::Event::PldmAuthInactiveCryptoStatus, {result});
    nv::ap_operation::pldm_update_ap_fw_callback(
        static_cast<spdm::crypto::CryptoStatus>(result));
    switch (result) {
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::Success):
            verify_cc = NV_PLDM_VERIFY_CC_SUCCESS;
            break;
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::FailSecurityVersionRollBack):
            verify_cc = NV_PLDM_VERIFY_CC_ROLLBACK_FAIL;
            break;
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::FailImageSigningKeyRevoke):
            verify_cc = NV_PLDM_VERIFY_CC_KEY_FAIL;
            break;
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::FailNbootImageAuthenticate):
        case static_cast<uint8_t>(nv::spdm::crypto::CryptoStatus::FailParsingFirmware):
            verify_cc = NV_PLDM_VERIFY_CC_IMAGE_AUTH_FAIL;
            break;
        default: verify_cc = NV_PLDM_VERIFY_CC_ERROR; break;
    }
}

void print_msg(NvU32 data)
{
    nv::info("debug data:");
    nv::info(" 0x%x\n", data);
}

void get_arr_comp_id_len(NvU32& arrlen)
{
    // coverity[cert_int31_c_violation] - valid cast
    arrlen = static_cast<NvU32>(AllApComponentId.size());
}

const NvU16* get_arr_comp_id_address()
{
    return AllApComponentId.data();
}

}  // namespace nv::pldm
}  // namespace nv::pldm
