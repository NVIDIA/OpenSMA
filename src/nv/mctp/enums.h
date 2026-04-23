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

using Rcode = pdk::mctp::platforms::Rcode;

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

// Common Status Codes to be used in NSM message handlers.
enum class NsmStatus : uint8_t
{
    OK                = 0x00,
    ErrorGeneral      = 0x01,
    NotSupported      = 0x02,
    I2cBusError       = 0x03,
    GpioVerifyFailure = 0x04,
    InvalidData       = 0x05,
};

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
    NvInternal                = 0xFF,  // NVIDIA internal commands (not for external use)
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
    DcdConfigureEventAck         = 0x0A,
    DcdGetGpio                   = 0x0F,
    DcdSetGpio                   = 0x10,
    DcdGetDeviceCapabilitiesV2   = 0x11,
};

// NVIDIA Type 4 Diagnostics command code
enum class NsmType4CmdCode : uint8_t
{
    InstallToken   = 0x01,
    EraseToken     = 0x02,
    QueryToken     = 0x03,
    QueryDeviceIds = 0x58,
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

// NVIDIA Type 0 Device Capability Discovery Event
enum class NsmDcdEvent : uint8_t
{
    GpioEvent = 3,

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
    TagRedundancyPolicyPersistent   = 1,   // Size: Enum8
    TagActiveFirmwareSlot           = 2,   // Size: NvU8
    TagActiveKeySet                 = 3,   // Size: NvU8
    TagWriteProtectState            = 4,   // Size: Enum8
    TagFirmwareSlotCount            = 5,   // Size: NvU8
    TagFirmwareSlotId               = 6,   // Size: NvU8
    TagFirmwareVerString            = 7,   // Size: Char array
    TagVerComparisonStamp           = 8,   // Size: NvU32
    TagBuildType                    = 9,   // Size: Enum8
    TagSigningType                  = 10,  // Size: Enum8
    TagFirmwareState                = 11,  // Size: Enum8
    TagSecurityVerNum               = 12,  // Size: NvU16
    TagMinSecurityVerNum            = 13,  // Size: NvU16
    TagSigningKeyIndex              = 14,  // Size: NvU16
    TagInbandUpdatePolicyPersistent = 15,  // Size: Enum8
    TagBootStatusCode               = 16,  // Size: NvU64
    TagInbandUpdatePolicyCurrent    = 17,  // Size: Enum8
    TagRedundancyPolicyCurrent      = 18,  // Size: Enum8
    TagApSkuId                      = 19,  // Size: NvU32
    TagGlobalFailoverPolicy         = 20,  // Size: Enum8
} GetRotTag;

// Device Capabilities V2 Tags
typedef enum
{
    TagTimestampGeneration = 0,  // Size: Enum8
    TagMaxInputBufferSize  = 1,  // Size: NvU32
} DeviceCapabilitiesV2Tag;

// Timestamp Generation Capabilities
typedef enum
{
    TimestampNotSupported   = 0,  // Device does not support generating timestamps
    TimestampEpochBased     = 1,  // Device reports epoch-based timestamps
    TimestampMonotonicBased = 2,  // Device reports monotonic timer-based timestamps
} TimestampGenerationCapability;

// Device buffer size constants
typedef enum
{
    MaxInputBufferSize = 4108,  // Maximum data payload size (MultiPktBufSize 4128 -
                                // PrivateHeader 4 - MCTP Header 4 - NSM V2 Header 12)
} DeviceBufferSize;

// ROT Tag Length constants (used for telemetry record sizing)
// These represent the length in powers of 2 (log2 of actual byte size)
namespace RotTagLength {
constexpr uint8_t TagRedundancyPolicyPersistentLen   = 0;  // Size: Enum8
constexpr uint8_t TagActiveFirmwareSlotLen           = 0;  // Size: NvU8
constexpr uint8_t TagActiveKeySetLen                 = 0;  // Size: NvU8
constexpr uint8_t TagWriteProtectStateLen            = 0;  // Size: Enum8
constexpr uint8_t TagFirmwareSlotCountLen            = 0;  // Size: NvU8
constexpr uint8_t TagFirmwareSlotIdLen               = 0;  // Size: NvU8
constexpr uint8_t TagFirmwareVerStringLen            = 5;  // Size: Char array
constexpr uint8_t TagVerComparisonStampLen           = 2;  // Size: NvU32
constexpr uint8_t TagBuildTypeLen                    = 0;  // Size: Enum8
constexpr uint8_t TagSigningTypeLen                  = 0;  // Size: Enum8
constexpr uint8_t TagFirmwareStateLen                = 0;  // Size: Enum8
constexpr uint8_t TagSecurityVerNumLen               = 1;  // Size: NvU16
constexpr uint8_t TagMinSecurityVerNumLen            = 1;  // Size: NvU16
constexpr uint8_t TagSigningKeyIndexLen              = 1;  // Size: NvU16
constexpr uint8_t TagInbandUpdatePolicyPersistentLen = 0;  // Size: Enum8
constexpr uint8_t TagBootStatusCodeLen               = 3;  // Size: NvU64
constexpr uint8_t TagInbandUpdatePolicyCurrentLen    = 0;  // Size: Enum8
constexpr uint8_t TagRedundancyPolicyCurrentLen      = 0;  // Size: Enum8
constexpr uint8_t TagApSkuIdLen                      = 2;  // Size: NvU32
constexpr uint8_t TagGlobalFailoverPolicyLen         = 0;  // Size: Enum8
}  // namespace RotTagLength

enum class NsmRedundancyPolicy : uint8_t
{
    Manual        = 0x00,
    Automatic     = 0x01,
    NotApplicable = 0xFF,
};

enum class NsmInBandUpdatePolicy : uint8_t
{
    Disabled      = 0x00,
    Enabled       = 0x01,
    NotApplicable = 0xFF,
};

enum class NsmGlobalFailoverPolicy : uint8_t
{
    Disabled      = 0x00,
    Enabled       = 0x01,
    NotApplicable = 0xFF,
};

enum class NsmSetRotPropertyRequest : uint8_t
{
    SetRedundancyPolicy     = 0x00,
    SetInbandUpdatePolicy   = 0x01,
    SetApSkuId              = 0x02,
    SetGlobalFailoverPolicy = 0x03,
};

enum class NsmImageCopyControlRequest : uint8_t
{
    QueryImageCopyProgress = 0x00,
    InitiateImageCopy      = 0x01,
};

enum class NsmImageCopyStatus : uint8_t
{
    NotTriggered              = 0x00,
    InProgress                = 0x01,
    Completed                 = 0x02,
    UndefinedFailed           = 0x03,
    NoValidImage              = 0x04,
    DestinationWriteProtected = 0x05,
    FailFlashAccess           = 0x06,
    FailVerifySignature       = 0x07,
};

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
    L4verifyL5CertFail                = 18,
    InvalidTemplate                   = 19,
} ProgramCertificateStatus;

