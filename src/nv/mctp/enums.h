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

#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-enums.h"

namespace nv::mctp {
using PhyId       = pdk::mctp::app::PhyId;
using PhyMediumId = pdk::mctp::app::PhyMediumId;

using MsgType = pdk::mctp::app::MsgType;

using Ccode = pdk::mctp::platforms::Ccode;

using Cmd = pdk::mctp::app::Cmd;

using SetEndpoint = pdk::mctp::app::SetEndpoint;

using AllocateEndpoint = pdk::mctp::app::AllocateEndpoint;

using EidAssignStatus = pdk::mctp::app::EidAssignStatus;

using EndpointType = pdk::mctp::app::EndpointType;

using EndpointIdType = pdk::mctp::app::EndpointIdType;

using VersionType = pdk::mctp::app::VersionType;

using Version = pdk::mctp::app::Version;

using PacketType = pdk::mctp::app::PacketType;

using VdmCmd = pdk::mctp::platforms::VdmCmd;

enum class BackgroundCopyCmd : uint8_t
{
    Disable_Bg            = 0x00,
    Enable_Bg             = 0x01,
    Disable_Bg_One_Time   = 0x02,
    Enable_Bg_One_Time    = 0x03,
    Init_Bg_Update        = 0x04,
    Query_Status          = 0x05,
    Query_Progress        = 0x06,
    Query_Pending_Bg_Copy = 0x07,
};

enum class BackgroundCopyPolicy : uint8_t
{
    Default = 0x00,
    // PdsBackgroundSetup
    Enable  = 0x01,
    Disable = 0x02,
    // PdsBackgroundSetupOneTime
    OnceDisable = 0x02,
    OnceEnable  = 0x03,
};

enum class BackgroundCopyState : uint8_t
{
    // version 1 definition
    Background_Copy_Idle_Or_Complete = 0x01,
    Background_Copy_In_Progress      = 0x02,
    // version 2 definition
    Background_Copy_Idle    = 0x01,
    Background_Copy_Success = 0x03,
    Background_Copy_Failed  = 0x04,
};

enum class BackgroundCopyPending : uint8_t
{
    No_Pending = 0x01,
    Pending    = 0x02,
};

enum class BackgroundCopyError : uint8_t
{
    Ap_Not_Boot      = 0x01,
    Copy_In_Progress = 0x02,
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
};

// NVIDIA Type 0 Device Capability Discovery Event
enum class NsmDcdEvent : uint8_t
{
};

// NVIDIA Type 6 Firmware Event
enum class NsmFwEvent : uint8_t
{
    RotStateInformationChangeEvent = 0x1,
};

enum class NsmGlobalEventSetting : uint8_t
{
    EventDisable      = 0x0,
    EventPolling      = 0x1,
    EventPush         = 0x2,
    EventNotSubscribe = 0x3,
};

typedef enum
{
    NsmBuildTypeDev       = 0,
    NsmBuildTypeRel       = 1,
    NsmBuildTypeUndefined = 2,
} NsmBuildType;

typedef enum
{
    MakefileBuildTypeRel   = 0,
    MakefileBuildTypeDev   = 1,
    MakefileBuildTypeDebug = 2,
} MakefileBuildType;

typedef enum
{
    UpdateKeyPermittedValue = 0,
    UpdateKeySpecifiedValue = 1,
} UpdateKeyValue;

typedef enum
{
    TagBackgroundCopyPolicy = 1,   // Size: Enum8
    TagActiveFirmwareSlot   = 2,   // Size: NvU8
    TagActiveKeySet         = 3,   // Size: NvU8
    TagWriteProtectState    = 4,   // Size: Enum8
    TagFirmwareSlotCount    = 5,   // Size: NvU8
    TagFirmwareSlotId       = 6,   // Size: NvU8
    TagFirmwareVerString    = 7,   // Size: Char array
    TagVerComparisonStamp   = 8,   // Size: NvU32
    TagBuildType            = 9,   // Size: Enum8
    TagSigningType          = 10,  // Size: Enum8
    TagFirmwareState        = 11,  // Size: Enum8
    TagSecurityVerNum       = 12,  // Size: NvU16
    TagMinSecurityVerNum    = 13,  // Size: NvU16
    TagSigningKeyIndex      = 14,  // Size: NvU16
    TagInbandUpdatePolicy   = 15,  // Size: Enum8
    TagBootStatusCode       = 16,  // Size: NvU64
} GetRotTag;

typedef enum
{
    TagBackgroundCopyPolicyLen = 0,  // Size: Enum8
    TagActiveFirmwareSlotLen   = 0,  // Size: NvU8
    TagActiveKeySetLen         = 0,  // Size: NvU8
    TagWriteProtectStateLen    = 0,  // Size: Enum8
    TagFirmwareSlotCountLen    = 0,  // Size: NvU8
    TagFirmwareSlotIdLen       = 0,  // Size: NvU8
    TagFirmwareVerStringLen    = 5,  // Size: Char array
    TagVerComparisonStampLen   = 2,  // Size: NvU32
    TagBuildTypeLen            = 0,  // Size: Enum8
    TagSigningTypeLen          = 0,  // Size: Enum8
    TagFirmwareStateLen        = 0,  // Size: Enum8
    TagSecurityVerNumLen       = 1,  // Size: NvU16
    TagMinSecurityVerNumLen    = 1,  // Size: NvU16
    TagSigningKeyIndexLen      = 1,  // Size: NvU16
    TagInbandUpdatePolicyLen   = 0,  // Size: Enum8
    TagBootStatusCodeLen       = 3,  // Size: NvU64
} RotTagLength;

typedef enum
{
    SigningTypeDebug    = 0,
    SigningTypeProd     = 1,
    SigningTypeExternal = 2,
    SigningTypeDot      = 3,
} SigningType;

typedef enum
{
    IrreversibleCtrlQuery   = 0x00,
    IrreversibleCtrlDisable = 0x01,
    IrreversibleCtrlEnable  = 0x02,
} IrreversibleCtrl;

typedef enum
{
    BootStatusEcReceiveAp0BootComplete = 5,
    BootStatusApFwBootSlot             = 20,
} BootStatus;

typedef enum
{
    Slot0Id     = 0,
    Slot1Id     = 1,
    InvalidSlot = 2,
} SlotId;

typedef enum
{
    ProgramSuccess                    = 0,
    InvalidCertificate                = 1,
    AlreadyExists                     = 2,
    InvalidRequestType                = 3,
    PrecedingCertificatesNoFound      = 4,
    SignatureValidationFail           = 5,
    UnknownFail                       = 6,
    PdsL3CertReadFail                 = 7,
    PdsL3CertSetFail                  = 8,
    OtpDdaOrdinalReadFail             = 9,
    OtpDdaOrdinalParseFail            = 10,
    OtpDdaOrdinalProgrammedFail       = 11,
    InvalidDdaOrdinalNumber           = 12,
    FlashWriteFail                    = 13,
    FlashEraseFail                    = 14,
    OtpL4SignatureReadFail            = 15,
    OtpL4SignatureProgrammedFail      = 16,
    OtpL4SignatureProgrammedCheckFail = 17,
} ProgramCertificateStatus;

// NVIDIA TYPE 3 Platform Environmental Telemetry  Command Code
enum class NsmPlatEnvCmdCode : uint8_t
{
    GetTemperatureReading   = 0x00,
    GetCurrentPowerDraw     = 0x03,
    GetInventoryInformation = 0x0C,
};

/** Sensors used in Type 3 - Get Temperature Reading */
enum Type3TemperatureSensors : uint8_t
{
    TempGpu1        = 0,  // Temperature value of GPU1 in SXM7.1
    TempGpu2        = 1,  // Temperature value of GPU2 in SXM7.1
    TempTMP451_1    = 2,  // TMP451 ambient sensor temp 1
    TempTMP451_2    = 3,  // TMP451 ambient sensor temp 2
    TempMaxModule   = 4,  // Maximum of TMP451 sensors
    TempSMAInternal = 5,  // Internal SMA temperature sensor
    TempCX8_1       = 6,  // CX8 1 Sensor
    TempCX8_2       = 7,  // CX8 2 Sensor
};

enum Type3PowerSensors : uint8_t
{
    PowerGpu1   = 0,  // GPU1 power in SXM7.1
    PowerGpu2   = 1,  // GPU2 power in SXM7.1
    PowerModule = 2,  // SXM module power (sum of GPUs which are currently reported on HSC)
};

// NVIDIA TYPE 4 MCU Variant Specific Entries
enum Type4CommonDiagnosticEntries : uint8_t
{
    DIAG_FIRMWARE_VERSION      = 0,   // FirmwareInfoTelemetry
    DIAG_BUILD_INFORMATION     = 1,   // FirmwareInfoTelemetry
    DIAG_FLASH_USAGE_AND_SIZE  = 2,   // PerformanceTelemetry
    DIAG_RAM_USAGE_AND_SIZE    = 3,   // PerformanceTelemetry
    DIAG_BOOT_TIME             = 4,   // PerformanceTelemetry
    DIAG_TASK_SWITCH_LATENCY   = 5,   // PerformanceTelemetry
    DIAG_TASK_PRIORITY         = 6,   // PerformanceTelemetry
    DIAG_CPU_UTILIZATION       = 7,   // PerformanceTelemetry
    DIAG_TASK0_EXECUTION_TIME  = 8,   // PerformanceTelemetry
    DIAG_TASK1_EXECUTION_TIME  = 9,   // PerformanceTelemetry
    DIAG_TASK2_EXECUTION_TIME  = 10,  // PerformanceTelemetry
    DIAG_TASK3_EXECUTION_TIME  = 11,  // PerformanceTelemetry
    DIAG_TASK4_EXECUTION_TIME  = 12,  // PerformanceTelemetry
    DIAG_TASK5_EXECUTION_TIME  = 13,  // PerformanceTelemetry
    DIAG_TASK6_EXECUTION_TIME  = 14,  // PerformanceTelemetry
    DIAG_TASK7_EXECUTION_TIME  = 15,  // PerformanceTelemetry
    DIAG_TASK8_EXECUTION_TIME  = 16,  // PerformanceTelemetry
    DIAG_TASK9_EXECUTION_TIME  = 17,  // PerformanceTelemetry
    DIAG_TASK10_EXECUTION_TIME = 18,  // PerformanceTelemetry
    DIAG_TASK11_EXECUTION_TIME = 19   // PerformanceTelemetry
};

// NVIDIA TYPE 4 MCU Variant Specific Entries
enum Type4McuDiagnosticEntries : uint8_t
{
    // Sensors <= 230
    DIAG_CX8_1_TEMP      = 220,  // CacheTemperatureTelemetry
    DIAG_CX8_2_TEMP      = 221,  // CacheTemperatureTelemetry
    DIAG_GPU1_TEMP       = 222,  // CacheTemperatureTelemetry
    DIAG_GPU2_TEMP       = 223,  // CacheTemperatureTelemetry
    DIAG_GPU1_POWER      = 224,  // CachePowerTelemetry
    DIAG_GPU2_POWER      = 225,  // CachePowerTelemetry
    DIAG_MODULE_POWER    = 226,  // CachePowerTelemetry
    DIAG_MODULE_TEMP1    = 227,  // CacheTemperatureTelemetry
    DIAG_MODULE_TEMP2    = 228,  // CacheTemperatureTelemetry
    DIAG_INTERNAL_TEMP   = 229,  // CacheTemperatureTelemetry
    DIAG_MAX_MODULE_TEMP = 230,  // CacheTemperatureTelemetry

