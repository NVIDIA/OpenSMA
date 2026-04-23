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

/**
 * @file i2c_backend.h
 * @brief Shared HID CP2112 I2C backend helpers for USB tasks.
 *
 * Provides virtual-to-physical I2C address mapping and I2C request routing
 * used by both the single-core HidSmb class and the dual-core usb_proxy task.
 */
#pragma once

#include <cstdint>
#include <span>
#include <utility>

#include "nv/ipchandler/ipchandler.h"

namespace nv::usb {

/**
 * @brief Map a virtual I2C address to a physical address and IPC handler.
 *
 * Looks up @p virtual_addr in the project's I2cVirtualAddressMappingTable.
 * Supports both static and dynamic address types.
 *
 * @param virtual_addr  Virtual I2C slave address (already shifted >> 1)
 * @return (ipchandler_id, physical_address) pair.
 *         Returns (Id::Unuse, virtual_addr) if not found and manual NACK is enabled.
 */
std::pair<ipchandler::Id, uint8_t> i2c_addr_mapping(uint8_t virtual_addr);

/**
 * @brief Check if a virtual I2C address maps to an OCP device.
 *
 * @param virtual_addr  Virtual I2C slave address (already shifted >> 1)
 * @return true if the device is marked as OCP in the mapping table
 */
bool is_i2c_ocp_device(uint8_t virtual_addr);

/**
 * @brief Check if an IPC handler ID corresponds to an I3C queue.
 */
bool is_i3c_handler(ipchandler::Id id);

/**
 * @brief Route an I2C request to the appropriate backend task (IOX/I3C/I2C).
 *
 * The source handler is always ipchandler::Id::Usb.
 *
 * @param ipchandler_id  Target handler ID from i2c_addr_mapping()
 * @param physical_addr  Physical I2C address from i2c_addr_mapping()
 * @param write_len      Number of bytes to write (0 for pure read)
 * @param read_len       Number of bytes to read (0 for pure write)
 * @param buffer         Data buffer for write data / read result
 * @return true if the request was sent successfully
 */
bool send_to_i2c_backend(ipchandler::Id     ipchandler_id,
                         uint8_t            physical_addr,
                         uint8_t            write_len,
                         uint16_t           read_len,
                         std::span<uint8_t> buffer);

}  // namespace nv::usb
