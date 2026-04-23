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

#include "nv/usb/i2c_backend.h"

#include "nv/i2c/common.h"
#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/iox/task.h"
#include "nv/ipc/queue.h"
#include "nv/debugtoken/debugtoken.h"

namespace nv::usb {

std::pair<ipchandler::Id, uint8_t> i2c_addr_mapping(const uint8_t virtual_addr)
{
    for (const ipc::I2cVirtualAddressMappingTableItem mapping_item :
         ipc::I2cVirtualAddressMappingTable) {
        if (mapping_item.virtual_address == virtual_addr) {
            if (mapping_item.need_debug_token) {
                if (!nv::debugtoken::is_debug_token_subtype_enabled_cached(
                        nv::debugtoken::Type::McuDebug,
                        nv::debugtoken::DebugTokenSubtypePwrFailI2cDebug)) {
                    return std::make_pair(ipchandler::Id::Unuse, virtual_addr);
                }
            }
            if (mapping_item.dynamic_address_type
                == ipc::I2cDynamicAddressType::NotDynamicType) {
                return std::make_pair(mapping_item.ipchandler_id,
                                      mapping_item.physical_address);
            }
            if constexpr (std::to_underlying(ipc::I2cDynamicAddressType::End)
                              - std::to_underlying(ipc::I2cDynamicAddressType::Begin)
                          != 1) {
                auto dynamic_address = ipc::find_i2c_dynamic_virtual_address(
                    mapping_item.dynamic_address_type);
                return std::make_pair(mapping_item.ipchandler_id,
                                      dynamic_address.value_or(mapping_item.physical_address));
            }
        }
    }

    if constexpr (ipc::I2cManualNackMode == true) {
        return std::make_pair(ipchandler::Id::Unuse, virtual_addr);
    }
    else {
        return std::make_pair(ipc::I2cDefaultInhandlerId, virtual_addr);
    }
}

bool is_i2c_ocp_device(const uint8_t virtual_addr)
{
    for (const ipc::I2cVirtualAddressMappingTableItem mapping_item :
         ipc::I2cVirtualAddressMappingTable) {
        if (mapping_item.virtual_address == virtual_addr) {
            return mapping_item.is_ocp_device;
        }
    }
    return false;
}

bool is_i3c_handler(ipchandler::Id id)
{
    return (id == ipchandler::Id::I3c0 || id == ipchandler::Id::I3c1);
}

bool send_to_i2c_backend(ipchandler::Id     ipchandler_id,
                         uint8_t            physical_addr,
                         uint8_t            write_len,
                         uint16_t           read_len,
                         std::span<uint8_t> buffer)
{
    if (ipchandler_id == ipchandler::Id::Iox) {
        if constexpr (nv::ipc::EnableIoxEmulation) {
            return iox::Task::send_i2c_request(
                ipchandler::Id::Usb, physical_addr, ipchandler_id, write_len, read_len, buffer);
        }
        else {
            return false;
        }
    }
    else if (is_i3c_handler(ipchandler_id)) {
        return i3c::Task::to_i2c(
            ipchandler::Id::Usb, physical_addr, ipchandler_id, write_len, read_len, buffer);
    }
    else {
        return i2c::Task::to_i2c(ipchandler::Id::Usb,
                                 physical_addr,
                                 ipchandler_id,
                                 write_len,
                                 read_len,
                                 i2c::I2cFlags::NoFlag,
                                 buffer);
    }
}

}  // namespace nv::usb
