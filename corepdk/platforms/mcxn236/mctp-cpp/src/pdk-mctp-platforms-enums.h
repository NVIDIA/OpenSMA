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

namespace pdk::mctp::platforms {

// Ensure contain Begin & End
enum class Interface : uint16_t
{
    Begin,
    UsI2c   = Begin,
    UsUsb   = 1,
    UsUsbFs = 2,
    UsI3c   = 3,
    UsEnd,
    DsI2c0 = 4,
    DsI2c1 = 5,
    DsI2c2 = 6,
    DsI2c3 = 7,
    DsI3c0 = 8,
    DsI3c1 = 9,
    Pldm   = 10,
    Spdm   = 11,
    Spdm4K = 12,
    Spi0   = 13,
    Spi1   = 14,
    Spi2   = 15,
    DsI2c4 = 16,
    DsI2c5 = 17,
    DsI2c6 = 18,
    DsI2c7 = 19,
    End,
};

constexpr bool is_upstream_client(Interface client)
{
    return client == Interface::UsI2c || client == Interface::UsUsb
        || client == Interface::UsUsbFs || client == Interface::UsI3c;
}

enum class Ccode : uint8_t
{
    Success                      = 0x00,
    ErrorGeneral                 = 0x01,
    ErrorInvalidData             = 0x02,
    ErrorInvalidLength           = 0x03,
    ErrorNotReady                = 0x04,
    ErrorUnsupportedCmd          = 0x05,
    ErrorUnsupportedMsgType      = 0x06,
    ErrorInvalidOcpVersion       = 0x07,
    ErrorInsufficientSpace       = 0x80,  // Ccode for RoutInfoUpdate
    ErrorUpdateDbFail            = 0x81,
    ErrorInvalidStateForCommand  = 0x80,  // ImageCopyControl (NSM T6)
    ErrorInvalidRequestType      = 0x81,  // ImageCopyControl (NSM T6)
    ErrorUnsupportedArgument     = 0x80,  // SetRotProperty   (NSM T6)
    ErrorEfuseUpdateFailed       = 0x86,
    ErrorIrreversibleConfDisable = 0x87,
    ErrorNonceMismatch           = 0x88,
    ErrorDebugTokenInstalled     = 0x89,
    ErrorPldmProcessing          = 0x8A,
    ErrorI2CError                = 0x8B,
};

enum class Rcode : uint16_t
{
    Null                         = 0x00,
    ErrorEfuseUpdateFailed       = 0x86,
    ErrorIrreversibleConfDisable = 0x87,
    ErrorNonceMismatch           = 0x88,
    ErrorDebugTokenInstalled     = 0x89,
    ErrorPldmProcessing          = 0x8A,
    ErrorPengingActivation       = 0x8B,

    // ImageCopyControl, SetRotProperty (NSM T6) related reason codes
    PropertyNotSupported           = 0x100,
    LifespanVolatileNotSupported   = 0x101,
    LifespanPersistentNotSupported = 0x102,
    NoBootComplete                 = 0x103,
    UpdateInProgress               = 0x104,
    ImageCopyInProgress            = 0x105,
    ImageCopyCompleted             = 0x106,
    FlashWearMitigation            = 0x107,
    IncompleteComponentSet         = 0x108,

    // RCode for Ccode ErrorInvalidData
    InvalidSensorId        = 0x200,
    InvalidThreshold       = 0x201,
    InvalidSensorNumber    = 0x202,  // Leak Detection
    InvalidThresholdNumber = 0x203,  // Leak Detection
};

enum class VdmCmd : uint8_t
{
    SetEpUuid           = 0x01,
    BootComplete        = 0x02,
    Heartbeat           = 0x03,
    EnableHeartbeat     = 0x04,
    QueryBootStatus     = 0x05,
    DownloadLog         = 0x06,
    EnableIbUpdate      = 0x07,
    SelfTest            = 0x08,
    BackgroundCopy      = 0x09,
    RestartNotification = 0x0A,
    InstallDbgToken     = 0x0B,
    EraseDbgToken       = 0x0C,
    QueryDbgTokenStatus = 0x0F,
    AddExtTimestamp     = 0x13,
    ProgramCertificate  = 0x31,
    ReadDevIkCsr        = 0x30,
    // 0x40, 0x41, 0x50 and 0x51 are reserved.
    GetGpioStatus    = 0x83,
    RegTableAccess   = 0x84,
    ScanI2c          = 0x85,
    DownloadCoverage = 0x86,
    FanControl       = 0x87,
    // 0x88 reserved for the DFP thermal-control command
    I2cFreqControl = 0x89,
    BusModeControl = 0x8A,
};

}  // namespace pdk::mctp::platforms
