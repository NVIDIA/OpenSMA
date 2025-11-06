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
#include "nv/i3c/smbpbi.h"
#include "nv/i2c/helper.h"
using namespace nv::i3c::smbpbi;

bool nv::i3c::smbpbi::generate_smbpbi_request(Direction          direction,
                                              uint8_t            address,
                                              std::span<uint8_t> buffer,
                                              SmbPbiBuffer&      output)
{
    if (buffer.size() > SmbPbiDataSize) {
        return false;
    }
    const uint8_t RequestSize = 4;
    output[0]                 = address;
    output[1]                 = (RequestSize << RegAccessShift) | RegAccessMask;
    if (direction == Direction::Read) {
        output[1] |= ReadRequestMask;
    }
    std::copy(buffer.begin(), buffer.end(), output.begin() + 2);
    output[7] = nv::i2c::crc8(std::span<uint8_t>(output).subspan(0, output.size() - 1));
    return true;
}

bool nv::i3c::smbpbi::start_smbpbi(nv::i3c::Driver& driver,
                                   uint8_t          address,
                                   CommandCode      command)
{
    smbpbi::SmbPbiBuffer                        smb_pbi_buffer{};
    std::array<uint8_t, smbpbi::SmbPbiDataSize> payload{
        static_cast<uint8_t>(smbpbi::Register::CommandStatus),
        static_cast<uint8_t>(command),
        static_cast<uint8_t>(smbpbi::PowerSource::GpuBoard),
        0x00,
        smbpbi::CommandExecute};
    const bool success = smbpbi::generate_smbpbi_request(
        smbpbi::Direction::Write, address, payload, smb_pbi_buffer);
    if (!success) {
        return false;
    }
    const bool result = driver.write(
        smb_pbi_buffer.at(0) >> 1,
        std::span<uint8_t>(smb_pbi_buffer).subspan(1, smb_pbi_buffer.size() - 1));
    if (!result) {
        return false;
    }

    nv::i3c::smbpbi::query_smbpbi_status(driver, address);
    return true;
}

bool nv::i3c::smbpbi::query_smbpbi_status(nv::i3c::Driver& driver, uint8_t address)
{
    smbpbi::SmbPbiBuffer                        smb_pbi_buffer{};
    std::array<uint8_t, smbpbi::SmbPbiDataSize> payload{
        static_cast<uint8_t>(smbpbi::Register::CommandStatus), 0x00, 0x00, 0x00};

    const bool success = smbpbi::generate_smbpbi_request(
        smbpbi::Direction::Read, address, payload, smb_pbi_buffer);
    if (!success) {
        return false;
    }
    const bool result = driver.write(
        smb_pbi_buffer.at(0) >> 1,
        std::span<uint8_t>(smb_pbi_buffer).subspan(1, smb_pbi_buffer.size() - 1));
    if (!result) {
        return false;
    }
    return true;
}

bool nv::i3c::smbpbi::query_smbpbi_data(nv::i3c::Driver& driver, uint8_t address)
{
    smbpbi::SmbPbiBuffer                        smb_pbi_buffer{};
    std::array<uint8_t, smbpbi::SmbPbiDataSize> payload{
        static_cast<uint8_t>(smbpbi::Register::Data), 0x00, 0x00, 0x00};

    const bool success = smbpbi::generate_smbpbi_request(
        smbpbi::Direction::Read, address, payload, smb_pbi_buffer);
    if (!success) {
        return false;
    }
    const bool result = driver.write(
        smb_pbi_buffer.at(0) >> 1,
        std::span<uint8_t>(smb_pbi_buffer).subspan(1, smb_pbi_buffer.size() - 1));
    if (!result) {
        return false;
    }
    return true;
}
