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
#include NV_IPC_CONFIG_H
#include <span>
#include <array>
#include "nv/i3c/driver.h"

namespace nv::i3c::smbpbi {

constexpr uint8_t SmbPbiBufferSize       = 8;
using SmbPbiBuffer                       = std::array<uint8_t, SmbPbiBufferSize>;
constexpr uint8_t SmbPbiDataSize         = 5;
constexpr uint8_t NumSmbPbiCommandToRead = 2;

enum class CommandCode : uint8_t
{
    NoOp           = 0x0,
    GetCap         = 0x1,
    GetTemperature = 0x2,
    GetPower       = 0x4,
};

enum class Register : uint8_t
{
    CommandStatus = 0x5C,
    Data          = 0x5D,
};

enum class StatusCode : uint8_t
{
    NullStatus      = 0x00,
    ErrNotSupported = 0x08,
    Success         = 0x1F,
};

enum class TemperatureSource : uint8_t
{
    PrimaryGpu   = 0x0,
    SecondaryGpu = 0x1,
    Board        = 0x4,
    Memory       = 0x5,
    ThermalLimit = 0x7,
};

enum class PowerSource : uint8_t
{
    GpuBoard = 0x0,
    Hbm      = 0x1,
    Module   = 0x2,
};

enum class State : uint8_t
{
    Init,
    ReadStatus,
    ReadData
};

enum class Direction : uint8_t
{
    Read,
    Write,
};

constexpr uint8_t RegAccessMask   = 0x0F;
constexpr uint8_t RegAccessShift  = 0x04;
constexpr uint8_t ReadRequestMask = 0x80;
constexpr uint8_t CommandExecute  = 0x80;
constexpr uint8_t StatusOffset    = 0x04;
constexpr uint8_t StatusMask      = 0x1F;

constexpr std::array<CommandCode, NumSmbPbiCommandToRead> SmbpbiCommands = {
    CommandCode::GetTemperature, CommandCode::GetPower};

bool generate_smbpbi_request(Direction          direction,
                             uint8_t            address,
                             std::span<uint8_t> buffer,
                             SmbPbiBuffer&      output);

bool start_smbpbi(nv::i3c::Driver& driver, uint8_t address, CommandCode command);

bool query_smbpbi_status(nv::i3c::Driver& driver, uint8_t address);

bool query_smbpbi_data(nv::i3c::Driver& driver, uint8_t address);

};  // namespace nv::i3c::smbpbi
