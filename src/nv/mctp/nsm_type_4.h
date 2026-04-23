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

#include "nv/i2c/lattice_driver.h"
#include "nv/mctp/constants.h"
#include "nv/perf_mon/perf_mon.h"

namespace nv::mctp {

constexpr uint8_t DevDiagGetDiagnosticsNoMoreSegments = 0xFF;
constexpr uint8_t DevDiagTimestampTagId               = 0XFF;
constexpr uint8_t DevDiagGetDiagnosticsFirstSegment   = 0x00;

constexpr uint8_t DevDiagGetResetStatisticsResetCauseId = 0x08;
constexpr uint8_t GetResetStatisticsResetLen            = 0x3;

// CPLD Register Table constants
constexpr uint8_t CpldRegisterTableNoMoreSegments = 0xFF;  // No more segments available
constexpr uint8_t CpldRegisterTableFirstSegment   = 0x00;  // First segment to query
constexpr uint16_t
    CpldRegTablePayloadMaxSize = Constants::MctpTxBufSize - sizeof(nv::mctp::PrivateHeader)
                               - sizeof(nv::mctp::Header)
                               - Constants::NsmHeaderResponseSize;  // 1 is for
                                                                    // the
                                                                    // next_segment
                                                                    // field

// Bridge and Port Recovery constants
constexpr uint8_t  NoMoreResetTargets = 0xFF;  // No more targets of this specific Reset Target
constexpr uint16_t ResetInProgress    = 0x0000;    // Reset in progress
constexpr uint16_t TimeCounterSaturated = 0xFFFF;  // Time counter saturated (49.7+ days)
constexpr uint16_t ResetTime1Ms         = 0x0001;  // 1 msec hardcoded for L1/L2 resets
constexpr uint8_t  ReservedField        = 0x00;    // Reserved field value

// Recovery Level enum
enum class RecoveryLevel : uint8_t
{
    ApplicationReset = 0,    // Application Reset (not supported)
    ProtocolReset    = 1,    // Protocol Reset
    PortReset        = 2,    // Port Reset
    OobHardwareReset = 3,    // OOB Hardware Reset (not supported)
    QueryNextTarget  = 255,  // Query next available Reset Target (no reset)
    // 4-254 are reserved
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
constexpr auto T4DsInterfaceErrorNum = 13;
/**
 * @brief DsInterfaceErrorTable defines the OoBerror for each interface
 */
constexpr inline std::array<DsInterfaceError, T4DsInterfaceErrorNum> DsInterfaceErrorTable = {
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c0, nv::perf_mon::OobBus::DsI2c0},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c1, nv::perf_mon::OobBus::DsI2c1},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c2, nv::perf_mon::OobBus::DsI2c2},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c3, nv::perf_mon::OobBus::DsI2c3},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c4, nv::perf_mon::OobBus::DsI2c4},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c5, nv::perf_mon::OobBus::DsI2c5},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c6, nv::perf_mon::OobBus::DsI2c6},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI2c7, nv::perf_mon::OobBus::DsI2c7},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI3c0, nv::perf_mon::OobBus::DsI3c0},
    DsInterfaceError{pdk::mctp::platforms::Interface::DsI3c1, nv::perf_mon::OobBus::DsI3c1},
    DsInterfaceError{  pdk::mctp::platforms::Interface::Spi0,   nv::perf_mon::OobBus::Spi0},
    DsInterfaceError{  pdk::mctp::platforms::Interface::Spi1,   nv::perf_mon::OobBus::Spi1},
    DsInterfaceError{  pdk::mctp::platforms::Interface::Spi2,   nv::perf_mon::OobBus::Spi2},
};

// Bridge and Port Recovery Request Data Structure
struct [[gnu::packed]] BridgePortRecoveryReq
{
    uint8_t recovery_level;  // Recovery Level (enum8)
    uint8_t reset_target;    // Reset Target (NvU8)
};

// Bridge and Port Recovery Response Data Structure
struct [[gnu::packed]] BridgePortRecoveryResp
{
    uint8_t  next_reset_target;      // Next Reset Target (NvU8) - Offset 0
    uint8_t  reserved;               // Reserved (NvU8) - Offset 1
    uint16_t time_since_last_reset;  // Time since last Reset request in msec (NvU16) - Offset
                                     // 2-3
};

enum T4WriteProtectionMode : uint8_t
{
    Clear = 0,
    Set   = 1,
};

struct [[gnu::packed]] T4WriteProtectionRequest
{
    uint8_t function;
    uint8_t mode;  // 0 = clear, 1 = set
    T4WriteProtectionRequest() : function{0}, mode{0}
    {
        // Empty
    }
};

/**
 * Platform hook for enabling/disabling write protection on a given function.
 * Weak default in nsm_type_4.cpp returns NotSupported.
 * Strong override (e.g. p7612_hgx) looks up WriteProtectionList, writes GPIO, and verifies
 * via readback.
 *
 * @param function  Write protection function ID from the request
 * @param mode      T4WriteProtectionMode (Clear=0, Set=1)
 * @return NsmStatus::OK on success, NsmStatus::NotSupported if function not found,
 *         NsmStatus::ErrorGeneral on GPIO readback mismatch
 */
NsmStatus platform_write_protection_gpio(uint8_t function, uint8_t mode);

// CPLD Register Table Request Data Structure
struct [[gnu::packed]] CpldRegisterTableReq
{
    uint8_t segment_index;  // Segment Index (0x00-0xFE for segment, 0xFF for no more segments)
};

struct [[gnu::packed]] CpldRegisterTableResp
{
    uint8_t next_segment;  // Next Segment (0x00-0xFE for next segment, 0xFF for no more
                           // segments)
    // Variable length segment data follows after this structure
    uint8_t segment_data[Cpld_User_Reg::CPLD_USER_REG_SIZE];  // Variable length segment data
                                                              // follows after this structure
};

}  // namespace nv::mctp
