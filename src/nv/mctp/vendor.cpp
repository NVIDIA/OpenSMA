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

#include "nv/mctp/vendor.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ranges>

#include "nv/debugtoken/debugtoken.h"
#include "nv/flash/driver.h"
#include "nv/gpio/driver.h"
#include "nv/logger/log.h"
#include "nv/mctp/constants.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/selftest.h"
#include "nv/nv.h"
#include "nv/pldm/task.h"
#include "nv/spdm/cert_library.h"
#include "nv/spdm/spdm_cert_chain.h"
#include "sys/i2c/utils.h"

#include NV_IPC_CONFIG_H

using namespace nv;
using namespace mctp;

bool Vendor::process(const Packet& rx, Packet& tx)
{
    auto& vdr          = VendorPktReq::from(rx);
    auto  command_code = static_cast<nv::mctp::VdmCmd>(vdr.command_code);
    bool  result       = true;

    switch (command_code) {
        case VdmCmd::QueryBootStatus    : break;
        case VdmCmd::DownloadLog        : on_download_log(rx, tx); break;
        case VdmCmd::SelfTest           : on_self_test(rx, tx); break;
        case VdmCmd::BackgroundCopy     : on_background_copy(rx, tx); break;
        case VdmCmd::GetGpioStatus      : on_get_gpio_status(rx, tx); break;
        case VdmCmd::ReadDevIkCsr       : on_read_devik_csr(rx, tx); break;
        case VdmCmd::ProgramCertificate : on_program_certificate(rx, tx); break;
        case VdmCmd::InstallDbgToken    : on_install_dbgtoken(rx, tx); break;
        case VdmCmd::EraseDbgToken      : on_erase_dbgtoken(rx, tx); break;
        case VdmCmd::QueryDbgTokenStatus: on_query_dbgtoken_status(rx, tx); break;
        case VdmCmd::RegTableAccess     : result = on_register_table_access(rx, tx); break;
        case VdmCmd::AddExtTimestamp    : on_add_ext_timestamp(rx, tx); break;
        case VdmCmd::ScanI2c            : on_scan_i2c(rx, tx); break;
        default                         : unsupported_command(rx, tx); return false;
    }
    return result;
}

bool Vendor::action(const Packet& rx, Packet& tx) const
{
    auto& vdr          = VendorPktReq::from(rx);
    auto  command_code = static_cast<nv::mctp::VdmCmd>(vdr.command_code);

    switch (command_code) {
        case VdmCmd::QueryBootStatus    : break;
        case VdmCmd::SelfTest           : break;
        case VdmCmd::BackgroundCopy     : action_background_copy(rx, tx); break;
        case VdmCmd::GetGpioStatus      : break;
        case VdmCmd::DownloadLog        : break;
        case VdmCmd::ReadDevIkCsr       : break;
        case VdmCmd::ProgramCertificate : break;
        case VdmCmd::InstallDbgToken    : break;
        case VdmCmd::EraseDbgToken      : break;
        case VdmCmd::QueryDbgTokenStatus: break;
        case VdmCmd::AddExtTimestamp    : break;
        case VdmCmd::ScanI2c            : break;
        case VdmCmd::RegTableAccess:
            if constexpr (ubs::features::reg_table) {
                break;
            }
            else {
                [[fallthrough]];
            }
        default: return false;
    }
    return true;
}

void Vendor::on_query_boot_status(const Packet& rx, Packet& tx) const
{
    constexpr uint64_t StatusCodeExample = 0x0123456789ABCDEF;
    constexpr uint8_t  Shift56           = 56;
    constexpr uint8_t  Shift48           = 48;
    constexpr uint8_t  Shift40           = 40;
    constexpr uint8_t  Shift32           = 32;
    constexpr uint8_t  Shift24           = 24;
    constexpr uint8_t  Shift16           = 16;
    constexpr uint8_t  Shift8            = 8;
    constexpr uint8_t  Shift0            = 0;

    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vtx           = VendorPktRes::from(tx);
    vtx.completion_code = Ccode::Success;

    uint64_t status = 0;

    // check version
    if (vtx.msg_version != 0x01) {
        vtx.completion_code = Ccode::ErrorUnsupportedCmd;
        return;
    }

    // TODO: need to get the real data
    status = StatusCodeExample;

    vtx.data[0] = (status >> Shift56) & UINT8_MAX;
    vtx.data[1] = (status >> Shift48) & UINT8_MAX;
    vtx.data[2] = (status >> Shift40) & UINT8_MAX;
    vtx.data[3] = (status >> Shift32) & UINT8_MAX;
    vtx.data[4] = (status >> Shift24) & UINT8_MAX;
    vtx.data[5] = (status >> Shift16) & UINT8_MAX;
    vtx.data[6] = (status >> Shift8) & UINT8_MAX;
    vtx.data[7] = (status >> Shift0) & UINT8_MAX;

    tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 8;
}

