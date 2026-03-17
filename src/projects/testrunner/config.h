/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

// Debug console configuration (NV_UART_INSTANCE, etc.)
#include "DebugConsoleConfig.h"

#ifdef __cplusplus
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <tuple>

#include "powersensor.h"

#include "nv/lstp/lstp_common.h"
#include "nv/gpio/common.h"
#include "nv/i2c/port.h"
#include "nv/i2c/sensor.h"
#include "nv/i2c/smb_direct.h"
#include "nv/iox/common.h"
#include "nv/i2c/slave_function.h"
#include "nv/ipchandler/enums.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/nsm_common.h"
#include "nv/mctp/nsm_event.h"
#include "nv/mctp/nsm_msg_bitmask.h"
#include "nv/mctp/router.h"
#include "nv/pldm/common.h"
#include "nv/spi/common.h"
#include "nv/telemetry/utils.h"
#include "nv/volt_mon/common.h"
#include "nv/vruart/common.h"
#include "nv/watchdog/notify_interface.h"
#include "sys/common/common.h"

// CPLD configuration (for test/mock purposes)
#define ENABLE_CPLD_PLDM 1
#ifdef ENABLE_CPLD_PLDM
constexpr nv::i2c::Port CPLD_I2C_PORT_PRGM        = nv::i2c::Port::Zero;
constexpr uint8_t       CPLD_I2C_ADDR_PRGM        = 0x40;
constexpr nv::i2c::Port CPLD_I2C_PORT_USR         = nv::i2c::Port::Zero;
constexpr uint8_t       CPLD_I2C_ADDR_USR         = 0x0B;
constexpr uint8_t       CPLD_I2C_ADDR_DBG         = 0x0F;
constexpr uint8_t       CPLD_I2C_ADDR_DBG_INSTALL = 0x11;

constexpr bool CPLD_ProgramN_Pin_Enabled = false;
#endif  // ENABLE_CPLD_PLDM

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
constexpr bool EnableLstp = false;
}  // namespace nv::ipc

namespace nv::lstp {

constexpr uint16_t                                       LstpGpioNum = 0;
constexpr inline std::array<LstpGpioConfig, LstpGpioNum> PinConfigs{};
constexpr inline std::array<uint8_t, LstpGpioNum>        LstpGpioMap = {};

constexpr uint8_t LstpNumChannels = 0;

// clang-format off
constexpr std::array<LstpChannelEntry, LstpNumChannels> LstpChannels{};
// clang-format on

static_assert(ValidateLstpChannelConfigs(LstpChannels));
static_assert(nv::ipc::EnableLstp == (LstpNumChannels > 0));

constexpr bool EnableSpi  = IsChannelEnabled(LstpChannels, LstpChannelType::Spi);
constexpr bool EnableGpio = IsChannelEnabled(LstpChannels, LstpChannelType::Gpio);
constexpr bool EnableI2c  = IsChannelEnabled(LstpChannels, LstpChannelType::I2c);
constexpr bool EnableIpmi = IsChannelEnabled(LstpChannels, LstpChannelType::Ipmi);

}  // namespace nv::lstp

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
    Lstp,
    Pldm,
    Iox,
#ifdef NV_UNITTEST
    Unittest,
#endif
    Logger,
    Spdm,
    SocPwrSmoothing,
    GpuPwrController,
    Core0,
    Core1,
    Privileged = Core1 + 1,
    Usb        = Privileged + 0,
    Ubridge    = Privileged + 1,
    Flash      = Privileged + 2,
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
    I2c6,
    I2c7,
    I2c8,
    I2c9,
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
    LstpToSpi,
    LstpTx,
    LstpGpioIrq,
    LstpToGpio,
    Iox,
    UbridgeTx,
    Ssif,
    End
};

