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
#include <cstddef>
#include <cstdint>

#include "nv/perf_mon/perf_mon.h"

namespace nv::mctp {

constexpr uint8_t NsmDevDiagSupportedCmdNum           = 16;
constexpr uint8_t DevDiagGetDiagnosticsNoMoreSegments = 0xFF;
constexpr uint8_t DevDiagTimestampTagId               = 0XFF;
constexpr uint8_t DevDiagGetDiagnosticsFirstSegment   = 0x00;

constexpr uint8_t DevDiagGetResetStatisticsResetCauseId = 0x08;
constexpr uint8_t GetResetStatisticsResetLen            = 0x3;

// NVIDIA TYPE 4 Device Diagnostics Command Code
enum class NsmDevDiagCmdCode : uint8_t
{
    GetDeviceResetStatistics = 0x0,
    GetDeviceDiagnostics     = 0x40,
};

enum class AggregateTaskEvent : uint32_t
{
    Mctp   = nv::common::bit(0),
    I2C    = nv::common::bit(1),
    I3C    = nv::common::bit(2),
    PLDM   = nv::common::bit(3),
    USB    = nv::common::bit(4),
    Flash  = nv::common::bit(5),
    Logger = nv::common::bit(6),
    SPDM   = nv::common::bit(7),
};

constexpr std::array<uint8_t, NsmDevDiagSupportedCmdNum> gen_type4_code_bitmask()
{
    std::array<uint8_t, NsmDevDiagSupportedCmdNum> bitmask = {0};

    constexpr uint8_t bit_positions[] = {
        static_cast<uint8_t>(NsmDevDiagCmdCode::GetDeviceResetStatistics),
        static_cast<uint8_t>(NsmDevDiagCmdCode::GetDeviceDiagnostics)};

    for (uint8_t pos : bit_positions) {
        const size_t byte_index = pos / 8;
        const size_t bit_offset = pos % 8;
        const size_t value      = (1u << bit_offset);
        if (value <= UCHAR_MAX) {
            bitmask.at(byte_index) |= static_cast<uint8_t>(value);
        }
    }

    return bitmask;
}

struct [[gnu::packed]] NsmDevDiagPersistentData
{
    static constexpr std::array<uint8_t, NsmDevDiagSupportedCmdNum>
        suppCmdCode = gen_type4_code_bitmask();
};

struct [[gnu::packed]] TaskExecutionTimeResp
{
    uint8_t                         task_id;
    nv::perf_mon::TaskExecutionTime execution_time;
};

struct [[gnu::packed]] T4FlashUsageResp
{
    uint32_t usage;
    uint32_t total_slots;  // total from 2 slots
};

struct [[gnu::packed]] T4RamSizeResp
{
    uint32_t usage;
    uint32_t total;
};

using T4TaskIdAndPriorityResponse = std::
    array<std::pair<uint8_t, uint8_t>, static_cast<uint8_t>(nv::ipc::TaskId::KernelEnd)>;

struct [[gnu::packed]] T4ErrorCounterResponse
{
    uint8_t                        latached_error;
    nv::perf_mon::OobBusErrorCount error_count;
};

/**
 *  The Data Types below are used for getting Error Counter Telemetry
 */
using DsMcpInterface                 = pdk::mctp::platforms::Interface;
using DsOobBusError                  = nv::perf_mon::OobBus;
using DsInterfaceError               = std::tuple<DsMcpInterface, DsOobBusError>;
constexpr auto T4DsInterfaceErrorNum = 9;
/**
 * @brief DsInterfaceErrorTable defines the OoBerror for each interface
 */
constexpr inline std::array<DsInterfaceError, T4DsInterfaceErrorNum> DsInterfaceErrorTable = {
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c0, nv::perf_mon::OobBus::DsI2c0},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c1, nv::perf_mon::OobBus::DsI2c1},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c2, nv::perf_mon::OobBus::DsI2c2},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c3, nv::perf_mon::OobBus::DsI2c3},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI3c0, nv::perf_mon::OobBus::DsI3c0},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI3c1, nv::perf_mon::OobBus::DsI3c1},
    DsInterfaceError{  pdk::mctp::platforms::Interface::Spi0,   nv::perf_mon::OobBus::Spi0},
    DsInterfaceError{  pdk::mctp::platforms::Interface::Spi1,   nv::perf_mon::OobBus::Spi1},
    DsInterfaceError{  pdk::mctp::platforms::Interface::Spi2,   nv::perf_mon::OobBus::Spi2},
};

}  // namespace nv::mctp
