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
#include "nv/i2c/helper.h"
#include "nv/mainbox/mailbox.h"
#include "nv/logger/log.h"
#include "sys/i2c/utils.h"
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
                                                    PlatformInfo       platform_info,
                                                    NVL_topology_info& out)
{
    out.NODE_INDEX   = platform_info.node_index;
    out.DEVICE_INDEX = platform_info.module_id;
    if (port == nv::i2c::Port::End) {
        // No FRU case
        return true;
    }
    else {
        // FRU case
        nv::fru::ChassisInfo chassis_info{};
        auto i2c_status = nv::fru::get_chassis_info(port, eeprom_addr, chassis_info);
        if (i2c_status != nv::fru::Status::FruSuccess) {
            return false;
        }

        SnBuffer sn_data = {0};
        nv::mainbox::read_mailbox(nv::mainbox::MainBoxMemoryType::BoardSerialNumber, sn_data);
        // remove CRC8
        memcpy(&out.RACK_GUID, sn_data.data(), sizeof(out.RACK_GUID));
#if 0
        i2c_status = nv::fru::get_board_info(port, eeprom_addr, board_info);
        if (i2c_status != nv::fru::Status::FruSuccess) {
            return false;
        }
        memcpy(&out.RACK_GUID, &board_info.serial, sizeof(out.RACK_GUID));
#endif
        fill_customer_fields(chassis_info, out);
        return true;
    }
}

bool TopologyInfo::is_sn_in_mailbox()
{
    using namespace nv::logger;
    using namespace nv::mainbox;
    SnBuffer sn = {0};
    read_mailbox(MainBoxMemoryType::BoardSerialNumber, sn);
    if (!is_sn_valid(sn)) {
        return false;
    }
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    const EventData sn_0 = {static_cast<uint8_t>(SnSyncStatus::MailBoxSerial),
                            sn[0],
                            sn[1],
                            sn[2],
                            sn[3],
                            sn[4],
                            sn[5],
                            sn[6]};
    const EventData sn_1 = {static_cast<uint8_t>(SnSyncStatus::MailBoxSerial),
                            sn[7],
                            sn[8],
                            sn[9],
                            sn[10],
                            sn[11],
                            sn[12],
                            sn[13]};  // CRC8
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    info(Event::NvlSnSync, sn_0);
    info(Event::NvlSnSync, sn_1);
    return true;
}

void sys::topology::TopologyInfo::update_sn()
{
    using namespace nv::logger;
    using namespace nv::mainbox;
    using namespace nv::i2c;
    using namespace sys::i2c;
    reset_sn();
    SnBuffer               sn          = {0};
    auto                   i2c_status  = I2cStatus::Ok;
    auto                   read_status = ReadSNStatus::Success;
    uint8_t                retry       = 0;
    std::array<uint8_t, 1> command     = {static_cast<uint8_t>(SnSyncCommand::SerialNumber)};
    // retry to read SN from target
    for (; retry < MaxRetry; retry++) {
        i2c_status = i2c_write_read(TargetPort, TargetAddress, command, sn);
        if (i2c_status == I2cStatus::Ok) {
            if (is_sn_valid(sn)) {
                read_status = ReadSNStatus::Success;
                break;
            }
            else {
                read_status = ReadSNStatus::PECInvalid;
            }
        }
        else {
            read_status = ReadSNStatus::ReadFailed;
        }
        nv::ctimer::Driver::delay_for_us(RetryUs);  // 10ms
    }
    info(Event::NvlSnSync,
         {static_cast<uint8_t>(read_status), retry, static_cast<uint8_t>(i2c_status)});
    if (read_status != ReadSNStatus::Success) {
        return;
    }
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    const EventData sn_0 = {static_cast<uint8_t>(SnSyncStatus::ReadBoardValue0),
                            sn[0],
                            sn[1],
                            sn[2],
                            sn[3],
                            sn[4],
                            sn[5],
                            sn[6]};
    const EventData sn_1 = {static_cast<uint8_t>(SnSyncStatus::ReadBoardValue1),
                            sn[7],
                            sn[8],
                            sn[9],
                            sn[10],
                            sn[11],
                            sn[12],
                            sn[13]};  // CRC8
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    info(Event::NvlSnSync, sn_0);
    info(Event::NvlSnSync, sn_1);
    write_mailbox(MainBoxMemoryType::BoardSerialNumber, sn);
}

bool TopologyInfo::is_sn_valid(std::span<uint8_t> sn)
{
    auto payload     = sn.subspan(0, sn.size() - 1);
    auto stored_crc8 = sn.back();
    return nv::i2c::crc8(payload) == stored_crc8;
}

void TopologyInfo::append_crc8(std::span<uint8_t> sn)
{
    auto crc8 = nv::i2c::crc8(sn.subspan(0, sn.size() - 1));
    sn.back() = crc8;
}

void TopologyInfo::reset_sn()
{
    using namespace nv::mainbox;
    SnBuffer sn = {0};
    std::fill(sn.begin(), sn.end(), SnResetValue);
    append_crc8(sn);
    write_mailbox(MainBoxMemoryType::BoardSerialNumber, sn);
}
