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
#include <functional>
#include <optional>

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

// CPLD configuration (not supported in this project)
constexpr nv::i2c::Port CPLD_I2C_PORT = nv::i2c::Port::End;

namespace nv::telemetry {

/** A mapping of telemetry id to index in the cache */
constexpr uint8_t TelemIndexMapSize = 13;
using TelemIndexMap                 = std::tuple<nv::telemetry::TelemId, int>;
constexpr inline std::array<TelemIndexMap, TelemIndexMapSize> TelemIndexMapList{
    TelemIndexMap{     nv::telemetry::TelemId::Gpu1Temp, -1},
    TelemIndexMap{     nv::telemetry::TelemId::Gpu2Temp, -1},
    TelemIndexMap{    nv::telemetry::TelemId::Gpu1Power, -1},
    TelemIndexMap{    nv::telemetry::TelemId::Gpu2Power, -1},
    TelemIndexMap{  nv::telemetry::TelemId::ModulePower, -1},
    TelemIndexMap{  nv::telemetry::TelemId::ModuleTemp1, -1},
    TelemIndexMap{  nv::telemetry::TelemId::ModuleTemp2, -1},
    TelemIndexMap{ nv::telemetry::TelemId::InternalTemp, -1},
    TelemIndexMap{nv::telemetry::TelemId::MaxModuleTemp, -1},
    TelemIndexMap{         nv::telemetry::TelemId::Gpio, -1},
    TelemIndexMap{   nv::telemetry::TelemId::CX8_1_Temp, -1},
    TelemIndexMap{   nv::telemetry::TelemId::CX8_2_Temp, -1},
};

}  // namespace nv::telemetry