/// All Mutexes must be part of this enum.
enum class MutexId
{
    Begin,
    I2cPort0 = Begin,
    I2cPort1,
    I2cPort2,
    I2cPort3,
    I2cPort4,
    I2cPort5,
    I2cPort6,
    I2cPort7,
    I2cPort8,
    I2cPort9,
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

constexpr uint32_t ApEcdsa384PublicKeySize = 96;
constexpr std::array<std::array<uint8_t, ApEcdsa384PublicKeySize>, 2> ApFwPublicKeys{};

constexpr uint32_t SpdmRequestQueueSize      = 2048;
constexpr uint32_t SpdmRxQueueSize           = 72;
constexpr uint32_t SpdmCryptoHelperQueueSize = 12;
constexpr bool     SpdmI2cResponder          = false;
constexpr bool     SpdmDummyCertificates     = false;
constexpr uint32_t SpdmCryptoHelperMaxItems  = EnableDualCore ? 2 : 1;
using QueueInfo                              = std::tuple<QueueId, uint8_t, uint16_t>;

// USB Low-Speed Transport Protocol (LSTP) for I2C/SSIF/SPI/GPIO tunneling
constexpr uint32_t UsbLstpMsgSize        = EnableLstp ? 512 : 1;
constexpr uint32_t LstpToGpioSize        = nv::lstp::EnableGpio ? UsbLstpMsgSize : 1;
constexpr uint32_t LstpGpioIrqQueueSize  = nv::lstp::EnableGpio
                                             ? sizeof(nv::lstp::LstpGpioIrqEventRequest)
                                             : 1;
constexpr uint32_t LstpGpioIrqQueueDepth = nv::lstp::EnableGpio ? nv::lstp::LstpGpioNum : 1;

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
    {            QueueId::MctpCmd,                      130,                                                 12},
    {              QueueId::Core0,                        1,                                                 72},
    {              QueueId::Core1,                        1,                                                 72},
    {               QueueId::I2c0,                       64,                                                 80},
    {               QueueId::I2c1,                       64,                                                 80},
    {               QueueId::I2c2,                       64,                                                 80},
    {               QueueId::I2c3,                       64,                                                 80},
    {               QueueId::I2c4,                       64,                                                 80},
    {               QueueId::I2c5,                        1,                                                  1},
    {               QueueId::I2c6,                        1,                                                  1},
    {               QueueId::I2c7,                        1,                                                  1},
    {               QueueId::I2c8,                        1,                                                  1},
    {               QueueId::I2c9,                        1,                                                  1},
    {               QueueId::I3c0,                       80,                                                 76},
    {               QueueId::I3c1,                       80,                                                 76},
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
    {          QueueId::LstpToSpi,                        1,                                                  1},
    {             QueueId::LstpTx,                        1,                                     UsbLstpMsgSize},
    {        QueueId::LstpGpioIrq,    LstpGpioIrqQueueDepth,                               LstpGpioIrqQueueSize},
    {         QueueId::LstpToGpio,                        1,                                     LstpToGpioSize},
    {                QueueId::Iox,                        8,                                                 80},
    {          QueueId::UbridgeTx,                        4,                                                514},
    {               QueueId::Ssif,                        1,                                                  1}
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
    Lstp,
    UartBridgeEvent,
    Ssif,
    End
};

constexpr uint32_t I3cQueueMaxTxSize = 64;

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
    SmbusCacheRefresh,
    Ap1Status,
    Ap2Status,
    Ap3Status,
    EepromUpdate,
    End
};

using I2cTimerInfo = std::tuple<TimerId, QueueId>;

constexpr inline std::array<I2cTimerInfo, 0> I2cTimerInfos{};

// Limits
[[maybe_unused]] constexpr auto MaxQueuesPerTask = 4;
[[maybe_unused]] constexpr auto MaxEventsPerTask = 4;

constexpr uint8_t GpioNum = 3;
enum Port : gpio::GpioPort
{
    ApGoodPort   = nv::gpio::InvalidGpioPort,
    GlobalWpPort = nv::gpio::InvalidGpioPort,

    // Port 1
    MCU_PWR_BRAKE_L_PORT = 1,

    // Port 4
    THERM_OVERT_N_PORT = 4,

    // Port 5
    MCU_THERM_WARN_L_PORT = 5,
};

enum Pin : gpio::GpioPin
{
    ApGoodPin   = nv::gpio::InvalidGpioPin,
    GlobalWpPin = nv::gpio::InvalidGpioPin,
    // Port 1
    MCU_PWR_BRAKE_L_PIN = 16,

    // Port 4 pins
    THERM_OVERT_N_PIN = 15,

    // Port 5
    MCU_THERM_WARN_L_PIN = 7,
};

using Gpios = std::tuple<gpio::GpioPort, gpio::GpioPin>;

