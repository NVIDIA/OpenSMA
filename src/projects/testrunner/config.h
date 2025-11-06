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

// USB configuration for C and C++ definitions
#define USB_CONFIG_MCTP (1U)

#ifdef __cplusplus
#pragma once
#include <array>
#include <cstdint>
#include <tuple>
#include <functional>
#include <optional>

#include "nv/spi/common.h"
#include "nv/gpio/common.h"
#include "nv/i2c/sensor.h"
#include "nv/mctp/router.h"
#include "nv/watchdog/notify_interface.h"
#include "nv/i2c/port.h"
#include "nv/ipchandler/enums.h"
#include "nv/mctp/enums.h"
#include "nv/telemetry/utils.h"
#include "sys/common/common.h"
#include "nv/iox/common.h"
#include "nv/pldm/common.h"
#include "nv/leak_det/common.h"

// CPLD configuration (for test/mock purposes)
constexpr nv::i2c::Port CPLD_I2C_PORT = nv::i2c::Port::Zero;

namespace nv::telemetry {

/** A mapping of telemetry id to index in the cache */
constexpr uint8_t TelemIndexMapSize = 13;
using TelemIndexMap                 = std::tuple<nv::telemetry::TelemId, int>;
constexpr inline std::array<TelemIndexMap, TelemIndexMapSize> TelemIndexMapList{
    TelemIndexMap{     nv::telemetry::TelemId::Gpu1Temp,  0},
    TelemIndexMap{     nv::telemetry::TelemId::Gpu2Temp,  1},
    TelemIndexMap{    nv::telemetry::TelemId::Gpu1Power,  2},
    TelemIndexMap{    nv::telemetry::TelemId::Gpu2Power,  3},
    TelemIndexMap{  nv::telemetry::TelemId::ModulePower,  4},
    TelemIndexMap{  nv::telemetry::TelemId::ModuleTemp1,  5},
    TelemIndexMap{  nv::telemetry::TelemId::ModuleTemp2,  6},
    TelemIndexMap{ nv::telemetry::TelemId::InternalTemp,  7},
    TelemIndexMap{nv::telemetry::TelemId::MaxModuleTemp,  8},
    TelemIndexMap{         nv::telemetry::TelemId::Gpio,  9},
    TelemIndexMap{   nv::telemetry::TelemId::CX8_1_Temp, -1},
    TelemIndexMap{   nv::telemetry::TelemId::CX8_2_Temp, -1},
};

}  // namespace nv::telemetry