namespace nv::ipc {
// Indicate if it is dual core project
constexpr bool EnableDualCore = true;

/// All Tasks must be part of this enum.
enum class TaskId
{
    Begin,
    Mctp = Begin,
    I2c0,
    I2c1,
    I2c2,
#ifdef NV_UNITTEST
    Unittest,
#endif
    Pldm,
    Logger,
    Spdm,
    Ipc0,
    Ipc1,
    Privileged = Ipc1 + 1,
    Usb        = Privileged + 0,
    Flash      = Privileged + 1,
    I3c0       = Privileged + 2,
    EndPrivileged,
    End   = EndPrivileged,
    Timer = End,
    Idle,
    KernelEnd,
    Invalid = KernelEnd
};

using TaskInfo = std::tuple<TaskId, CoreId>;

constexpr inline std::array<TaskInfo, int(TaskId::KernelEnd) + 1> TaskInfos{
    TaskInfo{    TaskId::Mctp,    CoreId::Core0},
    TaskInfo{    TaskId::I2c0,    CoreId::Core0},
    TaskInfo{    TaskId::I2c1,    CoreId::Core0},
    TaskInfo{    TaskId::I2c2,    CoreId::Core0},
#ifdef NV_UNITTEST
    TaskInfo{TaskId::Unittest,    CoreId::Core0},
#endif
    TaskInfo{    TaskId::Pldm,    CoreId::Core0},
    TaskInfo{  TaskId::Logger,    CoreId::Core0},
    TaskInfo{    TaskId::Spdm,    CoreId::Core0},
    TaskInfo{    TaskId::Ipc0,    CoreId::Core0},
    TaskInfo{    TaskId::Ipc1,    CoreId::Core1},
    TaskInfo{     TaskId::Usb,    CoreId::Core0},
    TaskInfo{   TaskId::Flash,    CoreId::Core0},
    TaskInfo{    TaskId::I3c0,    CoreId::Core0},
    TaskInfo{   TaskId::Timer, CoreId::Abstract},
    TaskInfo{    TaskId::Idle, CoreId::Abstract},
    TaskInfo{ TaskId::Invalid,  CoreId::Invalid}
};

constexpr CoreId get_core_from_task(TaskId task_id)
{
    auto idx = static_cast<std::size_t>(task_id);
    if (task_id == TaskId::Invalid || idx >= TaskInfos.size()) {
        return CoreId::Invalid;
    }
    return std::get<1>(TaskInfos[idx]);
}

using HardwareInfo = std::tuple<HardwareId, CoreId>;

constexpr inline std::array<HardwareInfo, int(HardwareId::End)> HardwareInfos{
    HardwareInfo{       HardwareId::USB,     get_core_from_task(TaskId::Usb)},
    HardwareInfo{     HardwareId::I3C_0, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{     HardwareId::I3C_1,    get_core_from_task(TaskId::I3c0)},
    HardwareInfo{HardwareId::eDMA_0_CH0, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{HardwareId::eDMA_0_CH1, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{HardwareId::eDMA_1_CH0,    get_core_from_task(TaskId::I3c0)},
    HardwareInfo{HardwareId::eDMA_1_CH1,    get_core_from_task(TaskId::I3c0)},
    HardwareInfo{ HardwareId::FLEXCOMM0,    get_core_from_task(TaskId::I2c1)},
    HardwareInfo{ HardwareId::FLEXCOMM1,    get_core_from_task(TaskId::I2c2)},
    HardwareInfo{ HardwareId::FLEXCOMM2, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{ HardwareId::FLEXCOMM3,    get_core_from_task(TaskId::I2c0)},
    HardwareInfo{ HardwareId::FLEXCOMM4, get_core_from_task(TaskId::Invalid)},
    // TODO: Add other hardware assignments based on system requirements
};

constexpr CoreId get_core_from_hardware(HardwareId hardware_id)
{
    auto idx = static_cast<std::size_t>(hardware_id);
    if (hardware_id == HardwareId::End || idx >= HardwareInfos.size()) {
        return CoreId::Invalid;
    }
    return std::get<1>(HardwareInfos[idx]);
}

/// All Queues must be part of this enum.
enum class QueueId
{
    Begin,
    MctpDataRequest = Begin,
    MctpPldmRequest,
    MctpSpdmRequest,
    MctpCmd,
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
    UsbToSpi,
    SpiToUsb,
    End
};

/// All Events must be part of this enum.
enum class EventId
{
    Begin,
    I2c0,
    I2c1,
    I2c2,
    I3c0,
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
    Spi0EdmaDriverEvent,
    End
};

using EventInfo = std::tuple<EventId, CoreId>;

constexpr inline std::array<EventInfo, int(EventId::End)> EventInfos{
    EventInfo{              EventId::Begin,                     CoreId::Invalid},
    EventInfo{               EventId::I2c0,    get_core_from_task(TaskId::I2c0)},
    EventInfo{               EventId::I2c1,    get_core_from_task(TaskId::I2c1)},
    EventInfo{               EventId::I2c2,    get_core_from_task(TaskId::I2c2)},
    EventInfo{               EventId::I3c0,    get_core_from_task(TaskId::I3c0)},
    EventInfo{           EventId::PldmTask,    get_core_from_task(TaskId::Pldm)},
    EventInfo{            EventId::UsbTask,     get_core_from_task(TaskId::Usb)},
    EventInfo{         EventId::FlashEvent,   get_core_from_task(TaskId::Flash)},
    EventInfo{          EventId::Spi0Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{          EventId::Spi1Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{          EventId::Spi2Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{           EventId::LogEvent,  get_core_from_task(TaskId::Logger)},
    EventInfo{           EventId::SpdmTask,                        CoreId::Both},
    EventInfo{     EventId::TaskBootStatus,                       CoreId::Core0},
    EventInfo{    EventId::TaskAliveStatus,                        CoreId::Both},
    EventInfo{EventId::Spi0EdmaDriverEvent, get_core_from_task(TaskId::Invalid)},
};

using ClientInfo = std::tuple<mctp::Client, TaskId>;

constexpr inline std::array<ClientInfo, int(mctp::Client::End)> ClientInfos{
    ClientInfo{ mctp::Client::UsI2c,    TaskId::I2c0},
    ClientInfo{ mctp::Client::UsUsb,     TaskId::Usb},
    ClientInfo{mctp::Client::DsI2c0,    TaskId::I2c1},
    ClientInfo{mctp::Client::DsI2c1,    TaskId::I2c2},
    ClientInfo{mctp::Client::DsI2c2, TaskId::Invalid},
    ClientInfo{mctp::Client::DsI2c3, TaskId::Invalid},
    ClientInfo{mctp::Client::DsI3c0,    TaskId::I3c0},
    ClientInfo{mctp::Client::DsI3c1, TaskId::Invalid},
    ClientInfo{  mctp::Client::Pldm,    TaskId::Pldm},
    ClientInfo{  mctp::Client::Spdm,    TaskId::Spdm},
    ClientInfo{mctp::Client::Spdm4K,    TaskId::Spdm},
    ClientInfo{  mctp::Client::Spi0, TaskId::Invalid},
    ClientInfo{  mctp::Client::Spi1, TaskId::Invalid},
    ClientInfo{  mctp::Client::Spi2, TaskId::Invalid},
};

/// All Stream Buffers must be part of this enum.
enum class StreamBufferId
{
    Begin,
    Core0ToCore1 = Begin,
    Core1ToCore0,
    C2CEnd,
    End = C2CEnd,
};

using StreamBufferInfo = std::tuple<StreamBufferId, uint32_t>;

// 4 is align 1 byte to 4 bytes, the reason of adding 1 byte is streambuffer has 1 byte cannot
// be read or write to maintain the ring buffer
// 8060 is the size of the pldm update 4k message
// Calculation: ( (4 + 16) + (4 + 4) + (4 + 16) + (4 + 72) ) * 65 = 8060
// 0x800 is the extra space for streambuffer : 2K bytes
constexpr uint32_t StreamBufferRingOverhead = 4;
constexpr uint32_t C2CUsbPldmUpdate4KSize   = 8060;
constexpr uint32_t C2CBufferExtraSpace      = 0x800;
constexpr uint32_t C2CBufferSize            = StreamBufferRingOverhead + C2CUsbPldmUpdate4KSize
                                 + C2CBufferExtraSpace;
constexpr inline std::array<StreamBufferInfo, int(StreamBufferId::End)> StreamBufferInfos{
    StreamBufferInfo{StreamBufferId::Core0ToCore1, C2CBufferSize},
    StreamBufferInfo{StreamBufferId::Core1ToCore0, C2CBufferSize},
};

// Down stream Information
constexpr uint8_t I2cUpStreamNum          = 1;
constexpr uint8_t I2cDownStreamNum        = 2;
constexpr uint8_t I3cDownStreamNum        = 0;
constexpr uint8_t DownStreamNum           = I2cDownStreamNum + I3cDownStreamNum;
constexpr uint8_t UpStreamNum             = 2;
constexpr uint8_t DefaultRoutingTableSize = DownStreamNum + UpStreamNum;
constexpr uint8_t RoutingInfoUpdateSize   = 0;
constexpr uint8_t RoutingTableSize        = DefaultRoutingTableSize + RoutingInfoUpdateSize;

constexpr inline std::array<mctp::DownStreamInfo, DownStreamNum> DownStreamInfos{
    // Use MCU pg540 as downstream which address is 0x40
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c0,
                         0x40},
    // Use MCU pg540 as downstream which address is 0x40
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c1,
                         0x40},
};
constexpr uint32_t SpdmRequestQueueSize      = 2048;
constexpr uint32_t SpdmRxQueueSize           = 72;
constexpr uint32_t SpdmCryptoHelperQueueSize = 12;
constexpr bool     SpdmI2cResponder          = true;
constexpr bool     SpdmDummyCertificates     = true;
constexpr uint32_t SpdmCryptoHelperMaxItems  = EnableDualCore ? 2 : 1;
using QueueInfo                              = std::tuple<QueueId, uint8_t, uint16_t, CoreId>;
// USB to SPI bridge (enable to support flashrom -p NV_SMA_SPI)
constexpr bool     EnableFlashrom = false;
constexpr uint32_t UsbSpiMsgSize  = EnableFlashrom ? 512 : 1;

/// define all queue lengths and item_sizes here
constexpr inline std::array<QueueInfo, int(QueueId::End)> QueueInfos{
    // id, len, item_size
    QueueInfo{    QueueId::MctpDataRequest,130,                                                 72,get_core_from_task(TaskId::Mctp)                                                                                                    },
    {    QueueId::MctpPldmRequest,   1,                                                256,    get_core_from_task(TaskId::Mctp)},
    {    QueueId::MctpSpdmRequest,   1,                               SpdmRequestQueueSize,    get_core_from_task(TaskId::Mctp)},
    {            QueueId::MctpCmd, 130,                                                  4,    get_core_from_task(TaskId::Mctp)},
    {               QueueId::I2c0,  64,                                                 80,    get_core_from_task(TaskId::I2c0)},
    {               QueueId::I2c1,  64,                                                 80,    get_core_from_task(TaskId::I2c1)},
    {               QueueId::I2c2,  64,                                                 80,    get_core_from_task(TaskId::I2c2)},
    {               QueueId::I2c3,  64,                                                 80, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I2c4,  64,                                                 80, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I2c5,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I3c0,   1,                                                 72,    get_core_from_task(TaskId::I3c0)},
    {               QueueId::I3c1,  64,                                                 72, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi0,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi1,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi2,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {             QueueId::PldmRx,   1,                                                 72,    get_core_from_task(TaskId::Pldm)},
    {           QueueId::PldmRx4k,   1,                                          4096 + 32,    get_core_from_task(TaskId::Pldm)},
    {              QueueId::UsbTx,   1,                                                 72,     get_core_from_task(TaskId::Usb)},
    {       QueueId::FlashRequest,   2,                                                280,   get_core_from_task(TaskId::Flash)},
    {      QueueId::FlashResponse,   1,                                                272,                        CoreId::Both},
    {       QueueId::RoutingTable,
              RoutingTableSize, sizeof(mctp::ShardRoutingTable) * RoutingTableSize,
              CoreId::Both                                                                                                              },
    {         QueueId::LogRequest,  64,                                                 22,  get_core_from_task(TaskId::Logger)},
    {QueueId::LogResponseBlocking,   1,                                                 22,  get_core_from_task(TaskId::Logger)},
    {        QueueId::LogDownload,   1,                                                 22,  get_core_from_task(TaskId::Logger)},
    // LogDownloadResp is related to Mctp since queue recv only use in
    // nv::logger::Task::download, which is only used in VDM command (But it doesn't block other
    // task from using nv::logger::Task::download)
    {    QueueId::LogDownloadResp,   1,                                                 22,    get_core_from_task(TaskId::Mctp)},
    {             QueueId::LogISR,   6,                                                 22,  get_core_from_task(TaskId::Logger)},
    {          QueueId::FlashSema,   1,                                                  1,                        CoreId::Both},
    {             QueueId::SpdmRx,   1,                                    SpdmRxQueueSize,    get_core_from_task(TaskId::Spdm)},
    {   QueueId::SpdmCryptoHelper,
              SpdmCryptoHelperMaxItems,                          SpdmCryptoHelperQueueSize,
              get_core_from_task(TaskId::Spdm)                                                                                          },
    {             QueueId::UsbHid,   1,                                                 68,     get_core_from_task(TaskId::Usb)},
    {           QueueId::UsbToSpi,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {           QueueId::SpiToUsb,   1,                                                  1, get_core_from_task(TaskId::Invalid)}
};

/// All Timers must be part of this enum
enum class TimerId
{
    Begin,
    MctpEnumerate = Begin,
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
    End
};

using TimerInfo = std::tuple<TimerId, CoreId>;

constexpr inline std::array<TimerInfo, int(TimerId::End)> TimerInfos{
    TimerInfo{        TimerId::MctpEnumerate,    get_core_from_task(TaskId::Mctp)},
    TimerInfo{                 TimerId::Pldm,    get_core_from_task(TaskId::Pldm)},
    TimerInfo{          TimerId::PerfMonitor,                       CoreId::Core0},
    TimerInfo{TimerId::MctpApPowerGoodEngage,    get_core_from_task(TaskId::Mctp)},
    TimerInfo{           TimerId::Bootloader,                       CoreId::Core0},
    TimerInfo{      TimerId::RuntimeWatchDog,                       CoreId::Core0},
    TimerInfo{           TimerId::Gpu1Seneor, get_core_from_task(TaskId::Invalid)},
    TimerInfo{           TimerId::Gpu2Seneor, get_core_from_task(TaskId::Invalid)},
    TimerInfo{            TimerId::SmbSensor, get_core_from_task(TaskId::Invalid)},
    TimerInfo{            TimerId::Ap1Status, get_core_from_task(TaskId::Invalid)},
    TimerInfo{            TimerId::Ap2Status, get_core_from_task(TaskId::Invalid)}
};

// Limits
[[maybe_unused]] constexpr auto MaxQueuesPerTask = 4;
[[maybe_unused]] constexpr auto MaxEventsPerTask = 4;

constexpr uint8_t GpioNum = 1;
enum Port : gpio::GpioPort
{
    GlobalWpPort = nv::gpio::InvalidGpioPort,
    ApGoodPort   = nv::gpio::InvalidGpioPort,

    // Port 4
    THERM_OVERT_N_PORT = 4
};

enum Pin : gpio::GpioPin
{
    GlobalWpPin = nv::gpio::InvalidGpioPin,
    ApGoodPin   = nv::gpio::InvalidGpioPin,

    // Port 4 pins
    THERM_OVERT_N_PIN = 15
};

using Gpios = std::tuple<gpio::GpioPort, gpio::GpioPin>;

constexpr inline std::array<Gpios, GpioNum> GpioSetup{
    // Port 4
    Gpios{THERM_OVERT_N_PORT, THERM_OVERT_N_PIN}, // IN
};

// TODO: confirming Rising/Falling edge trigger for each interrupt
using GpioInterruptConfig = std::tuple<nv::gpio::GpioPort,
                                       nv::gpio::GpioPin,
                                       nv::gpio::InterruptDetection,
                                       nv::gpio::InterruptSelect>;

constexpr int GpioInterruptNum = 4;

constexpr inline std::array<GpioInterruptConfig, GpioInterruptNum> GpioInterruptSetup{
    GpioInterruptConfig{4,
                        12,nv::gpio::InterruptDetection::InterruptRising,
                        nv::gpio::InterruptSelect::InterruptSelect0}, //  GA_GPIO9_IROT_ERROR_N
    GpioInterruptConfig{
                        4,                      13,
                        nv::gpio::InterruptDetection::InterruptRising,
                        nv::gpio::InterruptSelect::InterruptSelect0}, //  GA_GPIO10_IROT_AP_BOOT_COMPLETE
    GpioInterruptConfig{4,
                        15, nv::gpio::InterruptDetection::InterruptRising,
                        nv::gpio::InterruptSelect::InterruptSelect0}, //  THERM_OVERT
    GpioInterruptConfig{4,
                        20, nv::gpio::InterruptDetection::InterruptRising,
                        nv::gpio::InterruptSelect::InterruptSelect0},
};  // PS_BOARD_PGOOD

constexpr uint32_t CtimerFrequency = 48000000;

#if defined(CPU_MCXN547VDF_cm33_core0) || defined(CPU_MCXN556SCDF_cm33_core0)
constexpr auto UartInstance = 2;
#else
// No UART on core1
constexpr auto UartInstance = 0xfe;
#endif

enum BootedEventBits : uint32_t
{
    Mctp  = nv::common::bit(0),
    I2c0  = nv::common::bit(1),
    I2c1  = nv::common::bit(2),
    I2c2  = nv::common::bit(3),
    I3c0  = nv::common::bit(4),
    Usb   = nv::common::bit(5),
    Flash = nv::common::bit(6),
#ifdef NV_UNITTEST
    Unittest,
#endif
    Pldm           = nv::common::bit(7),
    Logger         = nv::common::bit(8),
    Spdm           = nv::common::bit(9),
    BootStatusMask = (nv::common::bit(10) - 1),
};

constexpr uint32_t WatchdogResetMs       = 2000;
constexpr uint32_t CheckTaskBootStatusMs = 1000;

// USB config
constexpr uint16_t UsbDeviceVid = 0x0955U;
constexpr uint16_t UsbDevicePid = 0xCF11U;

// Runtime WDT
constexpr uint32_t RuntimeWatchdogResetMs = 1000;
constexpr uint32_t SupervisorCheckMs      = 500;
constexpr uint32_t SupervisorCheckUs      = SupervisorCheckMs * 1000;

constexpr std::array<nv::watchdog::TaskMonitorIndex, 2> TaskMonitorList{
    nv::watchdog::TaskMonitorIndex::Flash, nv::watchdog::TaskMonitorIndex::Logger};

constexpr bool EnableRuntimeWdt       = false;
constexpr bool EnableCP2112NativeGpio = false;

// Debugtoken config
constexpr bool DebugTokenEnabled = true;

// I2C config
constexpr bool EnableSmbDirect = true;
// I2C scan VDM support
constexpr bool EnableI2cScanVdm = false;
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
constexpr uint32_t SensorUpdateMs      = 50 * 1000;
constexpr uint8_t  InternalTempWarnBit = 3;
constexpr uint8_t  I2cSensorAlertBit   = 4;
constexpr uint8_t  GpioTelemetrySize   = 1;
using GpioTelemetry                    = std::tuple<Port, Pin, uint8_t>;
constexpr inline std::array<GpioTelemetry, GpioTelemetrySize> GpioTelemetryTable{
    GpioTelemetry{THERM_OVERT_N_PORT, THERM_OVERT_N_PIN, 2}
};
constexpr uint8_t ModuleTempSensorSize = 0;
using ModuleTempSensor = std::tuple<nv::i2c::Port, uint8_t, nv::telemetry::TelemId>;
constexpr inline std::array<ModuleTempSensor, ModuleTempSensorSize> ModuleTempSensorList{};

// FPGA WAR: Bug ?
constexpr bool I2cIsEndpoint = false;

// For CX8 I3C init flow, CX8 need time to handle RSTDAA
constexpr bool EnableDelayInI3CInit = false;
// AP status ping interval for discovery notify when no GPIO is available
constexpr uint32_t CheckApStatusTimerUs = 0;
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
constexpr bool Enable_Nsm_type4 = false;

/******** ******** Iox Emulation Config Starts ******** ********/
constexpr bool                                          EnableIoxEmulation = false;
constexpr inline size_t                                 IoxNum             = 0;
constexpr inline uint8_t                                IoxI2cBaseAddr     = 0x50;
constexpr inline std::array<nv::iox::IoxConfig, IoxNum> IoxConfigs{};
/******** ******** Iox Emulation Config Ends ******** ********/

// SMA_READY pin configuration
constexpr bool               EnableMcuReadyPin = false;
constexpr nv::gpio::GpioPort McuReadyPinPort   = 0;
constexpr nv::gpio::GpioPin  McuReadyPinPin    = 0;

constexpr bool EnableForwardNvlInfo = false;
}  // namespace nv::ipc

namespace nv::mctp {

constexpr auto mcuTemperatureSensorsSize = 1;
/** List of active MCU sensors for NSM type 3 Temperature Reading */
constexpr inline std::array<Type3TemperatureSensors, mcuTemperatureSensorsSize>
    mcuTemperatureSensors{TempSMAInternal};

constexpr auto mcuPowerSensorsSize = 0;
/** List of active MCU sensors for NSM type 3 Power Draw */
constexpr inline std::array<Type3PowerSensors, mcuPowerSensorsSize> mcuPowerSensors{};

// Type 4 Diagnostics Telemetries
constexpr auto T4TelemetriesSize = 2;
using T3TagId                    = uint8_t;
constexpr T3TagId NoT3Tag        = 0xff;
using McuDiagnosticTelemetry     = std::
    tuple<Type4McuDiagnosticEntries, Type4TelemetryTypes, T3TagId>;
constexpr inline std::array<McuDiagnosticTelemetry, T4TelemetriesSize> mcuDiagnosticTelemetries{
    McuDiagnosticTelemetry{DIAG_GPIO_VALUE_BITMAP,             GpioTelemetry,         NoT3Tag},
    McuDiagnosticTelemetry{    DIAG_INTERNAL_TEMP, CacheTemperatureTelemetry, TempSMAInternal},
};

// Empty temperature sensor tables for compatibility
// Temperature Sensors Config for VR products
constexpr uint8_t I2cTempSensorSize = 0;

// This data is only shared between main.cpp and mctp task. Should not impact when migrating to
// dual core.
NV_SHARED_DATA inline std::array<nv::i2c::I2cTempSensorConfig, I2cTempSensorSize>
    I2cTempSensorList{};

}  // namespace nv::mctp

namespace nv::pldm {

// Example:
// // AP component ID Information
// constexpr uint8_t                     ApNum            = 2;
// constexpr std::array<uint16_t, ApNum> AllApComponentId = {
//     {0xff00, 0xc000}
// };
// constexpr inline std::array<FwInfo, ApNum> FwInfoList{
//     //                           comp id,                             fw_size, fw_offset,
//     //                           ap_sku_id,    build_mode
//     {{0xff00, 0xFFFFFFF0, 0x0, 0xffeeeeff, NV_BUILD_MODE},
//      {0xc000, 0xFFFFFFF0, 0x0, 0x6e070000, NV_BUILD_MODE}}
// };

// AP component ID Information
constexpr uint8_t                          ApNum            = 0;
constexpr std::array<uint16_t, ApNum>      AllApComponentId = {{}};
constexpr inline std::array<FwInfo, ApNum> FwInfoList{{}};
static_assert(ApNum < NV_PLDM_MAX_COMPONENT_SIZE, "ApNum should be less than 3");
}  // namespace nv::pldm

constexpr bool CPLD_ProgramN_Pin_Enabled = false;

#endif