constexpr inline std::array<Gpios, GpioNum> GpioSetup{
    // Port 1
    Gpios{ MCU_PWR_BRAKE_L_PORT,  MCU_PWR_BRAKE_L_PIN}, // OUT
    // Port 4
    Gpios{   THERM_OVERT_N_PORT,    THERM_OVERT_N_PIN}, // IN
    // Port 5
    Gpios{MCU_THERM_WARN_L_PORT, MCU_THERM_WARN_L_PIN}  // IN
};

// Product-specific NSM Event GPIO configuration
constexpr uint8_t                                            NsmEventGpioNum   = 0;
constexpr std::array<nv::ipc::NsmEventGpio, NsmEventGpioNum> GpioNsmEventSetup = {};

// GPIO NSM Event mask arrays - automatically generated from GpioNsmEventSetup
constexpr inline std::array<uint32_t, sys::gpio::PortsNumber + 1>
    GpioNsmEventMask = nv::ipc::PopulateGpioNsmEventMaskArray(GpioNsmEventSetup);

constexpr inline std::array<uint32_t, sys::gpio::PortsNumber + 1>
    GpioNsmEventAssertMask = nv::ipc::PopulateGpioNsmEventAssertMaskArray(GpioNsmEventSetup);

using GpioInterruptConfig = std::tuple<nv::gpio::GpioPort,
                                       nv::gpio::GpioPin,
                                       nv::gpio::InterruptDetection,
                                       nv::gpio::InterruptSelect>;
constexpr int                                                      GpioInterruptNum = 0;
constexpr inline std::array<GpioInterruptConfig, GpioInterruptNum> GpioInterruptSetup{};

constexpr uint32_t CtimerFrequency = 48000000;
constexpr auto     UartInstance    = NV_UART_INSTANCE;