// NVIDIA TYPE 2 PciLinks Command Code
enum class NsmPciLinksCmdCode : uint8_t
{
    AssertPcieFundamentalReset = 0x60,
};

// NVIDIA TYPE 3 Platform Environmental Telemetry  Command Code
enum class NsmPlatEnvCmdCode : uint8_t
{
    GetTemperatureReading      = 0x00,
    SetThermalParameter        = 0x01,
    GetThermalParameter        = 0x02,
    GetCurrentPowerDraw        = 0x03,
    GetInventoryInformation    = 0x0C,
    GetVoltage                 = 0x0F,
    GetLeakDetectionInfo       = 0x17,
    SetLeakDetectionThresholds = 0x16,
};

// NVIDIA TYPE 4 Device Diagnostics Command Code
enum class NsmDevDiagCmdCode : uint8_t
{
    GetDeviceResetStatistics     = 0x0,
    GetDeviceDiagnostics         = 0x40,
    GetDeviceHscAlert            = 0x64,
    EnableDisableWriteProtection = 0x65,
    ClearHscFaults               = 0x6D,
    GetCpldRegisterTable         = 0x6E,
    BridgePortRecovery           = 0x70,
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
    SetGpuDegradeMode               = 0x60,
    EnableDisablePowerSupply        = 0x61,
    GetSmaBaseboardSettings         = 0x64,
    // Device mode settings (MCU accepts OCP request header v1 and v2; see nsm_type_5.cpp)
    SetDeviceModeSettingsV2   = 0x82,
    GetDeviceModeSettingsV2   = 0x83,
    GetSupportedDeviceModesV2 = 0x84,
};

