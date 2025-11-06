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
#include <concepts>
#include <type_traits>

#include "nv/mctp/control.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/interface.h"
#include "nv/reg_table/hooks.h"
#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-vendor.h"

namespace nv::mctp {

// offset from the start of mctp multipacket VDM message to payload
constexpr uint8_t MultipacketPayloadOffset = 13;  // sub private header size
constexpr uint8_t DeviceSerialNumSize      = 16;

class Vendor : public pdk::mctp::app::Vendor
{
public:
    Vendor(Control& ctl) : pdk::mctp::app::Vendor(ctl) {}
    bool process(const Packet& rx, Packet& tx);
    bool action(const Packet& rx, Packet& tx) const;

protected:
    void on_boot_complete(const Packet& rx, Packet& tx) const;
    void on_query_boot_status(const Packet& rx, Packet& tx) const;
    void on_download_log(const Packet& rx, Packet& tx) const;
    void on_get_gpio_status(const Packet& rx, Packet& tx) const;
    void on_self_test(const Packet& rx, Packet& tx) const;
    void on_background_copy(const Packet& rx, Packet& tx) const;
    void on_read_devik_csr(const Packet& rx, Packet& tx) const;
    void on_program_certificate(const Packet& rx, Packet& tx) const;
    void on_install_dbgtoken(const Packet& rx, Packet& tx) const;
    void on_erase_dbgtoken(const Packet& rx, Packet& tx) const;
    void on_query_dbgtoken_status(const Packet& rx, Packet& tx) const;
    bool on_register_table_access(const Packet& rx, Packet& tx);
    void on_add_ext_timestamp(const Packet& rx, Packet& tx);
    void on_scan_i2c(const Packet& rx, Packet& tx) const;
    void on_download_coverage(const Packet& rx, Packet& tx) const;
    void action_background_copy(const Packet& rx, Packet& tx) const;

    [[no_unique_address]] reg_table::Hooks _regtable{};
    friend class reg_table::Hooks;
};

using VendorPktReq = pdk::mctp::app::VendorPktReq;
using VendorPktRes = pdk::mctp::app::VendorPktRes;
}  // namespace nv::mctp
