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

#include <array>
#include <climits>
#include <cstdint>
#include <tuple>

#include "nv/mctp/constants.h"
#include "nv/mctp/enums.h"

namespace nv::mctp {

enum PciLinksResetDevices : uint8_t
{
    AllNvswPcieReset = 0x4F,
    PexswPcieReset   = 0x50,
    Cx9PcieReset_0   = 0x60,
    Cx9PcieReset_1   = 0x61,
    Cx9PcieReset_2   = 0x62,
    Cx9PcieReset_3   = 0x63,
    Cx9PcieReset_4   = 0x64,
    Cx9PcieReset_5   = 0x65,
    Cx9PcieReset_6   = 0x66,
    Cx9PcieReset_7   = 0x67,
    AllCx9PcieReset  = 0x70,
};

constexpr auto PciLinkPcieResetValidDevicesSize = 11;

/** Array intended to check valid Device Index
 *  for T2 PciLinks Assert PCIe Fundamental reset
 */
constexpr inline std::array<uint8_t, PciLinkPcieResetValidDevicesSize> pciLinkValidDevices{
    AllNvswPcieReset,
    PexswPcieReset,
    Cx9PcieReset_0,
    Cx9PcieReset_1,
    Cx9PcieReset_2,
    Cx9PcieReset_3,
    Cx9PcieReset_4,
    Cx9PcieReset_5,
    Cx9PcieReset_6,
    Cx9PcieReset_7,
    AllCx9PcieReset,
};

static_assert(PciLinkPcieResetValidDevicesSize == pciLinkValidDevices.size());

enum class PciLinkPcieResetAction : uint8_t
{
    None  = 0,
    Reset = 1,
};

struct [[gnu::packed]] T2PciLinkPcieResetRequest
{
    uint8_t device_index;
    uint8_t action;
    T2PciLinkPcieResetRequest() : device_index{0}, action{0}
    {
        // Empty
    }
};

namespace nsm_type2 {

bool validatePcieLinkResetValue(uint8_t device_index);

/**
 * @brief Platform hook for Assert PCIe Fundamental Reset via CPLD.
 * Weak default returns NotSupported. Strong override in p7612_hgx core0/main.cpp.
 *
 * @return NsmStatus indicating result of CPLD register update.
 */
NsmStatus platform_assert_pcie_fundamental_reset(uint8_t device_index, uint8_t action);

}  // namespace nsm_type2

}  // namespace nv::mctp