enum BootedEventBits : uint32_t
{
    Mctp    = nv::common::bit(0),
    I2c0    = nv::common::bit(1),
    Pldm    = nv::common::bit(2),
    Iox     = nv::common::bit(3),
    Usb     = nv::common::bit(4),
    Flash   = nv::common::bit(5),
    Ubridge = nv::common::bit(6),
#ifdef NV_UNITTEST
    Unittest,
#endif
    Logger          = nv::common::bit(7),
    Spdm            = nv::common::bit(8),
    GpuPwrCtrl      = nv::common::bit(9),
    SocPwrSmoothing = nv::common::bit(10),
    Ssif            = nv::common::bit(11),
    Nhp             = nv::common::bit(12),
    BootStatusMask  = (nv::common::bit(13) - 1),
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

constexpr bool EnableCP2112NativeGpio  = false;
constexpr bool EnableI2CErrorInjection = false;

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

/******** ******** Iox Emulation Config Starts ******** ********/
constexpr bool                                          EnableIoxEmulation = false;
constexpr inline size_t                                 IoxNum             = 0;
constexpr inline uint8_t                                IoxI2cBaseAddr     = 0x50;
constexpr inline std::array<nv::iox::IoxConfig, IoxNum> IoxConfigs{};
/******** ******** Iox Emulation Config Ends ******** ********/

/******** ******** Voltage Monitor Config Starts ******** ********/
// clang-format off
namespace voltage_monitor_config {
using namespace nv::volt_mon;
// VOLTAGE_MONITOR_DISABLED  (expanded inline per code review)
constexpr bool EnableDbgInfo = false;
constexpr bool SensorOnAdc0  = false;
constexpr bool SensorOnAdc1  = false;

constexpr size_t         LeakDetectSensorNum                     = 0;
constexpr SensorId       LeakDetectSensorId[LeakDetectSensorNum] = {};
constexpr gpio::GpioPort AlertGpioPort = nv::gpio::InvalidGpioPort;
constexpr gpio::GpioPin  AlertGpioPin  = nv::gpio::InvalidGpioPin;
template<size_t Index>
constexpr nv::volt_mon::LeakDetectSensor leak_detect_get_sensor_config()
{
    static_assert(Index < LeakDetectSensorNum, "Sensor index out of range");
    return {};
}

constexpr size_t   BusBarTempSensorNum                     = 0;
constexpr SensorId BusBarTempSensorDefault                 = 0;
constexpr SensorId BusBarTempSensorId[BusBarTempSensorNum] = {};
template<size_t Index>
constexpr nv::volt_mon::BusBarTempSensor bus_bar_temp_get_sensor_config()
{
    static_assert(Index < BusBarTempSensorNum, "Sensor index out of range");
    return {};
}

constexpr size_t PgoodVoltSensorNum = 0;
template<size_t Index>
constexpr nv::volt_mon::PgoodVoltSensor pgood_volt_get_sensor_config()
{
    static_assert(Index < PgoodVoltSensorNum, "Sensor index out of range");
    return {};
}


constexpr nv::volt_mon::McuInternalTempSensor mcu_internal_temp_get_sensor_config()
{
    return {
        {
            nv::volt_mon::AdcInstance::Invalid,
            nv::volt_mon::AdcChannel::Invalid,
            nv::volt_mon::AdcScanMode::Invalid,
            nv::volt_mon::AdcCommand::None,
            nv::volt_mon::AdcCommand::None,
            nv::volt_mon::AdcCommand::None,
            nv::volt_mon::AdcTriggerSrc::Invalid,
            0,
        },
        nv::volt_mon::Sensor::Invalid,
        0.0f
    };
}
}  // namespace voltage_monitor_config
// clang-format on
/******** ******** Voltage Monitor Config Ends ******** ********/

// SMA_READY pin configuration
constexpr bool               EnableMcuReadyPin = false;
constexpr nv::gpio::GpioPort McuReadyPinPort   = 0;
constexpr nv::gpio::GpioPin  McuReadyPinPin    = 0;

constexpr bool EnableForwardNvlInfo = false;

// UART Bridge configuration (stub values for testrunner)
constexpr nv::vruart::Signal   pintx{.port = 0, .pin = 0};
constexpr nv::vruart::Signal   pinrx{.port = 0, .pin = 0};
constexpr nv::vruart::Baudrate UartOverUsbBaudrate     = 115200U;
constexpr nv::vruart::EdmaChn  UartOverUsbEdmaTxChn    = nv::vruart::EdmaChn::_0;
constexpr nv::vruart::EdmaChn  UartOverUsbEdmaRxChn    = nv::vruart::EdmaChn::_1;
constexpr nv::vruart::EdmaInst UartOverUsbEdmaInstance = nv::vruart::EdmaInst::_0;
constexpr nv::vruart::Instance UartOverUsbUartInstance = nv::vruart::Instance::_0;

// EEPROM bridge configuration (disabled)
constexpr bool            EnableEepromBridge = false;
constexpr uint8_t         EepromDstAddress   = 0x50;
constexpr nv::ipc::TaskId EepromTaskId       = nv::ipc::TaskId::End;
constexpr nv::i2c::Port   EepromDstPort      = nv::i2c::Port::Two;

// Slave function lookup table: maps (port, address) -> SlaveFunction
// Format: {port, address, function, enabled}
constexpr uint8_t SlaveFunctionTableSize = 0;
constexpr std::array<nv::i2c::SlaveFunctionEntry, SlaveFunctionTableSize> SlaveFunctionTable = {
    {}};

// EEPROM configuration (disabled)
constexpr uint16_t EepromSize          = 0;
constexpr QueueId  EepromI2cQueueId    = QueueId::End;
constexpr uint32_t EepromUpdateTimerUs = 0;

}  // namespace nv::ipc

namespace nv::i2c {

// Error Injection configuration: explicit count for I2C and IOX
constexpr size_t NV_I2C_ERROR_INJECTION_PORTS = 0;  // Number of I2C handlers configured for
                                                    // error injection
constexpr size_t NV_IOX_ERROR_INJECTION_PORTS = 0;  // Number of IOX devices configured for
                                                    // error injection
constexpr size_t NV_I2C_MAX_ERROR_INJECTION_PORTS = NV_I2C_ERROR_INJECTION_PORTS
                                                  + NV_IOX_ERROR_INJECTION_PORTS;

// Error Injection ipchandler-to-Port mapping - Simple table
struct ErrorInjectionPortMapping
{
    nv::ipchandler::Id ipchandler_id;
    Port               port;
};

// Empty mapping table since error injection is disabled (NV_I2C_MAX_ERROR_INJECTION_PORTS = 0)
// If you want to enable error injection, set NV_I2C_ERROR_INJECTION_PORTS and
// NV_IOX_ERROR_INJECTION_PORTS > 0 and populate this table
constexpr inline std::array<ErrorInjectionPortMapping, NV_I2C_MAX_ERROR_INJECTION_PORTS>
    ErrorInjectionPortMappingTable{};

/******** ******** SMBus Direct Configuration ******** ********/
// SMBus Direct not configured for this project (disabled)
constexpr nv::i2c::Port SmbusDirectPort     = nv::i2c::Port::End;  // Disabled (Port::End)
constexpr uint32_t      SmbusCacheRefreshMs = 0;  // Cache refresh period in microseconds (0 to
                                                  // disable)

static_assert(!(SmbusCacheRefreshMs > 0 && nv::ipc::SensorUpdateMs > 0),
              "Only One Timer for SMbus Direct can be enabled for I2c0 task!!!");

}  // namespace nv::i2c