void Vendor::on_self_test(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vtx           = VendorPktRes::from(tx);
    auto& vrx           = VendorPktReq::from(rx);
    vtx.completion_code = Ccode::Success;

    // check version
    if (vtx.msg_version != 0x01) {
        vtx.completion_code = Ccode::ErrorUnsupportedCmd;
        return;
    }

    uint32_t self_test{};
    memcpy(&self_test, &vrx.data[0], sizeof(self_test));

    /* version */
    vtx.data[0] = 1;

    constexpr auto SupportedCmds = SelfTest::supported_cmds();
    if ((self_test & SupportedCmds) != self_test) {
        tx.priv.packet_length  = sizeof(Header) + HeaderSizeResponse + 5;
        vtx.completion_code    = Ccode::ErrorUnsupportedCmd;
        self_test             &= SupportedCmds;
        memcpy(&vtx.data[0], &self_test, sizeof(self_test));
    }
    else {
        const uint32_t NeedLength = SelfTest::get_length(self_test);

        if (NeedLength > SelfTest::MaxResponseLength) {
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
            vtx.completion_code   = Ccode::ErrorInvalidLength;
        }
        else {
            // self test version is 1 bytes
            auto status = SelfTest::populate_result(
                self_test, std::span<uint8_t>(&vtx.data[1], sizeof(vtx.data) - 1));

            if (status == SelfTestStatus::Success) {
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1 + NeedLength;
                vtx.completion_code   = Ccode::Success;
            }
            else {
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
                vtx.completion_code   = Ccode::ErrorNotReady;
            }
        }
    }
}

void Vendor::on_background_copy(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vrx = VendorPktReq::from(rx);
    auto& vtx = VendorPktRes::from(tx);

    flash::Status flash_status{};

    vtx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse;

    // check version
    if (vtx.msg_version != 0x01 && vtx.msg_version != 0x02) {
        vtx.completion_code = Ccode::ErrorUnsupportedCmd;
        return;
    }

    // cmd code
    switch (vrx.data[0]) {
        case static_cast<uint8_t>(BackgroundCopyCmd::Disable_Bg):
            flash_status = flash::Flash::set_data(
                flash::Key::PdsBackgroundSetup,
                static_cast<flash::Data>(BackgroundCopyPolicy::Disable));
            if (flash_status != flash::Status::Ok) {
                vtx.completion_code = Ccode::ErrorUpdateDbFail;
            }
            flash_status = flash::Flash::set_data(
                flash::Key::PdsBackgroundSetupOneTime,
                static_cast<flash::Data>(BackgroundCopyPolicy::Default));
            if (flash_status != flash::Status::Ok) {
                vtx.completion_code = Ccode::ErrorUpdateDbFail;
            }
            break;
        case static_cast<uint8_t>(BackgroundCopyCmd::Enable_Bg):

            // @todo is token install
            flash_status = flash::Flash::set_data(
                flash::Key::PdsBackgroundSetup,
                static_cast<flash::Data>(BackgroundCopyPolicy::Enable));
            if (flash_status != flash::Status::Ok) {
                vtx.completion_code = Ccode::ErrorUpdateDbFail;
            }
            flash_status = flash::Flash::set_data(
                flash::Key::PdsBackgroundSetupOneTime,
                static_cast<flash::Data>(BackgroundCopyPolicy::Default));
            if (flash_status != flash::Status::Ok) {
                vtx.completion_code = Ccode::ErrorUpdateDbFail;
            }
            break;
        case static_cast<uint8_t>(BackgroundCopyCmd::Disable_Bg_One_Time):

            flash_status = flash::Flash::set_data(
                flash::Key::PdsBackgroundSetupOneTime,
                static_cast<flash::Data>(BackgroundCopyPolicy::OnceDisable));
            if (flash_status != flash::Status::Ok) {
                vtx.completion_code = Ccode::ErrorUpdateDbFail;
            }
            break;
        case static_cast<uint8_t>(BackgroundCopyCmd::Enable_Bg_One_Time):
            // @todo is token install
            flash_status = flash::Flash::set_data(
                flash::Key::PdsBackgroundSetupOneTime,
                static_cast<flash::Data>(BackgroundCopyPolicy::OnceEnable));
            if (flash_status != flash::Status::Ok) {
                vtx.completion_code = Ccode::ErrorUpdateDbFail;
            }
            break;
        case static_cast<uint8_t>(BackgroundCopyCmd::Init_Bg_Update): break;
        case static_cast<uint8_t>(BackgroundCopyCmd::Query_Status)  : {
            flash::Data background_copy_policy{};
            flash::Data background_copy_policy_one_time{};
            // +1 <background copy policy>
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
            flash_status          = flash::Flash::get_data(flash::Key::PdsBackgroundSetup,
                                                  background_copy_policy);
            if (flash_status != flash::Status::Ok) {
                background_copy_policy = static_cast<flash::Data>(
                    BackgroundCopyPolicy::Default);
            }
            flash_status = flash::Flash::get_data(flash::Key::PdsBackgroundSetupOneTime,
                                                  background_copy_policy_one_time);
            if (flash_status != flash::Status::Ok) {
                background_copy_policy_one_time = static_cast<flash::Data>(
                    BackgroundCopyPolicy::Default);
            }

            // set policy
            if (background_copy_policy_one_time
                == static_cast<flash::Data>(BackgroundCopyPolicy::OnceEnable)) {
                vtx.data[0] = static_cast<uint8_t>(BackgroundCopyCmd::Enable_Bg_One_Time);
            }
            else if (background_copy_policy_one_time
                     == static_cast<flash::Data>(BackgroundCopyPolicy::OnceDisable)) {
                vtx.data[0] = static_cast<uint8_t>(BackgroundCopyCmd::Disable_Bg_One_Time);
            }
            else if (background_copy_policy
                     == static_cast<flash::Data>(BackgroundCopyPolicy::Disable)) {
                vtx.data[0] = static_cast<uint8_t>(BackgroundCopyCmd::Disable_Bg);
            }
            else {
                vtx.data[0] = static_cast<uint8_t>(BackgroundCopyCmd::Enable_Bg);
            }
        } break;
        case static_cast<uint8_t>(BackgroundCopyCmd::Query_Progress): {
            nv::flash::ProgressPercent progress{};
            // +1 <status> +1 <progress>
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 2;
            flash_status          = flash::Flash::background_copy_query(progress);
            // byte 1 status
            // version 1 definition
            // 0x1: No background copy in progress or background copy complete.
            // 0x2: Background copy in progress.
            if (vtx.msg_version == 0x01) {
                vtx.data[0] = (flash_status == flash::Status::BackgroundCopyInprogress)
                                ? static_cast<uint8_t>(
                                      BackgroundCopyState::Background_Copy_In_Progress)
                                : static_cast<uint8_t>(
                                      BackgroundCopyState::Background_Copy_Idle_Or_Complete);
            }
            // version 2 definition
            // 0x1 = No background copy in progress
            // 0x2 = Background copy in progress
            // 0x3 = Background Copy Success
            // 0x4 = Background Copy Failed
            else {
                if (flash_status == flash::Status::BackgroundCopyIdle) {
                    vtx.data[0] = static_cast<uint8_t>(
                        BackgroundCopyState::Background_Copy_Idle);
                }
                else if (flash_status == flash::Status::BackgroundCopyInprogress) {
                    vtx.data[0] = static_cast<uint8_t>(
                        BackgroundCopyState::Background_Copy_In_Progress);
                }
                else if (flash_status == flash::Status::BackgroundCopyDone) {
                    vtx.data[0] = static_cast<uint8_t>(
                        BackgroundCopyState::Background_Copy_Success);
                }
                else if (flash_status == flash::Status::BackgroundCopyFailed) {
                    vtx.data[0] = static_cast<uint8_t>(
                        BackgroundCopyState::Background_Copy_Failed);
                }
            }

            // byte 2 progress
            vtx.data[1] = progress;
        } break;
        case static_cast<uint8_t>(BackgroundCopyCmd::Query_Pending_Bg_Copy): {
            flash::Data pending_bg_copy{};
            // +1 <status>
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
            flash_status = flash::Flash::get_data(flash::Key::NpdsAllowInitBackgroundCopy,
                                                  pending_bg_copy);
            if (flash_status != flash::Status::Ok) {
                pending_bg_copy = 0;
            }
            vtx.data[0] = pending_bg_copy == 0
                            ? static_cast<uint8_t>(BackgroundCopyPending::No_Pending)
                            : static_cast<uint8_t>(BackgroundCopyPending::Pending);
        } break;
        default: vtx.completion_code = Ccode::ErrorUnsupportedCmd; break;
    }

    if (vtx.completion_code == Ccode::Success
        && vrx.data[0] <= static_cast<uint8_t>(BackgroundCopyCmd::Enable_Bg_One_Time)) {
        nv::logger::info(nv::logger::Event::MctpBackgroundCopySetup, {vrx.data[0]});
    }
}