    // Counters 231-250
    DIAG_I3C0_BUS_ERROR = 231,
    /* DIAG_I3Cn_BUS_ERROR is implicit as
     * DIAG_I3C0_BUS_ERROR + n */

    DIAG_I2C_UPSTREAM_ERROR = 233,

    DIAG_I2C0_DOWNSTREAM_BUS_ERROR = 234,
    /* DIAG_I2Cn_DOWNSTREAM_BUS_ERROR is implicit as
     * DIAG_I2C0_DOWNSTREAM_BUS_ERROR + n */

    DIAG_SPI0_DOWNSTREAM_BUS_ERROR = 240,
    /* DIAG_SPIn_DOWNSTREAM_BUS_ERROR is implicit as
     * DIAG_SPI0_DOWNSTREAM_BUS_ERROR + n */

    DIAG_ERROR_COUNTER_BOUNDARY = 251,  // last Counter + 1

    DIAG_GPIO_VALUE_BITMAP = 254,  // GpioTelemetry
    DIAG_CurrentTimestamp  = 255   // TimestampTelemetry
};

enum Type4TelemetryTypes : uint8_t
{
    FirmwareInfoTelemetry     = 0,
    PerformanceTelemetry      = 1,
    GpioTelemetry             = 2,
    ErrorCounterTelemetry     = 3,
    CachePowerTelemetry       = 4,
    CacheTemperatureTelemetry = 5,
    TimestampTelemetry        = 6
};

}  // namespace nv::mctp