// NVIDIA Type FF (Internal) Command Codes
enum class NsmTypeFFCmdCode : uint8_t
{
    SetRackPowerSmoothingParam    = 0xA0,
    GetRackPowerSmoothingParam    = 0xA1,
    GetRackPowerSmoothingTestHook = 0xA2,
    SetRackPowerSmoothingTestHook = 0xA3,
    GetDebugTelemetry             = 0xA4,
    TriggerAdcCalibration         = 0xA5,  // Start ADC calibration test
    GetAdcCalibrationResults      = 0xA6,  // Retrieve calibration results
    AdcCalibSetLoopbackDacCode    = 0xA7,  // Set loopback DAC code (I2C); host allows settle
                                           // before read
    GetPowerSmoothRawReadback = 0xA8,      // Last raw SoC ADC or EDPP/ISINK DAC code (see
                                           // PwrSmoothRawReadbackId)
};

enum class DeviceModeIndex : uint32_t
{
    L1PowerMode                      = 0x00,
    NVLinkPortClock                  = 0x01,
    NVLinkAssymetricFlowTimers       = 0x02,
    DpuOperationMode                 = 0x03,
    PcieDeviceMode                   = 0x04,
    Bar0Firewall                     = 0x05,
    WprContiguousSize                = 0x06,
    MaxACPowerRampRate               = 0x07,
    SoCPowerSmoothEnabled            = 0x08,
    SoCPowerSmoothCurrentPresetIndex = 0x09,
    SoCPowerBrakeEnabled             = 0x0A,
    NcsiMac                          = 0x11,
    SoCThermBrakeEnabled             = 0x12,
    ProgramCpldFeatureRow            = 0x15,
    OtpCpldFeatureRow                = 0x16,
};

/** Sensors used in Type 3 - Get Temperature Reading */
enum Type3TemperatureSensors : uint8_t
{
    // Legacy Sensor ID for GB products

    TempGpu1          = 0,  // Temperature value of GPU1 in SXM7.1
    TempGpu2          = 1,  // Temperature value of GPU2 in SXM7.1
    TempTMP451_1      = 2,  // TMP451 ambient sensor temp 1
    TempTMP451_2      = 3,  // TMP451 ambient sensor temp 2
    TempMaxModule     = 4,  // Maximum of TMP451 sensors
    TempSMAInternal   = 5,  // Internal SMA temperature sensor
    TempCX8_1         = 6,  // CX8 1 Sensor
    TempCX8_2         = 7,  // CX8 2 Sensor
    GB_TempSensor_End = 8,