namespace nv::ipc {
// Indicate if it is dual core project
constexpr bool EnableDualCore = false;

/// All Tasks must be part of this enum.
enum class TaskId
{
    Begin,
    TestRunner = Begin,
    NHP,
    Mctp,
    I2c0,
    Pldm,
    Iox,
#ifdef NV_UNITTEST
    Unittest,
#endif
    Logger,
    Spdm,
    GpuPwrController,
    Core0,
    Core1,
    Privileged = Core1 + 1,
    Usb        = Privileged + 0,
    Flash      = Privileged + 1,
    EndPrivileged,
    End   = EndPrivileged,
    Timer = End,
    Idle,
    KernelEnd
};

/// All Queues must be part of this enum.
enum class QueueId
{
    Begin,
    Test1 = Begin,
    Test2,
    Test3,
    Test4,
    Test5,
    Test6,
    Test7,
    Test8,
    MctpDataRequest,
    MctpPldmRequest,
    MctpSpdmRequest,
    MctpCmd,
    Core0,
    Core1,
    I2c0,
    I2c1,
    I2c2,
    I2c3,
    I2c4,
    I2c5,
    I3c0,
    I3c1,
    Spi0,
    Spi1,
    Spi2,
    PldmRx,
    PldmRx4k,
    UsbTx,
    FlashRequest,
    FlashResponse,
    RoutingTable,
    LogRequest,
    LogResponseBlocking,
    LogDownload,
    LogDownloadResp,
    LogISR,
    FlashSema,
    SpdmRx,
    SpdmCryptoHelper,
    UsbHid,
    Iox,
    UsbToSpi,
    SpiToUsb,
    End
};

// Down stream Information
constexpr uint8_t DownStreamNum           = 0;
constexpr uint8_t UpStreamNum             = 1;
constexpr uint8_t DefaultRoutingTableSize = DownStreamNum + UpStreamNum;
constexpr uint8_t RoutingInfoUpdateSize   = 0;
constexpr uint8_t RoutingTableSize        = DefaultRoutingTableSize + RoutingInfoUpdateSize;

constexpr inline std::array<mctp::DownStreamInfo, DownStreamNum> DownStreamInfos{

};

constexpr uint32_t SpdmRequestQueueSize      = 2048;
constexpr uint32_t SpdmRxQueueSize           = 72;
constexpr uint32_t SpdmCryptoHelperQueueSize = 12;
constexpr bool     SpdmI2cResponder          = false;
constexpr bool     SpdmDummyCertificates     = false;
constexpr uint32_t SpdmCryptoHelperMaxItems  = EnableDualCore ? 2 : 1;
using QueueInfo                              = std::tuple<QueueId, uint8_t, uint16_t>;
// USB to SPI bridge (enable to support flashrom -p NV_SMA_SPI)
constexpr bool     EnableFlashrom = false;
constexpr uint32_t UsbSpiMsgSize  = EnableFlashrom ? 512 : 1;

/// define all queue lengths and item_sizes here
constexpr inline std::array<QueueInfo, int(QueueId::End)> QueueInfos{
    // id, len, item_size
    QueueInfo{              QueueId::Test1,1,                                               1024                                           },
    {              QueueId::Test2,                        2,                                                512},
    {              QueueId::Test3,                        4,                                                256},
    {              QueueId::Test4,                        8,                                                128},
    {              QueueId::Test5,                       16,                                                 64},
    {              QueueId::Test6,                       32,                                                 32},
    {              QueueId::Test7,                       64,                                                 16},
    {              QueueId::Test8,                      128,                                                  8},
    {    QueueId::MctpDataRequest,                      130,                                                 72},
    {    QueueId::MctpPldmRequest,                        1,                                                256},
    {    QueueId::MctpSpdmRequest,                        1,                               SpdmRequestQueueSize},
    {            QueueId::MctpCmd,                      130,                                                  4},
    {              QueueId::Core0,                        1,                                                 72},
    {              QueueId::Core1,                        1,                                                 72},
    {               QueueId::I2c0,                       64,                                                 80},
    {               QueueId::I2c1,                       64,                                                 80},
    {               QueueId::I2c2,                       64,                                                 80},
    {               QueueId::I2c3,                       64,                                                 80},
    {               QueueId::I2c4,                       64,                                                 80},
    {               QueueId::I2c5,                        1,                                                  1},
    {               QueueId::I3c0,                       64,                                                 72},
    {               QueueId::I3c1,                       64,                                                 72},
    {               QueueId::Spi0,                        1,                                                  1},
    {               QueueId::Spi1,                        1,                                                  1},
    {               QueueId::Spi2,                        1,                                                  1},
    {             QueueId::PldmRx,                        1,                                                 72},
    {           QueueId::PldmRx4k,                        1,                                          4096 + 32},
    {              QueueId::UsbTx,                        1,                                                 72},
    {       QueueId::FlashRequest,                        1,                                                280},
    {      QueueId::FlashResponse,                        1,                                                272},
    {       QueueId::RoutingTable,
              RoutingTableSize, sizeof(mctp::ShardRoutingTable) * RoutingTableSize                                      },
    {         QueueId::LogRequest,                       64,                                                 22},
    {QueueId::LogResponseBlocking,                        1,                                                 22},
    {        QueueId::LogDownload,                        1,                                                 22},
    {    QueueId::LogDownloadResp,                        1,                                                 22},
    {             QueueId::LogISR,                        6,                                                 22},
    {          QueueId::FlashSema,                        1,                                                  1},
    {             QueueId::SpdmRx,                        1,                                    SpdmRxQueueSize},
    {   QueueId::SpdmCryptoHelper, SpdmCryptoHelperMaxItems,                          SpdmCryptoHelperQueueSize},
    {             QueueId::UsbHid,                        1,                                                 68},
    {           QueueId::UsbToSpi,                        1,                                      UsbSpiMsgSize},
    {           QueueId::SpiToUsb,                        1,                                      UsbSpiMsgSize},
    {                QueueId::Iox,                        8,                                                 80}
};
/// All Events must be part of this enum.
enum class EventId
{
    Begin,
    Nhp,
    Test1 = Begin,
    Test2,
    Test3,
    Test4,
    Core0,
    Core1,
    I2c0,
    PldmTask,
    UsbTask,
    FlashEvent,
    Spi0Event,
    Spi1Event,
    Spi2Event,
    LogEvent,
    SpdmTask,
    TaskBootStatus,
    TaskAliveStatus,
    GpuPwrCtrlEvent,
    Spi0EdmaDriverEvent,
    End
};

/// All Timers must be part of this enum
enum class TimerId
{
    Begin,
    Test1 = Begin,
    Test2 = Begin,
    MctpEnumerate,
    Pldm,
    PerfMonitor,
    MctpApPowerGoodEngage,
    Bootloader,
    RuntimeWatchDog,
    Gpu1Seneor,
    Gpu2Seneor,
    SmbSensor,
    Ap1Status,
    Ap2Status,
    Ap3Status,
    End
};

// Limits
[[maybe_unused]] constexpr auto MaxQueuesPerTask = 4;
[[maybe_unused]] constexpr auto MaxEventsPerTask = 4;

constexpr uint8_t GpioNum = 1;
enum Port : gpio::GpioPort
{
    ApGoodPort   = nv::gpio::InvalidGpioPort,
    GlobalWpPort = nv::gpio::InvalidGpioPort,

