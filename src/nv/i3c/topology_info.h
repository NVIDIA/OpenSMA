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
    // I2C CBC SN Sync
    constexpr static nv::i2c::Port TargetPort = nv::i2c::Port::Two;

    constexpr static uint8_t  TargetAddress = 0x35;
    constexpr static uint32_t SnSize        = 14;  // 13 bytes + 1 byte CRC8
    constexpr static uint8_t  SnResetValue  = 0x30;
    constexpr static uint8_t  MaxRetry      = 32;
    constexpr static uint32_t RetryUs       = 10000;  // 10ms

    typedef std::array<uint8_t, SnSize> SnBuffer;

    enum class SnSyncCommand : uint8_t
    {
        SerialNumber = 0,
        ThermWarning = 1,
        Invalid      = 0xff
    };

    enum class SnSyncStatus : uint8_t
    {
        Success,
        ReadSNFailed,
        TargetStartFailed,
        ReadBoardValue0,
        ReadBoardValue1,
        ReadBoardSnSts,
        MailBoxMagicExist,
        MailBoxSerial
    };

    enum class ReadSNStatus : uint8_t
    {
        Success,
        PECInvalid,
        ReadFailed,
    };

    enum class CbcIndex : uint8_t
    {
        Revesion   = 0,
        SlotNumber = 2,
        TrayIndex  = 3,
        TopologyId = 4,
    };
    struct [[gnu::packed]] RACK_GUID_t
    {
        uint8_t serial_number[SnSize - 1];
    };

    struct [[gnu::packed]] NVL_topology_info
    {
        uint8_t     SR_PORTS_CPLD_VER            = 0;
        uint8_t     TRAY_TYPE                    = 0xFF;
        RACK_GUID_t RACK_GUID                    = {0};
        uint8_t     TOPOLOGY_ID_TYPE             = 0x7F;
        uint8_t     CHASSIS_PHYSICAL_SLOT_NUMBER = 0xFF;
        uint8_t     COMPUTE_SLOT_INDEX           = 0xFF;
        uint8_t     NODE_INDEX        : 4        = 0x0;
        uint8_t     DEVICE_INDEX      : 4        = 0x0;
        uint8_t     NVLINK_STATUS_LED : 4        = 0x0;
        uint8_t     PCB_REVISION      : 4        = 0xF;

        std::span<uint8_t> to_span() const
        {
            return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
        }
    };

    struct PlatformInfo
    {
        uint8_t node_index;
        uint8_t module_id;
        bool    nvs_present;
        uint8_t nvs_present_port;
        uint8_t nvs_present_pin;
    };

    static_assert(sizeof(RACK_GUID_t) == 13, "RACK_GUID size is not 13");
    static_assert(sizeof(NVL_topology_info) == 20, "NVL_topology_info size is not 20");

    static bool get_topology_info(nv::i2c::Port      port,
                                  uint8_t            eeprom_addr,
                                  PlatformInfo       platform_info,
                                  NVL_topology_info& out);
    static bool is_sn_in_mailbox();
    static bool is_sn_valid(std::span<uint8_t> sn);
    static void append_crc8(std::span<uint8_t> sn);
    static void update_sn();
    static void reset_sn();
};

}  // namespace sys::topology