namespace nv::mctp {

/** function to add/remove NSM Message Types
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
**/
constexpr void
config_nsm_types([[maybe_unused]] std::array<uint8_t, nsm_msg::NvMctpSupportedNum>& bitmask)
{
    // Default does nothing
}

/** function to add/remove NSM T0 Command Codes
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
**/
constexpr void
config_nsm_type0_cmd([[maybe_unused]] std::array<uint8_t, nsm_msg::NvMctpSupportedNum>& bitmask)
{
    // Default does nothing
}

/** function to add/remove NSM T2 Command Codes
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
**/
constexpr void
config_nsm_type2_cmd([[maybe_unused]] std::array<uint8_t, nsm_msg::NvMctpSupportedNum>& bitmask)
{
    // Default does nothing
}

/** function to add/remove NSM T3 Command Codes
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
**/
constexpr void
config_nsm_type3_cmd([[maybe_unused]] std::array<uint8_t, nsm_msg::NvMctpSupportedNum>& bitmask)
{
    // Default does nothing
}

/** function to add/remove NSM T4 Command Codes
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
**/
constexpr void
config_nsm_type4_cmd([[maybe_unused]] std::array<uint8_t, nsm_msg::NvMctpSupportedNum>& bitmask)
{
    // Default does nothing
}

/** function to add/remove NSM T5 Command Codes
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
**/
constexpr void
config_nsm_type5_cmd([[maybe_unused]] std::array<uint8_t, nsm_msg::NvMctpSupportedNum>& bitmask)
{
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevCfgCmdCode::GetSmaBaseboardSettings));
}

/** function to add/remove NSM T6 Command Codes
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
**/
constexpr void
config_nsm_type6_cmd([[maybe_unused]] std::array<uint8_t, nsm_msg::NvMctpSupportedNum>& bitmask)
{
    // Default does nothing
}

/** function to add/remove NSM TFF Command Codes
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
**/
constexpr void config_nsm_typeff_cmd(
    [[maybe_unused]] std::array<uint8_t, nsm_msg::NvMctpSupportedNum>& bitmask)
{
    // Default does nothing
}

/** function to add/remove NSM T0 Events
    Use nsm_msg::set_bit() and nsm_msg::unset_bit() defined in nsm_msg_bitmask.h
*/
constexpr std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> gen_type0_event_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> bitmask = {0};

    return bitmask;
}

constexpr auto mcuTemperatureSensorsSize = 8;
/** List of active MCU sensors for NSM type 3 Temperature Reading */
constexpr inline std::array<Type3TemperatureSensors, mcuTemperatureSensorsSize>
    mcuTemperatureSensors{TempGpu1,
                          TempGpu2,
                          TempTMP451_1,
                          TempTMP451_2,
                          TempMaxModule,
                          TempSMAInternal,
                          BusBar_Temp,
                          SMA_Internal};

constexpr auto mcuPowerSensorsSize = 3;
/** List of active MCU sensors for NSM type 3 Power Draw */
constexpr inline std::array<Type3PowerSensors, mcuPowerSensorsSize> mcuPowerSensors{
    PowerGpu1, PowerGpu2, PowerModule};

constexpr auto mcuVoltageSensorsSize = 0;
/** List of active MCU sensors for NSM type 3 Voltage */
constexpr inline std::array<T3Voltage, mcuVoltageSensorsSize> mcuVoltageSensors{};