    // Port 4
    THERM_OVERT_N_PORT = 4
};

enum Pin : gpio::GpioPin
{
    ApGoodPin   = nv::gpio::InvalidGpioPin,
    GlobalWpPin = nv::gpio::InvalidGpioPin,

    // Port 4 pins
    THERM_OVERT_N_PIN = 15
};

using Gpios = std::tuple<gpio::GpioPort, gpio::GpioPin>;

constexpr inline std::array<Gpios, GpioNum> GpioSetup{
    // Port 4
    Gpios{THERM_OVERT_N_PORT, THERM_OVERT_N_PIN}, // IN
};

constexpr uint32_t CtimerFrequency = 48000000;
constexpr auto     UartInstance    = 2;

enum BootedEventBits : uint32_t
{
    Mctp  = nv::common::bit(0),
    I2c0  = nv::common::bit(1),
    Pldm  = nv::common::bit(2),
    Iox   = nv::common::bit(3),
    Usb   = nv::common::bit(4),
    Flash = nv::common::bit(5),
#ifdef NV_UNITTEST
    Unittest,
#endif
    Logger         = nv::common::bit(6),
    Spdm           = nv::common::bit(7),
    GpuPwrCtrl     = nv::common::bit(8),
    BootStatusMask = (nv::common::bit(9) - 1),
};

constexpr uint32_t WatchdogResetMs       = 2000;
constexpr uint32_t CheckTaskBootStatusMs = 1000;

// USB config
constexpr uint16_t UsbDeviceVid = 0x0955U;
constexpr uint16_t UsbDevicePid = 0xCF10U;

// Runtime WDT
constexpr uint32_t RuntimeWatchdogResetMs = 1000;
constexpr uint32_t SupervisorCheckMs      = 500;
constexpr uint32_t SupervisorCheckUs      = SupervisorCheckMs * 1000;

constexpr std::array<nv::watchdog::TaskMonitorIndex, 2> TaskMonitorList{
    nv::watchdog::TaskMonitorIndex::Flash, nv::watchdog::TaskMonitorIndex::Logger};

constexpr bool EnableRuntimeWdt = false;

constexpr bool EnableCP2112NativeGpio = false;

// Debugtoken config
constexpr bool DebugTokenEnabled = false;

// I2C config
constexpr bool EnableSmbDirect = false;
// I2C scan VDM support
constexpr bool EnableI2cScanVdm = true;
// I2c virtual address mapping configuration
constexpr bool I2cManualNackMode = true;  // true, when mapping is not existed in the
                                          // I2cVirtualAddressMappingTable, command will send
                                          // back as Nack directly, otherwise the i2c command
                                          // will send according to I2cDefaultInhandlerId
constexpr nv::ipchandler::Id
    I2cDefaultInhandlerId = nv::ipchandler::Id::I3c1;  // this variable
                                                       // only use when
                                                       // I2cManualNackMode
                                                       // is false
enum class I2cDynamicAddressType : uint8_t
{
    Begin,
    NotDynamicType = Begin,
    // please add new dynamic address type here
    End,
};
// should be defined when declare dynamic address type in I2cDynamicAddressType
std::optional<uint8_t>
find_i2c_dynamic_virtual_address(I2cDynamicAddressType dynamic_address_type);

struct I2cVirtualAddressMappingTableItem
{
    uint8_t               virtual_address;
    uint8_t               physical_address;
    bool                  is_ocp_device;
    QueueId               queue_id;
    nv::ipchandler::Id    ipchandler_id;
    I2cDynamicAddressType dynamic_address_type = I2cDynamicAddressType::NotDynamicType;
};

namespace {
constexpr auto GetI2cVirtualMappingTable()
{
    // user define I3c | I2c address mapping
    // please define the I2c addr mapping through usb hid here as the invoke parameter
    constexpr auto I2cAddrMappingList = std::invoke(
        []<typename... Args>
        requires std::conjunction_v<std::is_same<Args, I2cVirtualAddressMappingTableItem>...>
        (const Args... MappingItems) constexpr {
            return std::array<I2cVirtualAddressMappingTableItem, sizeof...(MappingItems)>{
                MappingItems...};
        });
    // skip check when not I2c mapping is not needed
    if constexpr (I2cAddrMappingList.size() == 0) {
        return I2cAddrMappingList;
    }

    // check the mapping table is valid for the following rule
    // 1. address not exceed 0x7f
    constexpr auto IsI2cAddrRangeValid = [](const auto I2cAddrMappingList) constexpr {
        for (const auto& MappingItem : I2cAddrMappingList) {
            if (MappingItem.physical_address > 0x7f || MappingItem.virtual_address > 0x7f) {
                return false;
            }
        }
        return true;
    };
    static_assert(IsI2cAddrRangeValid(I2cAddrMappingList) == true,
                  "The I2c mapping address exceed 0x7f.");
    // 2. should not have duplicate virtual address
    constexpr auto IsI2cAddrDuplicate = [](const auto I2cAddrMappingList) constexpr {
        for (const auto& MappingItem : I2cAddrMappingList) {
            std::array<uint8_t, 0x7f + 1> count_table{};
            count_table.fill(0x00);
            if (count_table.at(MappingItem.virtual_address) == 0) {
                ++count_table.at(MappingItem.virtual_address);
            }
            else {
                return true;
            }
        }
        return false;
    };
    static_assert(IsI2cAddrDuplicate(I2cAddrMappingList) == false,
                  "The virtual_address element in I2c mapping table has duplicate one.");
    // 3. the same ipchandler_id should mapping to same queue_id.
    constexpr auto IsIpchandlerIdMappingToSameQueueId =
        [](const auto I2cAddrMappingList) constexpr {
            // not optimize check O(n^2)
            for (const auto& MappingItem1 : I2cAddrMappingList) {
                for (const auto& MappingItem2 : I2cAddrMappingList) {
                    if (&MappingItem2 == &MappingItem1) {
                        continue;
                    }
                    if (MappingItem1.ipchandler_id == MappingItem2.ipchandler_id
                        && MappingItem1.queue_id != MappingItem2.queue_id) {
                        return false;
                    }
                }
            }
            return true;
        };
    static_assert(IsIpchandlerIdMappingToSameQueueId(I2cAddrMappingList) == true,
                  "the mapping between ipchandler_id and queue_id is not unique.");
    return I2cAddrMappingList;
}

}  // namespace

constexpr auto I2cVirtualAddressMappingTable = GetI2cVirtualMappingTable();
// Telemetry
constexpr uint32_t SensorUpdateMs      = 0;
constexpr uint8_t  InternalTempWarnBit = 0;
constexpr uint8_t  I2cSensorAlertBit   = 0;
constexpr uint8_t  GpioTelemetrySize   = 0;
using GpioTelemetry                    = std::tuple<Port, Pin, uint8_t>;
constexpr inline std::array<GpioTelemetry, GpioTelemetrySize> GpioTelemetryTable{};
constexpr uint8_t                                             ModuleTempSensorSize = 0;
using ModuleTempSensor = std::tuple<nv::i2c::Port, uint8_t, nv::telemetry::TelemId>;
constexpr inline std::array<ModuleTempSensor, ModuleTempSensorSize> ModuleTempSensorList{};

// FPGA WAR: Bug ?
constexpr bool I2cIsEndpoint = false;

// For CX8 I3C init flow, CX8 need time to handle RSTDAA
constexpr bool EnableDelayInI3CInit = false;
// AP status ping interval for discovery notify when no GPIO is available
constexpr uint32_t CheckApStatusTimerUs = 10 * 1000 * 1000;
// Use the I2C AP status timer if I3C is not enabled
constexpr bool UseI2cApStatusTimer = false;
// GPU I3C pull up status pin
constexpr bool               I3CPullUpCheck = false;
constexpr nv::gpio::GpioPort I3CPullUpPort  = 0;
constexpr nv::gpio::GpioPin  I3CPullUpPin   = 0;

// Indicate if spi be used
constexpr bool Spi_Available = false;
// I2C transparent
constexpr bool I2cTransparent = false;
// NSM type 5 messages
constexpr bool Enable_Nsm_type5 = true;
// NSM type 3 messages
constexpr bool Enable_Nsm_type3 = true;
// NSM type 4 messages
constexpr bool Enable_Nsm_type4 = true;

/******** ******** Iox Emulation Config Starts ******** ********/
constexpr bool                                          EnableIoxEmulation = false;
constexpr inline size_t                                 IoxNum             = 0;
constexpr inline uint8_t                                IoxI2cBaseAddr     = 0x50;
constexpr inline std::array<nv::iox::IoxConfig, IoxNum> IoxConfigs{};
/******** ******** Iox Emulation Config Ends ******** ********/

/******** ******** Leak Detect Config Starts ******** ********/
namespace leak_detect_config {

using namespace nv::leak_detect;
using namespace nv::mctp;

constexpr bool EnableDbgInfo = true;

constexpr bool SensorOnAdc0 = true;
constexpr bool SensorOnAdc1 = false;

constexpr size_t   LeakDetectSensorNum                     = 0;
constexpr SensorId LeakDetectSensorId[LeakDetectSensorNum] = {};

constexpr gpio::GpioPort AlertGpioPort = nv::gpio::InvalidGpioPort;
constexpr gpio::GpioPin  AlertGpioPin  = nv::gpio::InvalidGpioPin;

template<size_t Index>
constexpr LeakDetectSensor get_sensor_config()
{
    static_assert(Index < LeakDetectSensorNum, "Sensor index out of range");
    // TODO: add sensor config here...
    return {};
}

/** do not change this function */
template<size_t... Indices>
inline void init_sensors_impl(LeakDetectSensor* sensors, std::index_sequence<Indices...>)
{
    ((sensors[Indices] = get_sensor_config<Indices>()), ...);
}

}  // namespace leak_detect_config
/******** ******** Leak Detect Config Ends ******** ********/

// SMA_READY pin configuration
constexpr bool               EnableMcuReadyPin = false;
constexpr nv::gpio::GpioPort McuReadyPinPort   = 0;
constexpr nv::gpio::GpioPin  McuReadyPinPin    = 0;

constexpr bool EnableForwardNvlInfo = false;
}  // namespace nv::ipc

