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

#include "nv/i2c/smb_direct.h"
#include "nv/telemetry/cache.h"
#include NV_IPC_CONFIG_H
#include <algorithm>
#include <cstring>

using namespace nv;
using namespace nv::telemetry;

bool nv::i2c::on_smbus_direct(uint8_t offset, std::span<uint8_t> buffer)
{
    if constexpr (!nv::ipc::EnableSmbDirect) {
        return false;
    }
    Cache::Table table = Cache::inst().get_table();
    if (offset < nv::smb_telemetry::SmbDirect::StartAddress
        || offset >= nv::smb_telemetry::SmbDirect::StartAddress + table.size()) {
        return false;
    }
    const size_t begin = offset - nv::smb_telemetry::SmbDirect::StartAddress;
    const size_t size  = buffer.size() > (table.size() - begin) ? table.size() - begin
                                                                : buffer.size();
    std::fill(buffer.begin(), buffer.end(), Cache::InvalidData);
    std::memcpy(&buffer[0], &table.at(begin), size);
    const int gpio_index = std::get<1>(nv::telemetry::TelemIndexMapList.at(TelemId::Gpio));
    if (gpio_index >= 0 && begin == static_cast<size_t>(gpio_index) * sizeof(TelemId)) {
        Cache::inst().clear_error(Cache::ErrorItem::I2cSensorAlert);
    }
    return true;
}

bool nv::i2c::vr_on_smbus_direct(uint8_t                  offset,
                                 std::span<uint8_t>       buffer,
                                 std::span<const uint8_t> cache)
{
    const uint8_t end_address = nv::smb_telemetry::get_end_address();
    if (end_address == nv::smb_telemetry::SmbDirect::StartAddress) {
        return false;
    }

    if (offset < nv::smb_telemetry::SmbDirect::StartAddress || offset >= end_address) {
        return false;
    }

    const uint8_t actual_size = std::min(static_cast<size_t>(end_address - offset),
                                         buffer.size());

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    std::memcpy(buffer.data(),
                &cache[offset - nv::smb_telemetry::SmbDirect::StartAddress],
                actual_size);

    if (actual_size < buffer.size()) {
        std::fill(buffer.begin() + actual_size, buffer.end(), Cache::InvalidData);
    }

    return true;
}
