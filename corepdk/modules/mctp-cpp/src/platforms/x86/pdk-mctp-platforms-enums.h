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
    UsI2c = Begin,
    UsUsb = 1,
    UsEnd,
    DsI2c0 = 2,
    DsI2c1 = 3,
    DsI2c2 = 4,
    DsI2c3 = 5,
    DsI3c0 = 6,
    DsI3c1 = 7,
    Pldm   = 8,
    Spdm   = 9,
    Spdm4K = 10,
    Spi0   = 11,
    Spi1   = 12,
    Spi2   = 13,
    DsI2c4 = 14,
    DsI2c5 = 15,
    DsI2c6 = 16,
    DsI2c7 = 17,
    End,
};

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
    SetEpUuid              = 0x01,
    BootComplete           = 0x02,
    Heartbeat              = 0x03,
    EnableHeartbeat        = 0x04,
    QueryBootStatus        = 0x05,
    DownloadLog            = 0x06,
    EnableIbUpdate         = 0x07,
    SelfTest               = 0x08,
    BackgroundCopy         = 0x09,
    RestartNotification    = 0x0A,
    InstallDbgToken        = 0x0B,
    EraseDbgToken          = 0x0C,
    QueryDbgTokenStatus    = 0x0F,
    AddExtTimestamp        = 0x13,
    ProgramCertificate     = 0x31,
    ReadDevIkCsr           = 0x30,
    ApProvision            = 0x40,
    QueryApProvisionStatus = 0x41,
    GetGpioStatus          = 0x83,
    RegTableAccess         = 0x84,
    ScanI2c                = 0x85,
    DownloadCoverage       = 0x86,
    FanControl             = 0x87,
};

// NVIDIA Message Types
enum class NsmMsgType : uint8_t
{
    DeviceCapabilityDiscovery = 0,
    NetworkPorts              = 1,
    PciLinks                  = 2,
    PlatformEnviromentals     = 3,
    Diagnostics               = 4,
    DeviceConfiguration       = 5,
    Firmware                  = 6,
    Reserved                  = 7,
};

// NVIDIA TYPE 0 Device Capability Discovery Command Code
enum class NsmDcdCmdCode : uint8_t
{
    DcdPing                      = 0x00,
    DcdGetSupNvMsgTypes          = 0x01,
    DcdGetSupCmdCodes            = 0x02,
    DcdGetSupEventSrcs           = 0x03,
    DcdGetCurrentEventSrcs       = 0x04,
    DcdSetCurrentEventSrcs       = 0x05,
    DcdSetEventSubscription      = 0x06,
    DcdGetEventSubscription      = 0x07,
    DcdQueryDeviceIdentification = 0x09,
    DcdGetGpio                   = 0x0F,
    DcdSetGpio                   = 0x10,
};

// NVIDIA Type 6 Firmware Command Code
enum class NsmFWCmdCode : uint8_t
{
    GetRotStateInfo    = 0x1,  // Get RoT (Root of Trust) state information
    IrreversibleConf   = 0x2,  // Set irreversible configuration
    QueryCodeAuthKey   = 0x3,  // Query firmware code authentication key
    UpdateCodeAuthKey  = 0x4,  // Update firmware code authentication key
    QuerySecVerNum     = 0x5,  // Query firmware security version number
    UpdateMinSecVerNum = 0x6,  // Update minimum security version number
    QueryFwCompId      = 0x7,  // Query firmware component ID
    SetRotProperty     = 0x8,  // Set RoT property
    ImageCopyControl   = 0x9,  // Image copy control
};

// NVIDIA TYPE 5 Device Configuration Command Code
enum class NsmDevCfgCmdCode : uint8_t
{
    SetErrorInjectionMode           = 0x03,
    GetErrorInjectionMode           = 0x04,
    GetSupportedErrorInjectionTypes = 0x05,
    SetCurrentErrorInjectionTypes   = 0x06,
    GetCurrentErrorInjectionTypes   = 0x07,
    GetErrorInjectionPayload        = 0x0A,
    SetErrorInjectionPayload        = 0x0B,
    ActivateErrorInjection          = 0x0C,
};

// NVIDIA TYPE 3 Platform Environmental Telemetry  Command Code
enum class NsmPlatEnvCmdCode : uint8_t
{
    GetTemperatureReading   = 0x00,
    GetCurrentPowerDraw     = 0x03,
    GetInventoryInformation = 0x0C,
};

// NVIDIA TYPE 4 Device Diagnostics Command Code
enum class NsmDevDiagCmdCode : uint8_t
{
    GetDeviceResetStatistics = 0x0,
    GetDeviceDiagnostics     = 0x40,
};

}  // namespace pdk::mctp::platforms