// Type 4 Diagnostics Telemetries
constexpr auto T4TelemetriesSize = 12;
using T3TagId                    = uint8_t;
constexpr T3TagId NoT3Tag        = 0xff;
using McuDiagnosticTelemetry     = std::
    tuple<Type4McuDiagnosticEntries, Type4TelemetryTypes, T3TagId>;
constexpr inline std::array<McuDiagnosticTelemetry, T4TelemetriesSize> mcuDiagnosticTelemetries{
    McuDiagnosticTelemetry{DIAG_GPIO_VALUE_BITMAP,        GpioTelemetry,         NoT3Tag},
    McuDiagnosticTelemetry{        DIAG_GPU1_TEMP, TemperatureTelemetry,        TempGpu1},
    McuDiagnosticTelemetry{        DIAG_GPU2_TEMP, TemperatureTelemetry,        TempGpu2},
    McuDiagnosticTelemetry{     DIAG_MODULE_TEMP1, TemperatureTelemetry,    TempTMP451_1},
    McuDiagnosticTelemetry{     DIAG_MODULE_TEMP2, TemperatureTelemetry,    TempTMP451_2},
    McuDiagnosticTelemetry{    DIAG_INTERNAL_TEMP, TemperatureTelemetry, TempSMAInternal},
    McuDiagnosticTelemetry{  DIAG_VR_SMA_Internal, TemperatureTelemetry,    SMA_Internal},
    McuDiagnosticTelemetry{  DIAG_MAX_MODULE_TEMP, TemperatureTelemetry,   TempMaxModule},
    McuDiagnosticTelemetry{       DIAG_GPU1_POWER,       PowerTelemetry,       PowerGpu1},
    McuDiagnosticTelemetry{       DIAG_GPU2_POWER,       PowerTelemetry,       PowerGpu2},
    McuDiagnosticTelemetry{     DIAG_MODULE_POWER,       PowerTelemetry,     PowerModule},
    McuDiagnosticTelemetry{   DIAG_VR_BUSBAR_TEMP, TemperatureTelemetry,     BusBar_Temp},
};

// Temperature Sensors Config for VR products
constexpr uint8_t I2cTempSensorSize = 8;

// This data is only shared between main.cpp and mctp task. Should not impact when migrating to
// dual core.
NV_SHARED_DATA inline std::array<nv::i2c::I2cTempSensorThresholdsConfig, I2cTempSensorSize>
    I2cTempSensorList{
        nv::i2c::I2cTempSensorThresholdsConfig{nv::i2c::Port::Four,
                                               0, 0,
                                               0x70, 0x73,
                                               0x66, 0x69,
                                               nv::mctp::Type3TemperatureSensors::CPU1_Die,
                                               {0x48, 0x1c}},
        nv::i2c::I2cTempSensorThresholdsConfig{nv::i2c::Port::Four,
                                               0, 0,
                                               0x70, 0x73,
                                               0x66, 0x69,
                                               nv::mctp::Type3TemperatureSensors::CPU1_SoC,
                                               {0x4d, 0x3c}},
        nv::i2c::I2cTempSensorThresholdsConfig{nv::i2c::Port::Four,
                                               0, 0,
                                               0x70, 0x73,
                                               0x66, 0x69,
                                               nv::mctp::Type3TemperatureSensors::SMA_External,
                                               {0x50, 0x49}},
        nv::i2c::I2cTempSensorThresholdsConfig{nv::i2c::Port::Four,
                                               0, 0,
                                               0x7f, 0x7f,
                                               0x7f, 0x7f,
                                               nv::mctp::Type3TemperatureSensors::GPU1_Die_A,
                                               {0x4c, 0x4c}},
        nv::i2c::I2cTempSensorThresholdsConfig{nv::i2c::Port::Four,
                                               0, 0,
                                               0x7f, 0x7f,
                                               0x7f, 0x7f,
                                               nv::mctp::Type3TemperatureSensors::GPU1_Die_B,
                                               {0x4a, 0x7c}},
        nv::i2c::I2cTempSensorThresholdsConfig{nv::i2c::Port::Four,
                                               0, 0,
                                               0x7f, 0x7f,
                                               0x7f, 0x7f,
                                               nv::mctp::Type3TemperatureSensors::GPU2_Die_A,
                                               {0x4b, 0x5c}},
        nv::i2c::I2cTempSensorThresholdsConfig{nv::i2c::Port::Four,
                                               0, 0,
                                               0x7f, 0x7f,
                                               0x7f, 0x7f,
                                               nv::mctp::Type3TemperatureSensors::GPU2_Die_B,
                                               {0x49, 0x6c}},
        nv::i2c::I2cTempSensorThresholdsConfig{nv::i2c::Port::Four,
                                               0, 0,
                                               0x7f, 0x7f,
                                               0x7f, 0x7f,
                                               nv::mctp::Type3TemperatureSensors::NvLink_Temp,
                                               {0x46, 0x4e}}
};
// Compile-time check to ensure the array is properly sized
static_assert(sizeof(I2cTempSensorList)
                  == nv::mctp::I2cTempSensorSize
                         * sizeof(nv::i2c::I2cTempSensorThresholdsConfig),
              "Array elements should be tightly packed without padding");