void Vendor::on_read_devik_csr(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vtx           = VendorPktRes::from(tx);
    vtx.completion_code = Ccode::Success;

    // check version
    if (vtx.msg_version == 0x01) {
        const auto CsrLength = nv::spdm::cert::get_l4_csr_len();
        if (CsrLength > UINT16_MAX - sizeof(Header) - HeaderSizeResponse) {
            vtx.completion_code = Ccode::ErrorInvalidLength;
            return;
        }
        tx.priv.packet_length = CsrLength + sizeof(Header) + HeaderSizeResponse;
        // save stack usage, need to check the buffer size is enough
        if (tx.priv.packet_length > nv::mctp::Constants::MctpTxBufSize) {
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse;
            vtx.completion_code   = Ccode::ErrorUnsupportedCmd;
            return;
        }
        // create the array pointer to data.
        auto& l4_csr = *std::bit_cast<std::array<uint8_t,
                                                 nv::mctp::Constants::MctpTxBufSize
                                                     - sizeof(Header) - HeaderSizeResponse>*>(
            &vtx.data[0]);
        auto l4_csr_span = std::span<uint8_t>(l4_csr);
        nv::spdm::cert::read_l4_csr(l4_csr_span);
    }
    else {
        vtx.completion_code = Ccode::ErrorUnsupportedCmd;
        return;
    }
}

