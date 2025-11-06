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
#include <cstdint>
#include "sys/i2c/i2c.h"

namespace sys::topology {

class TopologyInfo
{
public:
    enum class CbcIndex : uint8_t
    {
        Revesion   = 0,
        SlotNumber = 2,
        TrayIndex  = 3,
        TopologyId = 4,
    };
    struct [[gnu::packed]] RACK_GUID_t
    {
        uint32_t vendor_id : 24;
        uint16_t work_week_build;
        uint16_t year_build;
        uint8_t  serial_number[6];
    };

    struct [[gnu::packed]] NVL_topology_info
    {
        uint8_t     SR_PORTS_CPLD_VER;
        uint8_t     TRAY_TYPE;
        RACK_GUID_t RACK_GUID;
        uint8_t     TOPOLOGY_ID_TYPE;
        uint8_t     CHASSIS_PHYSICAL_SLOT_NUMBER;
        uint8_t     COMPUTE_SLOT_INDEX;     // AKA Tray Index
        uint8_t     NODE_INDEX        : 4;  // AKA Module ID
        uint8_t     DEVICE_INDEX      : 4;
        uint8_t     NVLINK_STATUS_LED : 4;
        uint8_t     PCB_REVISION      : 4;

        std::span<uint8_t> to_span() const
        {
            return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
        }
    };

    static_assert(sizeof(RACK_GUID_t) == 13, "RACK_GUID size is not 13");
    static_assert(sizeof(NVL_topology_info) == 20, "NVL_topology_info size is not 20");
    static bool
    get_topology_info(nv::i2c::Port port, uint8_t eeprom_addr, NVL_topology_info& out);
};

}  // namespace sys::topology