    // Sensor ID for VR products and future products that can compatible with existing NSM T3
    // Spec
    SMA_External   = 16,   // SMA External temperature sensor
    SMA_Internal   = 17,   // SMA Internal temperature sensor
    NvLink_Temp    = 18,   // NvLink temperature sensor
    BusBar_Temp    = 19,   // Busbar temperature sensor
    QM4_1_Temp     = 128,  // QM4 NVSwitch 1 temperature sensor
    QM4_2_Temp     = 129,  // QM4 NVSwitch 2 temperature sensor
    GPU1_Die_A     = 138,  // GPU1 Die A temperature sensor
    GPU1_Die_B     = 139,  // GPU1 Die B temperature sensor
    GPU2_Die_A     = 140,  // GPU2 Die A temperature sensor
    GPU2_Die_B     = 141,  // GPU2 Die B temperature sensor
    PCB_Temp_1     = 144,  // PCB temperature sensor
    PCB_Temp_2     = 145,  // PCB temperature sensor
    PCB_Temp_3     = 146,  // PCB temperature sensor 3
    PCB_Temp_4     = 147,  // PCB temperature sensor 4
    HSCC_Temp      = 168,  // HSCC temperature sensor. Fractal North HSCC
    HSCC_Temp_2    = 169,  // HSCC temperature sensor 2. Fractal South HSCC
    CPLD_Temp      = 190,  // CPLD temperature sensor
    HSC_Temp       = 192,  // HSC temperature sensor. Fractal East A
    HSC_Temp_2     = 193,  // HSC temperature sensor 2. Fractal East B
    HSC_Temp_3     = 194,  // HSC temperature sensor 3. Fractal West A
    HSC_Temp_4     = 195,  // HSC temperature sensor 4. Fractal West B
    HSC_Sby_Temp   = 196,  // HSC SBY temperature sensor
    CPU1_Die       = 216,  // CPU1 Die temperature sensor
    CPU1_SoC       = 217,  // CPU1 SoC temperature sensor
    CPU2_Die       = 218,  // CPU2 Die temperature sensor
    CPU2_SoC       = 219,  // CPU2 SoC temperature sensor
    TempSensor_End = 220,  // Last sensor ID + 1
};

enum I2cTempSensorIndex : uint8_t
{
    Port = 0,
    Addr,
    Id_Addr,
    ManufacturerId,
    TagId,
};

enum Type3PowerSensors : uint8_t
{
    // Legacy Power Sensor enum for GB products
    PowerGpu1   = 0,  // GPU1 power in SXM7.1
    PowerGpu2   = 1,  // GPU2 power in SXM7.1
    PowerModule = 2,  // SXM module power (sum of GPUs which are currently reported on HSC)
    GB_PowerSensor_End = 3,

    // Power sensors for VR products
    Power_Gpu1   = 9,    // GPU1 power
    Power_Gpu2   = 10,   // GPU2 power
    PowerHsc     = 128,  // HSC power, Fractal East A
    PowerHsc_2   = 129,  // HSC power 2, Fractal East B
    PowerHsc_3   = 130,  // HSC power 3, Fractal West A
    PowerHsc_4   = 131,  // HSC power 4, Fractal West B
    PowerHsc_Sby = 138,  // Fractal HSC SBY power
    PowerHscc    = 192,  // HSCC power, Fractal North HSCC
    PowerHscc_2  = 193,  // HSCC power 2, Fractal South HSCC
};

enum T3Voltage : uint8_t
{
    Voltage_Hsc_Vout     = 128,  // HSC voltage output, Fractal East A
    Voltage_Hsc_Vout_2   = 129,  // HSC voltage output 2, Fractal East B
    Voltage_Hsc_Vout_3   = 130,  // HSC voltage output 3, Fractal West A
    Voltage_Hsc_Vout_4   = 131,  // HSC voltage output 4, Fractal West B
    Voltage_Hsc_Sby_Vout = 138,  // Fractal HSC SBY voltage output
    Voltage_Hsc_Vin      = 192,  // HSC voltage input, Fractal East A
    Voltage_Hsc_Vin_2    = 193,  // HSC voltage input 2, Fractal East B
    Voltage_Hsc_Vin_3    = 194,  // HSC voltage input 3, Fractal West A
    Voltage_Hsc_Vin_4    = 195,  // HSC voltage input 4, Fractal West B
    Voltage_Hsc_Sby_Vin  = 202,  // Fractal HSC SBY voltage input
    Voltage_Hscc_Vout    = 208,  // HSCC voltage output, Fractal North HSCC
    Voltage_Hscc_Vout_2  = 209,  // HSCC voltage output 2, Fractal South HSCC
    Voltage_Hscc_Vin     = 224,  // HSCC voltage input, Fractal North HSCC
    Voltage_Hscc_Vin_2   = 225,  // HSCC voltage input 2, Fractal South HSCC
};