//********************* Write Protection Configuration *********************/
constexpr uint8_t WP_VALID_PORT       = 0;
constexpr uint8_t WP_VALID_PIN        = 0;
constexpr auto    WriteProtectionSize = 4;
constexpr inline std::array<nv::mctp::WriteProtectionGpioConfig, WriteProtectionSize>
    WriteProtectionList{
        nv::mctp::WriteProtectionGpioConfig{
                                            Type4WriteProtectionFunction::WP_BASEBOARD_FRU_EEPROM, WP_VALID_PORT, WP_VALID_PIN},
        nv::mctp::WriteProtectionGpioConfig{
                                            Type4WriteProtectionFunction::WP_NVSW_QM4_SPI, WP_VALID_PORT, WP_VALID_PIN        },
        nv::mctp::WriteProtectionGpioConfig{
                                            Type4WriteProtectionFunction::WP_GPU_SPI, WP_VALID_PORT, WP_VALID_PIN             },
        nv::mctp::WriteProtectionGpioConfig{
                                            Type4WriteProtectionFunction::WP_CX9_SPI, WP_VALID_PORT, WP_VALID_PIN             }
};

static_assert(WriteProtectionList.size() == WriteProtectionSize,
              "WriteProtectionList size mismatch");

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

namespace nv::soc_pwr_smoothing {

#define NV_SOC_PWR_SMOOTHING_ADC_IRQ_HANDLER ADC0_IRQHandler
constexpr bool     SocAdcHiResMode             = true;
constexpr uint32_t SocAdcPeripheral            = 0;  // Index for array-based accesse
constexpr uint32_t SocAdcFifoNum               = 0;
constexpr uint32_t SocAdcInitialTriggerCommand = 1;
constexpr uint32_t EdppDacPeripheral           = 0;
constexpr uint32_t IsinkDacPeripheral          = 1;
constexpr uint32_t PwrBrakeGpioPort            = nv::ipc::MCU_PWR_BRAKE_L_PORT;
constexpr uint32_t PwrBrakeGpioPin             = nv::ipc::MCU_PWR_BRAKE_L_PIN;
constexpr uint32_t McuThermWarnPort            = nv::ipc::MCU_THERM_WARN_L_PORT;
constexpr uint32_t McuThermWarnPin             = nv::ipc::MCU_THERM_WARN_L_PIN;
constexpr float    OvrmMaxDacOutputV           = 2.2;

// SoC voltage measurement configuration
constexpr float SocAdcRefVoltageV = 3.3;  // ADC reference voltage (V)
// Voltage divider resistors: V_rack --[R1]-- V_adc --[R2]-- GND
constexpr uint32_t SocVoltageDividerR1 = 10000;  // Upper resistor (Ω)
constexpr uint32_t SocVoltageDividerR2 = 3320;   // Lower resistor (Ω)
constexpr float    SocVoltageMin       = 1.0;    // Min rack voltage for 0% SoC (V)
constexpr float    SocVoltageMax       = 7.0;    // Max rack voltage for 100% SoC (V)

// ADC Calibration Test Mode - External DAC I2C Configuration
constexpr nv::i2c::Port AdcCalibDacI2cPort = nv::i2c::Port::Three;
constexpr uint8_t       AdcCalibDacI2cAddr = 0x4C;  // AD5693 7-bit address

}  // namespace nv::soc_pwr_smoothing

#endif
