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
#include "nv/i3c/topology_info.h"
#include <cstring>
#include <span>
#include "nv/fru/fru.h"
using namespace sys::topology;

namespace {

void fill_customer_fields(nv::fru::ChassisInfo&            chassis_info,
                          TopologyInfo::NVL_topology_info& out)
{
    using namespace sys::topology;
    out.SR_PORTS_CPLD_VER            = chassis_info.custom_fields[0][static_cast<uint8_t>(
        TopologyInfo::CbcIndex::Revesion)];
    out.CHASSIS_PHYSICAL_SLOT_NUMBER = chassis_info.custom_fields[0][static_cast<uint8_t>(
        TopologyInfo::CbcIndex::SlotNumber)];
    out.TOPOLOGY_ID_TYPE             = chassis_info.custom_fields[0][static_cast<uint8_t>(
        TopologyInfo::CbcIndex::TopologyId)];
    out.COMPUTE_SLOT_INDEX           = chassis_info.custom_fields[0][static_cast<uint8_t>(
        TopologyInfo::CbcIndex::TrayIndex)];
}

}  // namespace

bool sys::topology::TopologyInfo::get_topology_info(nv::i2c::Port      port,
                                                    uint8_t            eeprom_addr,
                                                    NVL_topology_info& out)
{
    memset(&out, 0, sizeof(out));
    nv::fru::ChassisInfo chassis_info{};
    auto i2c_status = nv::fru::get_chassis_info(port, eeprom_addr, chassis_info);
    if (i2c_status != nv::fru::Status::FruSuccess) {
        return false;
    }

    memcpy(&out.RACK_GUID, &chassis_info.serial, sizeof(out.RACK_GUID));
    out.DEVICE_INDEX      = 0;  // TBD
    out.PCB_REVISION      = 5;  // TBD
    out.NVLINK_STATUS_LED = 0;  // TBD
    out.TRAY_TYPE         = 0;  // TBD
    fill_customer_fields(chassis_info, out);

    return true;
}