void Vendor::on_program_certificate(const Packet& rx, Packet& tx) const
{
    using namespace nv::spdm::certlib;

    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vtx = VendorPktRes::from(tx);
    auto& vrx = VendorPktReq::from(rx);
    // CCcode + Program Type + Certificate response
    tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 2;
    // expect all success first
    vtx.completion_code = Ccode::Success;
    vtx.data[0]         = vrx.data[0];
    vtx.data[1]         = ProgramSuccess;

    if (rx.priv.packet_length < (sizeof(VendorPktReq) - sizeof(VendorPktReq::data))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // calculate the length of data that mctp actually receive
    const uint32_t RxDataLength = rx.priv.packet_length
                                - (sizeof(VendorPktReq) - sizeof(VendorPktReq::data));
    const uint32_t InputCertlength = RxDataLength - 1;

    // copy the input cert data
    const CertArray& input_cert_array = *std::bit_cast<CertArray*>(&vrx.data[1]);
    // check the recieve certificate length and format is correct
    if (!check_certificate_format_valid(input_cert_array, InputCertlength)) {
        vtx.data[1] = InvalidCertificate;
    }

    // check version
    if (vtx.msg_version == 0x01 && vtx.data[1] == ProgramSuccess) {
        constexpr uint8_t L3Type = 0;
        constexpr uint8_t L4Type = 1;
        /* chaek type is the l3 or l4 cert to program */
        switch (vrx.data[0]) {
            case L3Type: {
                // check l3 cert is never programed with pds.
                nv::flash::Data   is_l3_program = 0;
                nv::flash::Status pds_status    = nv::flash::Flash::get_data(
                    nv::flash::Key::PdsL3CertificateProgramed, is_l3_program);
                if (pds_status != nv::flash::Status::Ok) {
                    vtx.data[1] = PdsL3CertReadFail;
                    break;
                }
                if (is_l3_program != 0) {
                    vtx.data[1] = AlreadyExists;
                    break;
                }

                // check input l3 cert DDA ordinal number
                // 1. dda ordinal on efuse == 0, no programd before
                // 2. dda ordinal on efuse !=1 but match the dda ordinal on input l3 cert,
                // recovery case
                uint32_t       dda_ordinal_number_otp  = 0;
                const uint32_t dda_ordinal_number_cert = parse_dda_ordinal_number(
                    input_cert_array);
                constexpr uint32_t DdaOrdinalEfuseAddr = 31;
                // check the dda ordinal number on the input cert is valid
                if (dda_ordinal_number_cert == std::numeric_limits<uint32_t>::max()) {
                    vtx.data[1] = OtpDdaOrdinalParseFail;
                    break;
                }
                // check the read operation of dda ordinal number on the otp is valid
                if (nv::flash::Status::Ok
                    != nv::flash::Flash::read_efuse(DdaOrdinalEfuseAddr,
                                                    dda_ordinal_number_otp)) {
                    vtx.data[1] = OtpDdaOrdinalReadFail;
                    break;
                }
                // check the dda ordinal number is never programed before or dda ordinal number
                // on the input cert match the dda ordinal number on the otp
                if ((dda_ordinal_number_otp != 0
                     && dda_ordinal_number_otp != dda_ordinal_number_cert)) {
                    vtx.data[1] = InvalidDdaOrdinalNumber;
                    break;
                }

                // check l2 exist
                if (nv::spdm::cert::get_l2_cert_len() == 0) {
                    vtx.data[1] = PrecedingCertificatesNoFound;
                    break;
                }

                // verify the l3 by l2 cert
                CertArray l2_cert_array{};
                nv::spdm::cert::read_l2_cert(std::span<uint8_t>(l2_cert_array));
                if (!validate_certificate_signature(l2_cert_array, input_cert_array)) {
                    vtx.data[1] = SignatureValidationFail;
                    break;
                }

                // all check pass, let l3 cert insatll in internal flash
                const nv::flash::Address L3CertPhyAddr = 0xEC000;
                auto get_virtual_address = [](nv::flash::Address phy_cert_location) {
                    const nv::flash::Address
                        VirCertLocation = nv::flash::Flash::get_flash_address(
                            phy_cert_location, nv::bootloader::Driver::current_boot_index());
                    return VirCertLocation;
                };
                const nv::flash::Address L3CertVirAddr = get_virtual_address(L3CertPhyAddr);
                auto erase_to_flash = [](nv::flash::Address start_erase_flash_address,
                                         nv::flash::Address end_erase_flash_address) {
                    if (start_erase_flash_address % nv::flash::SectorSize != 0) {
                        return false;
                    }
                    if (end_erase_flash_address % nv::flash::SectorSize != 0) {
                        return false;
                    }
                    while (start_erase_flash_address < end_erase_flash_address) {
                        if (nv::flash::Flash::erase(start_erase_flash_address)
                            != nv::flash::Status::Ok) {
                            return false;
                        }
                        start_erase_flash_address += nv::flash::SectorSize;
                    }
                    return true;
                };
                auto write_to_flash = [](nv::flash::Address       write_flash_address,
                                         std::span<const uint8_t> write_data) {
                    if (write_data.size() % 256 != 0) {
                        return false;
                    }
                    uint32_t complete_size = 0;
                    while (complete_size < write_data.size()) {
                        constexpr uint32_t WriteSize = 256;
                        const auto         Chunk = write_data.subspan(complete_size, WriteSize);
                        auto flash_status = nv::flash::Flash::write(write_flash_address, Chunk);
                        if (flash_status != nv::flash::Status::Ok) {
                            return false;
                        }
                        if (write_flash_address > UINT32_MAX - WriteSize) {
                            return false;
                        }
                        write_flash_address += WriteSize;
                        if (complete_size > UINT32_MAX - WriteSize) {
                            return false;
                        }
                        complete_size += WriteSize;
                    }
                    return true;
                };
                if (!erase_to_flash(L3CertVirAddr, L3CertVirAddr + nv::flash::SectorSize)) {
                    vtx.data[1] = FlashEraseFail;
                    break;
                }
                if (!write_to_flash(L3CertVirAddr,
                                    std::span<const uint8_t>(input_cert_array))) {
                    vtx.data[1] = FlashWriteFail;
                    break;
                }
                // update the pds
                pds_status = nv::flash::Flash::set_data(
                    nv::flash::Key::PdsL3CertificateProgramed, 1);
                if (pds_status != nv::flash::Status::Ok) {
                    vtx.data[1] = PdsL3CertSetFail;
                    break;
                }
                // update the otp
                if (nv::flash::Status::Ok
                    != nv::flash::Flash::program_efuse(DdaOrdinalEfuseAddr,
                                                       dda_ordinal_number_cert)) {
                    vtx.data[1] = OtpDdaOrdinalProgrammedFail;
                    break;
                }
            } break;
            case L4Type: {
                constexpr size_t FuseArrSizeForSinature                                    = 12;
                constexpr std::array<uint32_t, FuseArrSizeForSinature> SignatureEfuseAddrR = {
                    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43};
                constexpr std::array<uint32_t, FuseArrSizeForSinature> SignatureEfuseAddrS = {
                    44, 45, 46, 47, 48, 49, 50, 63, 64, 65, 66, 67};

                // check otp of signature is not programed
                auto check_efuse_programed =
                    [&](const std::array<uint32_t, FuseArrSizeForSinature>& EfuseAddrArray) {
                        for (const auto EfuseAddr : EfuseAddrArray) {
                            uint32_t data = 0;
                            auto     ret  = nv::flash::Flash::read_efuse(EfuseAddr, data);
                            // encounter error when read efuse
                            if (ret != nv::flash::Status::Ok) {
                                vtx.data[1] = OtpL4SignatureReadFail;
                                return false;
                            }
                            // already programed
                            if (data != 0) {
                                vtx.data[1] = AlreadyExists;
                                return false;
                            }
                        }
                        return true;
                    };
                if (!check_efuse_programed(SignatureEfuseAddrR)
                    || !check_efuse_programed(SignatureEfuseAddrS)) {
                    break;
                }

                // check l3 exist
                if (nv::spdm::cert::get_l3_cert_len() == 0) {
                    vtx.data[1] = PrecedingCertificatesNoFound;
                    break;
                }

                // verify the l4 by l3 cert
                CertArray l3_cert_array{};
                nv::spdm::cert::read_l3_cert(std::span<uint8_t>(l3_cert_array));
                if (!validate_certificate_signature(l3_cert_array, input_cert_array)) {
                    vtx.data[1] = SignatureValidationFail;
                    break;
                }

                // all check pass, start to install the signature of L4
                // hrer is a little complex, firstly we need to find the acutually size of
                // singature data without padding and write the signature back form the end to
                // start.
                Signature          l4sign = parse_signature(input_cert_array);
                std::span<uint8_t> l4_sign_r_data_without_padding_redundant_size;
                {  // r value
                    // if have padding zero, the start offset should add 1
                    const uint32_t StartOffsetOfSign = l4sign.r_value.at(0) == 0x00 ? 1 : 0;
                    const uint32_t LengthOfSignDataWithoutPadding = l4sign.r_length_token
                                                                  - StartOffsetOfSign;
                    l4_sign_r_data_without_padding_redundant_size =
                        std::span<uint8_t>(l4sign.r_value)
                            .subspan(StartOffsetOfSign, LengthOfSignDataWithoutPadding);
                }
                std::span<uint8_t> l4_sign_s_data_without_padding_redundant_size;
                {  // s value
                    // if have padding zero, the start offset should add 1
                    const uint32_t StartOffsetOfSign = l4sign.s_value.at(0) == 0x00 ? 1 : 0;
                    const uint32_t LengthOfSignDataWithoutPadding = l4sign.s_length_token
                                                                  - StartOffsetOfSign;
                    l4_sign_s_data_without_padding_redundant_size =
                        std::span<uint8_t>(l4sign.s_value)
                            .subspan(StartOffsetOfSign, LengthOfSignDataWithoutPadding);
                }
                auto program_efuse = [&](const std::array<uint32_t, FuseArrSizeForSinature>&
                                                                   EfuseAddrArray,
                                         const std::span<uint8_t>& data) {
                    auto signature_iter = data.rbegin();
                    for (const uint32_t EfuseAddr : std::ranges::reverse_view(EfuseAddrArray)) {
                        // coverity[parameter_hidden] - This is no hidden
                        uint32_t data{};
                        data                               = 0;
                        std::array<uint8_t, 4>& data_array = *std::bit_cast<
                            std::array<uint8_t, sizeof(decltype(data))>*>(&data);
                        for (uint8_t& data_it : std::ranges::reverse_view(data_array)) {
                            data_it = (signature_iter
                                       != l4_sign_r_data_without_padding_redundant_size.rend())
                                        ? *signature_iter++
                                        : static_cast<uint8_t>(0x00);
                        }
                        // program the data into efuse
                        if (nv::flash::Flash::program_efuse(EfuseAddr, data)
                            != nv::flash::Status::Ok) {
                            vtx.data[1] = OtpL4SignatureProgrammedFail;
                            return false;
                        }
                        uint32_t read_data = 0;
                        if (nv::flash::Flash::read_efuse(EfuseAddr, read_data)
                            != nv::flash::Status::Ok) {
                            vtx.data[1] = OtpL4SignatureReadFail;
                            return false;
                        }
                        if (data != read_data) {
                            vtx.data[1] = OtpL4SignatureProgrammedCheckFail;
                            return false;
                        }
                    }
                    return true;
                };
                // start to program the signature into efuse
                if (!program_efuse(SignatureEfuseAddrR,
                                   l4_sign_r_data_without_padding_redundant_size)
                    || !program_efuse(SignatureEfuseAddrS,
                                      l4_sign_s_data_without_padding_redundant_size)) {
                    break;
                }

            } break;

            default:
                // no match any type, return error code.
                vtx.data[1] = InvalidRequestType;
                break;
        }
        if (vtx.data[1] != ProgramSuccess) {
            vtx.completion_code = Ccode::ErrorGeneral;
        }
        return;
    }
}
void Vendor::on_get_gpio_status(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vtx           = VendorPktRes::from(tx);
    auto& vrx           = VendorPktReq::from(rx);
    vtx.completion_code = Ccode::Success;

    // check version
    if (vtx.msg_version == 0x01) {
        /* version */
        vtx.data[0] = 1;

        auto gpio_port = vrx.data[0];
        auto gpio_pin  = vrx.data[1];

        bool is_found = false;
        if (!ipc::GpioSetup.empty()) {
            for (auto& [port, pin] : ipc::GpioSetup) {
                if (gpio_port == port && gpio_pin == pin) {
                    is_found = true;
                }
            }
        }

        if (is_found == false) {
            vtx.completion_code = Ccode::ErrorInvalidData;
            return;
        }

        uint8_t data{};
        auto    gpio_status = gpio::Driver::read(gpio_port, gpio_pin, data);
        if (gpio_status != gpio::Status::Ok) {
            vtx.completion_code = Ccode::ErrorGeneral;
            return;
        }
        vtx.data[1] = data;

        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1 + 1;
    }
    else if (vtx.msg_version == 0x02) {
        auto operation    = vrx.data[0];
        auto gpio_port    = vrx.data[1];
        auto gpio_pin     = vrx.data[2];
        auto write_status = vrx.data[3];

        // NOLINTNEXTLINE
        bool is_found = false;
        if (!ipc::GpioSetup.empty()) {
            for (auto& [port, pin] : ipc::GpioSetup) {
                if (gpio_port == port && gpio_pin == pin) {
                    is_found = true;
                }
            }
        }

        if (is_found == false) {
            vtx.completion_code = Ccode::ErrorInvalidData;
            return;
        }
        else {
            if (operation == 1) {
                uint8_t data{};
                auto    gpio_status = gpio::Driver::read(gpio_port, gpio_pin, data);
                if (gpio_status != gpio::Status::Ok) {
                    vtx.completion_code = Ccode::ErrorGeneral;
                    return;
                }
                vtx.data[0] = 0;
                vtx.data[1] = data;

                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1 + 1;
            }
            else if (operation == 2) {
                auto gpio_status = gpio::Driver::write(gpio_port, gpio_pin, write_status);
                if (gpio_status != gpio::Status::Ok) {
                    vtx.completion_code = Ccode::ErrorGeneral;
                    return;
                }

                vtx.data[0]           = 0;
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
            }
            else {
                vtx.data[0]           = 1;
                vtx.completion_code   = Ccode::ErrorInvalidData;
                tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;
                return;
            }
        }
    }
    else {
        vtx.completion_code = Ccode::ErrorUnsupportedCmd;
        return;
    }
}

void Vendor::on_download_log(const Packet& rx, Packet& tx) const
{
    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vtx = VendorPktRes::from(tx);
    auto& vrx = VendorPktReq::from(rx);

    // check version
    if (vtx.msg_version != 0x01) {
        vtx.completion_code = Ccode::ErrorUnsupportedCmd;
        return;
    }

    nv::logger::Dlreq req{};
    auto              cur_session = vrx.data[0];
    req.session                   = cur_session;

    constexpr uint32_t LogEntryPerVdm = 3;

    for (uint32_t i = 0; i < LogEntryPerVdm; i++) {
        auto status = nv::logger::Logger::download(req);
        if (status != nv::logger::Status::Ok) {
            vtx.completion_code   = Ccode::ErrorInvalidData;
            tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse;
        }
        if (req.size == 0) {
            break;
        }
        memcpy(&vtx.data[2] + vtx.data[1], req.data.data(), req.size);
        vtx.data[1] += req.size;
    }
    vtx.data[0]           = req.session;
    vtx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 2 + vtx.data[1];
}

bool Vendor::on_register_table_access(const Packet& rx, Packet& tx)
{
    if constexpr (ubs::features::reg_table) {
        fill_packet_header(rx, tx);
        fill_vendor_msg_header(rx, tx);
        _regtable.on_register_table_access(rx, tx);
    }
    else {
        unsupported_command(rx, tx);
        return false;
    }
    return true;
}

void Vendor::on_install_dbgtoken(const Packet& rx, Packet& tx) const
{
    if constexpr (nv::ipc::DebugTokenEnabled) {
        fill_packet_header(rx, tx);
        fill_vendor_msg_header(rx, tx);
        auto& vtx             = VendorPktRes::from(tx);
        auto& vrx             = VendorPktReq::from(rx);
        vtx.completion_code   = Ccode::Success;
        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;

        // check version
        if (vtx.msg_version != 0x01) {
            vtx.completion_code = Ccode::ErrorUnsupportedCmd;
            return;
        }

        // check message size
        if (rx.priv.packet_length < (MultipacketPayloadOffset + debugtoken::PayloadSize)) {
            vtx.completion_code = Ccode::ErrorInvalidData;
            return;
        }

        // authenticate and install dbgToken
        auto status = debugtoken::install_dbg_token(
            std::span<const uint8_t, debugtoken::PayloadSize>(
                static_cast<const uint8_t*>(vrx.data), debugtoken::PayloadSize));

        if (status != debugtoken::Status::Success) {
            vtx.completion_code = Ccode::ErrorGeneral;
        }

        // send error code in response
        vtx.data[0] = static_cast<uint8_t>(status);
    }
    else {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
    }
}

void Vendor::on_erase_dbgtoken(const Packet& rx, Packet& tx) const
{
    if constexpr (nv::ipc::DebugTokenEnabled) {
        fill_packet_header(rx, tx);
        fill_vendor_msg_header(rx, tx);
        auto& vtx = VendorPktRes::from(tx);

        vtx.completion_code   = Ccode::Success;
        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 1;

        // check version
        if (vtx.msg_version != 0x01) {
            vtx.completion_code = Ccode::ErrorUnsupportedCmd;
            return;
        }

        // erase debug token
        auto status = debugtoken::erase_installed_dbg_token();
        if (status != debugtoken::Status::Success) {
            vtx.completion_code = Ccode::ErrorGeneral;
        }

        // send error code in response
        vtx.data[0] = static_cast<uint8_t>(status);
    }
    else {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
    }
}

void Vendor::on_query_dbgtoken_status(const Packet& rx, Packet& tx) const
{
    // Debug Token Query v3 (Command Code 0x0F)
    if constexpr (nv::ipc::DebugTokenEnabled) {
        fill_packet_header(rx, tx);
        fill_vendor_msg_header(rx, tx);
        auto& vtx = VendorPktRes::from(tx);

        constexpr uint8_t Shift24 = 24;
        constexpr uint8_t Shift16 = 16;
        constexpr uint8_t Shift8  = 8;

        constexpr uint8_t ResponseDataVersion = 0x01;

        constexpr uint8_t ResponseVersionIndex = 0;
        constexpr uint8_t DeviceTypeHighIndex  = 1;
        constexpr uint8_t DeviceTypeLowIndex   = 2;
        constexpr uint8_t TokenStatusIndex     = 3;
        constexpr uint8_t SerialNumIndex       = 4;
        constexpr uint8_t McufwStatusIndex     = 20;
        constexpr uint8_t TokenTypeIndex       = 21;
        constexpr uint8_t TotalResponseBytes   = 41;

        const auto& uuid = _ctl.router().ec.uuid;

        vtx.completion_code = Ccode::Success;

        // check version
        if (vtx.msg_version != 0x03) {  // Debug Token Query v3
            vtx.completion_code = Ccode::ErrorUnsupportedCmd;
            return;
        }

        // check for the presence of debug token in spi flash
        bool isTokenInstalled = false;
        isTokenInstalled      = debugtoken::is_dbg_token_in_flash();

        // Set response data
        vtx.data[ResponseVersionIndex] = ResponseDataVersion;
        vtx.data[DeviceTypeHighIndex]  = (debugtoken::MCU_ENDPOINT_ID >> 8) & UINT8_MAX;
        vtx.data[DeviceTypeLowIndex]   = debugtoken::MCU_ENDPOINT_ID & UINT8_MAX;
        vtx.data[TokenStatusIndex]     = (isTokenInstalled) ? UINT8_C(1) : UINT8_C(0);

        memcpy(&vtx.data[SerialNumIndex], &uuid, DeviceSerialNumSize);

        // Check whether the system is Debug fused or Production fused.
        // The debug key index is 0.
        // If the debug key index is revoked, the running MCUFW is production FW,
        // and the MCU is considered to be in production mode; otherwise, it is in debug mode.
        constexpr uint32_t KeyRevokeOffset = 0x18;

        std::array<uint8_t, 4> buffer = {};
        const ipc::Queue::Item Item(buffer.begin(), buffer.begin() + 4);

        if (nv::flash::Flash::read_cfpa(Item, KeyRevokeOffset) != nv::flash::Status::Ok) {
            vtx.completion_code = Ccode::ErrorGeneral;
            return;
        }

        uint32_t key_permission = 0;
        memcpy(&key_permission, buffer.data(), sizeof(key_permission));

        // Check if the first bit of key_permission is revoked
        const uint8_t isFirstBitRevoked = (key_permission & 0x1) != 0;

        vtx.data[McufwStatusIndex] = isFirstBitRevoked ? debugtoken::McufwProdMode
                                                       : debugtoken::McufwDebugMode;

        // Set token types (big endian format)
        const debugtoken::Type token_type = debugtoken::get_token_type();
        vtx.data[TokenTypeIndex] = (static_cast<uint32_t>(token_type) >> Shift24) & UINT8_MAX;
        vtx.data[TokenTypeIndex + 1] = (static_cast<uint32_t>(token_type) >> Shift16)
                                     & UINT8_MAX;
        vtx.data[TokenTypeIndex + 2] = (static_cast<uint32_t>(token_type) >> Shift8)
                                     & UINT8_MAX;
        vtx.data[TokenTypeIndex + 3] = static_cast<uint32_t>(token_type) & UINT8_MAX;

        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + TotalResponseBytes;
    }
    else {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
    }
}

/**
 * Add external timestamp(epoch time with uint64_t format) to log
 *
 * @param[in] rx  MCTP RX buffer
 * @param[in] tx  MCTP TX buffer
 * @return void
 */
void Vendor::on_add_ext_timestamp(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_vendor_msg_header(rx, tx);
    auto& vrx           = VendorPktReq::from(rx);
    auto& vtx           = VendorPktRes::from(tx);
    vtx.completion_code = Ccode::Success;

    // check version
    if (vtx.msg_version != 0x01) {
        vtx.completion_code = Ccode::ErrorUnsupportedCmd;
        return;
    }

    // Add timestamp to log
    logger::EventData timestamp = {0};

    // BE to LE
    memcpy(&timestamp, &vrx.data[0], sizeof(uint64_t));
    std::reverse(timestamp.begin(), timestamp.end());

    // Logging external timestamp
    auto status = logger::info(logger::Event::AddExtTimestamp, timestamp);
    if (status != logger::Status::Ok) {
        vtx.completion_code = Ccode::ErrorGeneral;
        return;
    }

    // Store timestamp to PDS
    uint32_t       timestamp_LSB  = 0;  // First 4 bytes (Least Significant Bytes)
    uint32_t       timestamp_MSB  = 0;  // Last 4 bytes (Most Significant Bytes)
    const uint32_t timestamp_tick = sys::ipc::get_os_ticks();
    memcpy(&timestamp_LSB, &timestamp[0], sizeof(uint32_t));
    memcpy(&timestamp_MSB, &timestamp[4], sizeof(uint32_t));

    auto npds_status = nv::flash::Flash::set_data(nv::flash::Key::NpdsExtTimestampLSB,
                                                  timestamp_LSB);
    if (npds_status != nv::flash::Status::Ok) {
        vtx.completion_code = Ccode::ErrorGeneral;
        return;
    }
    npds_status = nv::flash::Flash::set_data(nv::flash::Key::NpdsExtTimestampMSB,
                                             timestamp_MSB);
    if (npds_status != nv::flash::Status::Ok) {
        vtx.completion_code = Ccode::ErrorGeneral;
        return;
    }
    npds_status = nv::flash::Flash::set_data(nv::flash::Key::NpdsExtTimestampTick,
                                             timestamp_tick);
    if (npds_status != nv::flash::Status::Ok) {
        vtx.completion_code = Ccode::ErrorGeneral;
        return;
    }
}

void Vendor::action_background_copy(const Packet& rx, Packet& tx) const
{
    auto& vrx = VendorPktReq::from(rx);
    auto& vtx = VendorPktRes::from(tx);

    if (vtx.completion_code != Ccode::Success) {
        return;
    }

    if (vrx.data[0] == static_cast<uint8_t>(BackgroundCopyCmd::Init_Bg_Update)) {
        flash::Data         pending_bg_copy{};
        const flash::Status flash_status = flash::Flash::get_data(
            flash::Key::NpdsAllowInitBackgroundCopy, pending_bg_copy);
        if (flash_status != flash::Status::Ok) {
            return;
        }

        if (pending_bg_copy != 0) {
            /* trigger BG */
            pldm::Task::pldm_bg_init();
        }
    }
}

void Vendor::on_scan_i2c(const Packet& rx, Packet& tx) const
{
    if constexpr (nv::ipc::EnableI2cScanVdm) {
        fill_packet_header(rx, tx);
        fill_vendor_msg_header(rx, tx);
        auto& vtx           = VendorPktRes::from(tx);
        auto& vrx           = VendorPktReq::from(rx);
        vtx.completion_code = Ccode::Success;
        nv::i2c::Port port  = nv::i2c::Port::Zero;

        // Request format:
        // Byte 0: Version (0x01)
        // Byte 1: I2C port (0x00-0x07)

        // Check version
        if (vrx.msg_version != 0x01) {
            vtx.completion_code = Ccode::ErrorUnsupportedCmd;
            return;
        }

        // check port boundary
        const uint8_t port_num = vrx.data[1];
        if (port_num < static_cast<uint8_t>(nv::i2c::Port::End)) {
            port = static_cast<nv::i2c::Port>(port_num);
        }
        else {
            vtx.completion_code = Ccode::ErrorInvalidData;
            return;
        }

        // verify master is enabled and slave is disabled
        if (!sys::i2c::is_master_enabled(port) || sys::i2c::is_slave_enabled(port)) {
            vtx.completion_code = Ccode::ErrorGeneral;
            return;
        }

        // Response format:
        // Byte 0: Version (0x01)
        // Byte 1: Number of devices found
        // Bytes 2-17: Bitmap of found devices (128 bits = 16 bytes for addresses 0-127)
        vtx.data[0] = 0x01;  // Version
        vtx.data[1] = 0x00;  // Number of devices (to be filled)

        // Initialize bitmap to all zeros
        constexpr uint8_t        BitmapSize = 16;  // 128 bits = 16 bytes
        const std::span<uint8_t> bitmap(static_cast<uint8_t*>(vtx.data) + 2, BitmapSize);
        std::fill(bitmap.begin(), bitmap.end(), 0);

        // Scan I2C bus for responsive devices
        for (uint8_t addr = 1; addr < 128; addr++) {
            auto status = sys::i2c::i2c_write(port, addr, {});
            if (status == nv::i2c::I2cStatus::Ok) {
                // Set bit in bitmap for found device
                const uint8_t byte_index  = addr / 8;  // Which byte in the bitmap
                const uint8_t bit_index   = addr % 8;  // Which bit in that byte
                bitmap[byte_index]       |= (1U << bit_index);
                if (vtx.data[1] < 128) {
                    vtx.data[1]++;  // Increment device count
                }
            }
        }

        // Set response length: header + version byte + count byte + bitmap
        tx.priv.packet_length = sizeof(Header) + HeaderSizeResponse + 2 + BitmapSize;
    }
    else {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
    }
}
