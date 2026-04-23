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

#include <cstdint>
#include <cstring>

#include "nv/mctp/enums.h"
#include "nv/mctp/nsm.h"

using namespace nv;
using namespace mctp;

namespace nv::mctp {

namespace nsm_type2 {

bool validatePcieLinkResetValue(uint8_t device_index)
{
    for (const auto& valid : nv::mctp::pciLinkValidDevices) {
        if (device_index == valid) {
            return true;
        }
    }
    return false;
}

}  // namespace nsm_type2

namespace nsm_type2 {

__attribute__((weak)) NsmStatus platform_assert_pcie_fundamental_reset(uint8_t device_index,
                                                                       uint8_t action)
{
    (void)device_index;
    (void)action;
    return NsmStatus::NotSupported;
}

}  // namespace nsm_type2

bool Nsm::process_pci_links(const Packet& rx, Packet& tx)
{
    using cmd = nv::mctp::NsmPciLinksCmdCode;
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    ntx.nv_msg_type = nrx.nv_msg_type;
    ntx.set_pci_links_code(nrx.get_pci_links_code());

    // Helper to handle unsupported commands
    [[maybe_unused]] auto unsupported_command = [&]() {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return false;
    };

    switch (nrx.get_pci_links_code()) {
        case cmd::AssertPcieFundamentalReset:
            if constexpr (is_cmd_set(NsmMsgType::PciLinks, cmd::AssertPcieFundamentalReset)) {
                on_pci_links_assert_pcie_fundamental_reset(rx, tx);
                break;
            }
            return unsupported_command();
        default: return unsupported_command();
    }

    return true;
}

void Nsm::on_pci_links_assert_pcie_fundamental_reset(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    if (!is_input_length_valid(rx, sizeof(T2PciLinkPcieResetRequest))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    T2PciLinkPcieResetRequest request{};

    // check request data
    std::memcpy(&request.device_index, &nrx.data[0], sizeof(T2PciLinkPcieResetRequest));
    if (false == nsm_type2::validatePcieLinkResetValue(request.device_index)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    if (request.action != static_cast<uint8_t>(PciLinkPcieResetAction::None)
        && request.action != static_cast<uint8_t>(PciLinkPcieResetAction::Reset)) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto status = nsm_type2::platform_assert_pcie_fundamental_reset(request.device_index,
                                                                    request.action);
    if (status == NsmStatus::I2cBusError) {
        fill_error_packet(Ccode::ErrorI2CError, rx, tx);
        return;
    }
    else if (status == NsmStatus::NotSupported) {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return;
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

}  // namespace nv::mctp
