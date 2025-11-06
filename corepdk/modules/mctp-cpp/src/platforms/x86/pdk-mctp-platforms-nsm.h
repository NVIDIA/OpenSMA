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
#pragma once
#include <bit>
#include <span>

#include "app/pdk-mctp-app-enums.h"

namespace pdk::mctp::platforms {
struct [[gnu::packed]] NsmPktReq : app::TransportHeader
{
    /* nvidia OEM binding */
    app::MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    union CmdCode
    {
        NsmDcdCmdCode     dcd_code;
        NsmFWCmdCode      fw_code;
        NsmDevCfgCmdCode  devcfg_code;
        NsmPlatEnvCmdCode plat_env_code;
        NsmDevDiagCmdCode devdiag_code;
    } cmd_code;

    uint8_t data_size;
    uint8_t data[1];

    static NsmPktReq& from(app::Packet& buf) { return *std::bit_cast<NsmPktReq*>(&buf.hdr); }

    static const NsmPktReq& from(const app::Packet& buf)
    {
        return *std::bit_cast<const NsmPktReq*>(&buf.hdr);
    }

    static NsmPktReq& from(NsmPacket& buf) { return *std::bit_cast<NsmPktReq*>(&buf.hdr); }

    static const NsmPktReq& from(const NsmPacket& buf)
    {
        return *std::bit_cast<const NsmPktReq*>(&buf.hdr);
    }

    NsmDcdCmdCode get_dcd_code() const { return cmd_code.dcd_code; }
    void          set_dcd_code(NsmDcdCmdCode code) { cmd_code.dcd_code = code; }

    NsmFWCmdCode get_fw_code() const { return cmd_code.fw_code; }
    void         set_fw_code(NsmFWCmdCode code) { cmd_code.fw_code = code; }

    NsmDevCfgCmdCode get_dev_cfg_code() const { return cmd_code.devcfg_code; }
    void             set_dev_cfg_code(NsmDevCfgCmdCode code) { cmd_code.devcfg_code = code; }

    NsmPlatEnvCmdCode get_plat_env_code() const { return cmd_code.plat_env_code; }
    void set_plat_env_code(NsmPlatEnvCmdCode code) { cmd_code.plat_env_code = code; }

    NsmDevDiagCmdCode get_dev_diag_code() const { return cmd_code.devdiag_code; }
    void set_dev_diag_code(NsmDevDiagCmdCode code) { cmd_code.devdiag_code = code; }
};
constexpr uint16_t NvMctpPciVendorId = 0xDE10;  // MCTP header is big endian
}  // namespace pdk::mctp::platforms