enum NsmPlatEnvLeakSensors : uint8_t
{
    NsmPlatEnvLeakSensorDripTray  = 0,
    NsmPlatEnvLeakSensorManifold  = 1,
    NsmPlatEnvLeakSensorOrchidCPX = 2,
    NsmPlatEnvLeakSensorBF4       = 3,
    NsmPlatEnvLeakSensorsSize
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
    // Sensors <= 230 (VR)
    // Voltage
    DIAG_VR_HSCC_Vin_1   = 176,
    DIAG_VR_HSCC_Vin_2   = 177,
    DIAG_VR_HSC_Vin_1    = 178,
    DIAG_VR_HSC_Vin_2    = 179,
    DIAG_VR_HSC_Vin_3    = 180,
    DIAG_VR_HSC_Vin_4    = 181,
    DIAG_VR_HSC_Vin_5    = 182,
    DIAG_VR_HSC_Sby_Vin  = DIAG_VR_HSC_Vin_5,
    DIAG_VR_HSCC_Vout_1  = 183,
    DIAG_VR_HSCC_Vout_2  = 184,
    DIAG_VR_HSC_Vout_1   = 185,
    DIAG_VR_HSC_Vout_2   = 186,
    DIAG_VR_HSC_Vout_3   = 187,
    DIAG_VR_HSC_Vout_4   = 188,
    DIAG_VR_HSC_Vout_5   = 189,
    DIAG_VR_HSC_Sby_Vout = DIAG_VR_HSC_Vout_5,
    // Power
    DIAG_VR_HSCC_POWER_1  = 190,
    DIAG_VR_HSCC_POWER_2  = 191,
    DIAG_VR_HSC_POWER_1   = 192,
    DIAG_VR_HSC_POWER_2   = 193,
    DIAG_VR_HSC_POWER_3   = 194,
    DIAG_VR_HSC_POWER_4   = 195,
    DIAG_VR_HSC_POWER_5   = 196,
    DIAG_VR_HSC_Sby_POWER = DIAG_VR_HSC_POWER_5,
    // Temperature
    DIAG_VR_HSCC_Temp    = 197,                 // HSCC temperature sensor
    DIAG_VR_HSCC_Temp_2  = 198,                 // HSCC temperature sensor 2
    DIAG_VR_HSC_Temp     = 199,                 // HSC temperature sensor
    DIAG_VR_HSC_Temp_2   = 200,                 // HSC temperature sensor 2
    DIAG_VR_HSC_Temp_3   = 201,                 // HSC temperature sensor 3
    DIAG_VR_HSC_Temp_4   = 202,                 // HSC temperature sensor 4
    DIAG_VR_HSC_Temp_5   = 203,                 // HSC temperature sensor 5
    DIAG_VR_HSC_Sby_Temp = DIAG_VR_HSC_Temp_5,  // HSC SBY temperature sensor
    DIAG_VR_CPU1_Die     = 204,                 // CPU1 Die temperature sensor
    DIAG_VR_CPU1_SoC     = 205,                 // CPU1 SoC temperature sensor
    DIAG_VR_CPU2_Die     = 206,                 // CPU2 Die temperature sensor
    DIAG_VR_CPU2_SoC     = 207,                 // CPU2 SoC temperature sensor
    DIAG_VR_PCB_Temp_1   = 208,                 // PCB temperature sensor
    DIAG_VR_PCB_Temp_2   = 209,                 // PCB temperature sensor
    DIAG_VR_PCB_Temp_3   = 210,                 // PCB temperature sensor
    DIAG_VR_PCB_Temp_4   = 211,                 // PCB temperature sensor
    DIAG_VR_GPU1_Die_A   = 212,                 // GPU1 Die A temperature sensor
    DIAG_VR_GPU1_Die_B   = 213,                 // GPU1 Die B temperature sensor
    DIAG_VR_GPU2_Die_A   = 214,                 // GPU2 Die A temperature sensor
    DIAG_VR_GPU2_Die_B   = 215,                 // GPU2 Die B temperature sensor
    DIAG_VR_CPLD_TEMP    = 216,                 // CPLD temperature sensor
    DIAG_VR_BUSBAR_TEMP  = 217,                 // BusBar temperature sensor
    DIAG_VR_NvLink_Temp  = 218,                 // NvLink temperature sensor
    DIAG_VR_SMA_External = 219,                 // SMA External temperature sensor