namespace nv::mctp {

constexpr auto mcuTemperatureSensorsSize = 6;
/** List of active MCU sensors for NSM type 3 Temperature Reading */
constexpr inline std::array<Type3TemperatureSensors, mcuTemperatureSensorsSize>
    mcuTemperatureSensors{
        TempGpu1, TempGpu2, TempTMP451_1, TempTMP451_2, TempMaxModule, TempSMAInternal};

constexpr auto mcuPowerSensorsSize = 3;
/** List of active MCU sensors for NSM type 3 Power Draw */
constexpr inline std::array<Type3PowerSensors, mcuPowerSensorsSize> mcuPowerSensors{
    PowerGpu1, PowerGpu2, PowerModule};

// Type 4 Diagnostics Telemetries
constexpr auto T4TelemetriesSize = 10;
using T3TagId                    = uint8_t;
constexpr T3TagId NoT3Tag        = 0xff;
using McuDiagnosticTelemetry     = std::
    tuple<Type4McuDiagnosticEntries, Type4TelemetryTypes, T3TagId>;
constexpr inline std::array<McuDiagnosticTelemetry, T4TelemetriesSize> mcuDiagnosticTelemetries{
    McuDiagnosticTelemetry{DIAG_GPIO_VALUE_BITMAP,             GpioTelemetry,         NoT3Tag},
    McuDiagnosticTelemetry{        DIAG_GPU1_TEMP, CacheTemperatureTelemetry,        TempGpu1},
    McuDiagnosticTelemetry{        DIAG_GPU2_TEMP, CacheTemperatureTelemetry,        TempGpu2},
    McuDiagnosticTelemetry{     DIAG_MODULE_TEMP1, CacheTemperatureTelemetry,    TempTMP451_1},
    McuDiagnosticTelemetry{     DIAG_MODULE_TEMP2, CacheTemperatureTelemetry,    TempTMP451_2},
    McuDiagnosticTelemetry{    DIAG_INTERNAL_TEMP, CacheTemperatureTelemetry, TempSMAInternal},
    McuDiagnosticTelemetry{  DIAG_MAX_MODULE_TEMP, CacheTemperatureTelemetry,   TempMaxModule},
    McuDiagnosticTelemetry{       DIAG_GPU1_POWER,       CachePowerTelemetry,       PowerGpu1},
    McuDiagnosticTelemetry{       DIAG_GPU2_POWER,       CachePowerTelemetry,       PowerGpu2},
    McuDiagnosticTelemetry{     DIAG_MODULE_POWER,       CachePowerTelemetry,     PowerModule}
};

// Temperature Sensors Config for VR products
constexpr uint8_t I2cTempSensorSize = 0;

// This data is only shared between main.cpp and mctp task. Should not impact when migrating to
// dual core.
NV_SHARED_DATA inline std::array<nv::i2c::I2cTempSensorConfig, I2cTempSensorSize>
    I2cTempSensorList{};

}  // namespace nv::mctp

namespace nv::perf_mon {
constexpr uintptr_t __max_fw_size  = 0xFF10;
constexpr uintptr_t __text_size    = 0x1010;
constexpr uintptr_t __max_ram_size = 0x2020;
constexpr uintptr_t __data_size    = 0x0A0A;

}  // namespace nv::perf_mon

namespace nv::nhp {
// TODO: Add minimal content required here to get testrunner builds to function
#define NHP_VPP 0
#define NHP_NHP 1
#define NHP_AHS 2
#ifndef NHP_PROTOCOL
#define NHP_PROTOCOL NHP_AHS
#endif
#ifndef NUM_HOTPLUG_INSTANCES
#define NUM_HOTPLUG_INSTANCES 4
#endif
#ifndef NUM_E1S_DRIVES
#define NUM_E1S_DRIVES (8U / NUM_HOTPLUG_INSTANCES)
#endif

constexpr static uint8_t NhpProtocol     = static_cast<uint8_t>(NHP_PROTOCOL);
constexpr static uint8_t NumNhpInstances = static_cast<uint8_t>(NUM_HOTPLUG_INSTANCES);
constexpr static uint8_t NumE1sDrives    = static_cast<uint8_t>(NUM_E1S_DRIVES);
constexpr static uint8_t NumAdcTriggers  = 2U;
constexpr std::array<uint32_t, NumAdcTriggers> AdcTriggers             = {1U, 2U};
constexpr static int                           MAX_ADC_POLL_ITERATIONS = 1000;
constexpr static uint16_t                      MappingVersion          = 0b001;
constexpr static uint16_t                      PgoodThreshold          = 0xE8BAU;
constexpr static uint16_t                      NumOfPartitions         = 0b01;
constexpr static uint32_t                      ClkEnToPerstDelayUs     = 400;

}  // namespace nv::nhp

namespace nv::pldm {

// AP component ID Information
constexpr uint8_t                          ApNum            = 0;
constexpr std::array<uint16_t, ApNum>      AllApComponentId = {{}};
constexpr inline std::array<FwInfo, ApNum> FwInfoList{{}};
static_assert(ApNum < NV_PLDM_MAX_COMPONENT_SIZE, "ApNum should be less than 3");
}  // namespace nv::pldm

constexpr bool CPLD_ProgramN_Pin_Enabled = false;

#endif