    // Sensors <= 230 (B300/GB300)
    DIAG_CX8_1_TEMP      = 220,                 // TemperatureTelemetry
    DIAG_CX8_2_TEMP      = 221,                 // TemperatureTelemetry
    DIAG_GPU1_TEMP       = 222,                 // TemperatureTelemetry
    DIAG_GPU2_TEMP       = 223,                 // TemperatureTelemetry
    DIAG_GPU1_POWER      = 224,                 // PowerTelemetry
    DIAG_GPU2_POWER      = 225,                 // PowerTelemetry
    DIAG_MODULE_POWER    = 226,                 // PowerTelemetry
    DIAG_MODULE_TEMP1    = 227,                 // TemperatureTelemetry
    DIAG_MODULE_TEMP2    = 228,                 // TemperatureTelemetry
    DIAG_INTERNAL_TEMP   = 229,                 // TemperatureTelemetry
    DIAG_VR_SMA_Internal = DIAG_INTERNAL_TEMP,  // same ID for Internal Temp
    DIAG_MAX_MODULE_TEMP = 230,                 // TemperatureTelemetry

    // Counters 231-250
    DIAG_I3C0_BUS_ERROR            = 231,  // i3c range 231-233 (4 items)
    DIAG_I2C_UPSTREAM_ERROR        = 234,  // i2c up range 234-235 (2 items)
    DIAG_I2C0_DOWNSTREAM_BUS_ERROR = 236,  // i2c down range 236-246 (11 items)
    DIAG_SPI0_DOWNSTREAM_BUS_ERROR = 247,  // spi down range 247-250 (4 items)

    DIAG_GPIO_VALUE_BITMAP = 254,  // GpioTelemetry
    DIAG_CurrentTimestamp  = 255   // TimestampTelemetry
};

enum Type4TelemetryTypes : uint8_t
{
    FirmwareInfoTelemetry = 0,
    PerformanceTelemetry  = 1,
    GpioTelemetry         = 2,
    ErrorCounterTelemetry = 3,
    PowerTelemetry        = 4,
    TemperatureTelemetry  = 5,
    VoltageTelemetry      = 6,
    TimestampTelemetry    = 7
};

enum class FanControlMode : uint8_t
{
    Disabled    = 0x00,
    Enabled     = 0x01,
    Steady      = 0x02,
    Rpm         = 0x03,
    Temperature = 0x04,
};

// NSM T4 Enable Disable Write Protection (0x65)
enum Type4WriteProtectionFunction : uint8_t
{
    WP_BASEBOARD_FRU_EEPROM = 129,  // Baseboard FRU EEPROM
    WP_NVSW_QM4_SPI         = 131,  // NVSW (QM4) SPI
    WP_GPU_SPI              = 170,  // GPU SPI
    WP_CX9_SPI              = 183,  // CX9 SPI
};

// Power Sensor Faults device selector
enum class PowerSensorFaults : uint8_t
{
    HSC     = 128,
    HSC_2   = 129,
    HSC_3   = 130,
    HSC_4   = 131,
    HSC_Sby = 132,
    HSCC    = 192,
    HSCC_2  = 193,
};

enum GpioEventSource : uint8_t
{
    PhysicalGpio = 0,
    VirtualGpio  = 1,
    SpoofingGpio = 2,
};

}  // namespace nv::mctp
