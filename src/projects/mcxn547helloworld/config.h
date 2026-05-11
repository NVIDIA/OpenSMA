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
#define USB_CONFIG_COMPOSITE (1U)
#define USB_CONFIG_LSTP      (1U)

// Debug console configuration (NV_UART_INSTANCE, etc.)
#include "DebugConsoleConfig.h"

#ifdef __cplusplus
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>

#include "nv/gpio/common.h"
#include "nv/lstp/lstp_common.h"
#include "nv/i2c/sensor.h"
#include "nv/i2c/smb_direct.h"
#include "nv/mctp/router.h"
#include "nv/mctp/nsm_event.h"
#include "nv/watchdog/notify_interface.h"
#include "nv/i2c/port.h"
#include "nv/i2c/slave_function.h"
#include "nv/ipchandler/enums.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/nsm_common.h"
#include "nv/mctp/nsm_msg_bitmask.h"
#include "nv/telemetry/utils.h"
#include "sys/common/common.h"
#include "nv/iox/common.h"
#include "nv/pldm/common.h"
#include "nv/vrot/interface/types.h"
#include "nv/volt_mon/common.h"
#include "core0/powersensor.h"
#include "nv/vruart/common.h"

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
constexpr bool EnableLstp = true;

/********************* Uart over USB Config starts *********************/
constexpr nv::vruart::Signal   pintx{.port = 2, .pin = 3};
constexpr nv::vruart::Signal   pinrx{.port = 2, .pin = 4};
constexpr nv::vruart::Baudrate UartOverUsbBaudrate     = 115200U;
constexpr nv::vruart::EdmaChn  UartOverUsbEdmaTxChn    = nv::vruart::EdmaChn::_0;
constexpr nv::vruart::EdmaChn  UartOverUsbEdmaRxChn    = nv::vruart::EdmaChn::_1;
constexpr nv::vruart::EdmaInst UartOverUsbEdmaInstance = nv::vruart::EdmaInst::_1;
constexpr nv::vruart::Instance UartOverUsbUartInstance = nv::vruart::Instance::_9;
constexpr nv::vruart::Protocol UartOverUsbProtocol     = nv::vruart::Protocol::Lstp;
constexpr uint32_t UbridgeQueueSize = (UartOverUsbProtocol == nv::vruart::Protocol::Lstp) ? 512
                                                                                          : 514;
/********************* Uart over USB Config ends *********************/
}  // namespace nv::ipc

namespace nv::lstp {

constexpr uint8_t  LstpNumChannels = 6;
constexpr uint16_t LstpGpioNum     = 2;

// clang-format off
constexpr inline std::array<LstpChannelEntry, LstpNumChannels> LstpChannels{
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::Management, 0, "MCU"},
        LstpManagementConfig{LSTP_VERSION, LstpNumChannels}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::Spi, 0, "SPI"},
        LstpSpiChannelConfig{2, 24000000}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c3), "I2C"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::Gpio, 0, "GPIO"},
        LstpGpioChannelConfig{LstpGpioNum}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::Ipmi, 0, "SSIF"},
        LstpIpmiChannelConfig{}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::Uart, static_cast<uint8_t>(nv::ipc::UartOverUsbUartInstance), "UART"},
        LstpUartChannelConfig{nv::ipc::UartOverUsbBaudrate}
    }
};
// clang-format on

static_assert(ValidateLstpChannelConfigs(LstpChannels));
static_assert(nv::ipc::EnableLstp == (LstpNumChannels > 0));

constexpr bool EnableSpi  = IsChannelEnabled(LstpChannels, LstpChannelType::Spi);
constexpr bool EnableGpio = IsChannelEnabled(LstpChannels, LstpChannelType::Gpio);
constexpr bool EnableI2c  = IsChannelEnabled(LstpChannels, LstpChannelType::I2c);
constexpr bool EnableIpmi = IsChannelEnabled(LstpChannels, LstpChannelType::Ipmi);
constexpr bool EnableUart = IsChannelEnabled(LstpChannels, LstpChannelType::Uart);

}  // namespace nv::lstp

namespace nv::ipc {

// Indicate if it is dual core project
constexpr bool EnableDualCore = true;
constexpr bool EnableNcsi     = false;
// Enable SmartDMA for I3C
#if defined(I3C_DMA_USE_STOP_OFFLOAD) || (I3C_DMA_USE_STOP_OFFLOAD == 1)
constexpr bool EnableSmartDMA = true;
#else
constexpr bool EnableSmartDMA = false;
#endif

// After USB bus reset while enumerated, record mailbox and soft-reset
constexpr bool EnableUsbPortResetSelfReset = false;

/// All Tasks must be part of this enum.
enum class TaskId
{
    Begin,
    Mctp = Begin,
    I2c0,
    I2c1,
    I2c2,
    I2c3,
    Lstp,
#ifdef NV_UNITTEST
    Unittest,
#endif
    Pldm,
    Logger,
    Ssif,
    Ipc0,
    Ipc1,
    Privileged = Ipc1 + 1,
    Usb        = Privileged + 0,
    Ubridge    = Privileged + 1,
    Flash      = Privileged + 2,
    I3c0       = Privileged + 3,
    Spi0       = Privileged + 4,
    Diag       = Privileged + 5,
    Spdm       = Privileged + 6,
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
    TaskInfo{    TaskId::I2c3,    CoreId::Core0},
    TaskInfo{    TaskId::Lstp,    CoreId::Core0},
#ifdef NV_UNITTEST
    TaskInfo{TaskId::Unittest,    CoreId::Core0},
#endif
    TaskInfo{    TaskId::Pldm,    CoreId::Core0},
    TaskInfo{  TaskId::Logger,    CoreId::Core0},
    TaskInfo{    TaskId::Ssif,    CoreId::Core0},
    TaskInfo{    TaskId::Ipc0,    CoreId::Core0},
    TaskInfo{    TaskId::Ipc1,    CoreId::Core1},
    TaskInfo{     TaskId::Usb,    CoreId::Core0},
    TaskInfo{ TaskId::Ubridge,    CoreId::Core0},
    TaskInfo{   TaskId::Flash,    CoreId::Core0},
    TaskInfo{    TaskId::I3c0,    CoreId::Core0},
    TaskInfo{    TaskId::Spi0,    CoreId::Core0},
    TaskInfo{    TaskId::Diag,    CoreId::Core0},
    TaskInfo{    TaskId::Spdm,    CoreId::Core0},
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
    HardwareInfo{        HardwareId::USB,     get_core_from_task(TaskId::Usb)},
    HardwareInfo{      HardwareId::I3C_0, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{      HardwareId::I3C_1,    get_core_from_task(TaskId::I3c0)},
    HardwareInfo{ HardwareId::eDMA_0_CH0, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{ HardwareId::eDMA_0_CH1, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{ HardwareId::eDMA_1_CH0,    get_core_from_task(TaskId::I3c0)},
    HardwareInfo{ HardwareId::eDMA_1_CH1,    get_core_from_task(TaskId::I3c0)},
    HardwareInfo{ HardwareId::eDMA_0_CH9,    get_core_from_task(TaskId::Spi0)},
    HardwareInfo{HardwareId::eDMA_0_CH10,    get_core_from_task(TaskId::Spi0)},
    HardwareInfo{  HardwareId::FLEXCOMM0,    get_core_from_task(TaskId::I2c1)},
    HardwareInfo{  HardwareId::FLEXCOMM1,    get_core_from_task(TaskId::I2c2)},
    HardwareInfo{  HardwareId::FLEXCOMM2,    get_core_from_task(TaskId::I2c3)},
    HardwareInfo{  HardwareId::FLEXCOMM3,    get_core_from_task(TaskId::I2c0)},
    HardwareInfo{  HardwareId::FLEXCOMM4, get_core_from_task(TaskId::Invalid)},
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
    Ssif,
    UbridgeTx,
    UbridgeRx,
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

/// All Events must be part of this enum.
enum class EventId
{
    Begin,
    I2c0,
    I2c1,
    I2c2,
    I2c3,
    I3c0,
    PldmTask,
    UsbTask,
    FlashEvent,
    Spi0Event,
    Spi1Event,
    Spi2Event,
    LogEvent,
    SpdmTask,
    DiagEvent,
    TaskBootStatus,
    TaskAliveStatus,
    Spi0EdmaDriverEvent,
    Lstp,
    Ssif,
    UartBridgeEvent,
    End
};

using EventInfo = std::tuple<EventId, CoreId>;

constexpr inline std::array<EventInfo, int(EventId::End)> EventInfos{
    EventInfo{              EventId::Begin,                     CoreId::Invalid},
    EventInfo{               EventId::I2c0,    get_core_from_task(TaskId::I2c0)},
    EventInfo{               EventId::I2c1,    get_core_from_task(TaskId::I2c1)},
    EventInfo{               EventId::I2c2,    get_core_from_task(TaskId::I2c2)},
    EventInfo{               EventId::I2c3,    get_core_from_task(TaskId::I2c3)},
    EventInfo{               EventId::I3c0,    get_core_from_task(TaskId::I3c0)},
    EventInfo{           EventId::PldmTask,    get_core_from_task(TaskId::Pldm)},
    EventInfo{            EventId::UsbTask,     get_core_from_task(TaskId::Usb)},
    EventInfo{         EventId::FlashEvent,   get_core_from_task(TaskId::Flash)},
    EventInfo{          EventId::Spi0Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{          EventId::Spi1Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{          EventId::Spi2Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{           EventId::LogEvent,  get_core_from_task(TaskId::Logger)},
    EventInfo{           EventId::SpdmTask,                        CoreId::Both},
    EventInfo{          EventId::DiagEvent,    get_core_from_task(TaskId::Diag)},
    EventInfo{     EventId::TaskBootStatus,                       CoreId::Core0},
    EventInfo{    EventId::TaskAliveStatus,                        CoreId::Both},
    EventInfo{EventId::Spi0EdmaDriverEvent,    get_core_from_task(TaskId::Spi0)},
    EventInfo{               EventId::Lstp,    get_core_from_task(TaskId::Lstp)},
    EventInfo{               EventId::Ssif,    get_core_from_task(TaskId::Ssif)},
    EventInfo{    EventId::UartBridgeEvent, get_core_from_task(TaskId::Ubridge)}
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
constexpr uint8_t RoutingInfoUpdateSize   = 4;
constexpr uint8_t RoutingTableSize        = DefaultRoutingTableSize + RoutingInfoUpdateSize;
constexpr bool    EnableEndpointStatusChangeDebounce = false;
constexpr auto    EndpointStatusChangePeriodMs       = 0;

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

constexpr uint32_t ApEcdsa384PublicKeySize = 96;
constexpr std::array<std::array<uint8_t, ApEcdsa384PublicKeySize>, 2> ApFwPublicKeys{};

constexpr uint32_t SpdmRequestQueueSize      = 2048;
constexpr uint32_t SpdmRxQueueSize           = 72;
constexpr uint32_t SpdmCryptoHelperQueueSize = 12;
constexpr bool     SpdmI2cResponder          = true;
constexpr bool     SpdmDummyCertificates     = true;
constexpr uint32_t SpdmCryptoHelperMaxItems  = EnableDualCore ? 2 : 1;
using QueueInfo                              = std::tuple<QueueId, uint8_t, uint16_t, CoreId>;

// USB Low-Speed Transport Protocol (LSTP) for I2C/SSIF/SPI/GPIO tunneling
constexpr uint32_t UsbLstpMsgSize = EnableLstp ? 512 : 1;
constexpr uint32_t LstpTxQDepth   = EnableLstp ? nv::lstp::LstpNumChannels : 1;
// Note: LstpTx queue depth <= number of channels (< if messages from different channels never
// come concurrently)
constexpr uint32_t LstpToSpiQSize    = nv::lstp::EnableSpi ? UsbLstpMsgSize : 1;
constexpr uint32_t LstpToGpioQSize   = nv::lstp::EnableGpio ? UsbLstpMsgSize : 1;
constexpr uint32_t LstpGpioIrqQSize  = nv::lstp::EnableGpio
                                         ? sizeof(nv::lstp::LstpGpioIrqEventRequest)
                                         : 1;
constexpr uint32_t LstpGpioIrqQDepth = nv::lstp::EnableGpio ? nv::lstp::LstpGpioNum : 1;

/// define all queue lengths and item_sizes here
constexpr inline std::array<QueueInfo, int(QueueId::End)> QueueInfos{
    // id, len, item_size
    QueueInfo{    QueueId::MctpDataRequest,130,                                                 72,get_core_from_task(TaskId::Mctp)                                                                                                             },
    {    QueueId::MctpPldmRequest,            1,                                                256,    get_core_from_task(TaskId::Mctp)},
    {    QueueId::MctpSpdmRequest,            1,                               SpdmRequestQueueSize,    get_core_from_task(TaskId::Mctp)},
    {            QueueId::MctpCmd,          130,                                                 12,    get_core_from_task(TaskId::Mctp)},
    {               QueueId::I2c0,            8,                                                528,    get_core_from_task(TaskId::I2c0)},
    {               QueueId::I2c1,            8,                                                528,    get_core_from_task(TaskId::I2c1)},
    {               QueueId::I2c2,            8,                                                528,    get_core_from_task(TaskId::I2c2)},
    {               QueueId::I2c3,            8,                                                528,    get_core_from_task(TaskId::I2c3)},
    {               QueueId::I2c4,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I2c5,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I2c6,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I2c7,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I2c8,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I2c9,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I3c0,            8,                                                524,    get_core_from_task(TaskId::I3c0)},
    {               QueueId::I3c1,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi0,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi1,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi2,            1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {             QueueId::PldmRx,            1,                                                 72,    get_core_from_task(TaskId::Pldm)},
    {           QueueId::PldmRx4k,            1,                                          4096 + 32,    get_core_from_task(TaskId::Pldm)},
    {              QueueId::UsbTx,            1,                                                 72,     get_core_from_task(TaskId::Usb)},
    {       QueueId::FlashRequest,            2,                                                280,   get_core_from_task(TaskId::Flash)},
    {      QueueId::FlashResponse,            1,                                                272,                        CoreId::Both},
    {       QueueId::RoutingTable,
              RoutingTableSize, sizeof(mctp::ShardRoutingTable) * RoutingTableSize,
              CoreId::Both                                                                                                                       },
    {         QueueId::LogRequest,           64,                                                 22,  get_core_from_task(TaskId::Logger)},
    {QueueId::LogResponseBlocking,            1,                                                 22,  get_core_from_task(TaskId::Logger)},
    {        QueueId::LogDownload,            1,                                                 22,  get_core_from_task(TaskId::Logger)},
    // LogDownloadResp is related to Mctp since queue recv only use in
    // nv::logger::Task::download, which is only used in VDM command (But it doesn't block other
    // task from using nv::logger::Task::download)
    {    QueueId::LogDownloadResp,            1,                                                 22,    get_core_from_task(TaskId::Mctp)},
    {             QueueId::LogISR,            6,                                                 22,  get_core_from_task(TaskId::Logger)},
    {          QueueId::FlashSema,            1,                                                  1,                        CoreId::Both},
    {             QueueId::SpdmRx,            1,                                    SpdmRxQueueSize,    get_core_from_task(TaskId::Spdm)},
    {   QueueId::SpdmCryptoHelper,
              SpdmCryptoHelperMaxItems,                          SpdmCryptoHelperQueueSize,
              get_core_from_task(TaskId::Spdm)                                                                                                   },
    {             QueueId::UsbHid,            1,                                                517,     get_core_from_task(TaskId::Usb)},
    {          QueueId::LstpToSpi,            1,                                     LstpToSpiQSize,    get_core_from_task(TaskId::Spi0)},
    {             QueueId::LstpTx, LstpTxQDepth,                                     UsbLstpMsgSize,     get_core_from_task(TaskId::Usb)},
    {        QueueId::LstpGpioIrq,
              LstpGpioIrqQDepth,                                   LstpGpioIrqQSize,
              get_core_from_task(TaskId::Lstp)                                                                                                   },
    {         QueueId::LstpToGpio,            1,                                    LstpToGpioQSize,    get_core_from_task(TaskId::Lstp)},
    {               QueueId::Ssif,            1,                                                258,    get_core_from_task(TaskId::Ssif)},
    {          QueueId::UbridgeTx,            4,                                   UbridgeQueueSize, get_core_from_task(TaskId::Ubridge)},
    {          QueueId::UbridgeRx,            4,                                   UbridgeQueueSize, get_core_from_task(TaskId::Ubridge)}
};

constexpr uint32_t I3cQueueMaxTxSize = 128;

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
    SmbusCacheRefresh,
    Ap1Status,
    Ap2Status,
    Ap3Status,
    EepromUpdate,
    I2c3RepeatedStartTimeout,
    EndpointStatusChangeStart,
    EndpointStatusChangeEnd = EndpointStatusChangeStart + DownStreamNum - 1,
    I2CTargetTimeoutCheck,
    End
};

using TimerInfo = std::tuple<TimerId, CoreId>;

namespace {
constexpr auto make_timer_infos()
{
    std::array<TimerInfo, int(TimerId::End)> infos{};
    int                                      idx = 0;

    infos[idx++] = TimerInfo{TimerId::MctpEnumerate, get_core_from_task(TaskId::Mctp)};
    infos[idx++] = TimerInfo{TimerId::Pldm, get_core_from_task(TaskId::Pldm)};
    infos[idx++] = TimerInfo{TimerId::PerfMonitor, CoreId::Core0};
    infos[idx++] = TimerInfo{TimerId::MctpApPowerGoodEngage, get_core_from_task(TaskId::Mctp)};
    infos[idx++] = TimerInfo{TimerId::Bootloader, CoreId::Core0};
    infos[idx++] = TimerInfo{TimerId::RuntimeWatchDog, CoreId::Core0};
    infos[idx++] = TimerInfo{TimerId::Gpu1Seneor, get_core_from_task(TaskId::Invalid)};
    infos[idx++] = TimerInfo{TimerId::Gpu2Seneor, get_core_from_task(TaskId::Invalid)};
    infos[idx++] = TimerInfo{TimerId::SmbSensor, get_core_from_task(TaskId::Invalid)};
    infos[idx++] = TimerInfo{TimerId::SmbusCacheRefresh, get_core_from_task(TaskId::Mctp)};
    infos[idx++] = TimerInfo{TimerId::Ap1Status, get_core_from_task(TaskId::Invalid)};
    infos[idx++] = TimerInfo{TimerId::Ap2Status, get_core_from_task(TaskId::Invalid)};
    infos[idx++] = TimerInfo{TimerId::Ap3Status, get_core_from_task(TaskId::Invalid)};
    infos[idx++] = TimerInfo{TimerId::EepromUpdate, get_core_from_task(TaskId::Invalid)};
    infos[idx++] = TimerInfo{TimerId::I2c3RepeatedStartTimeout,
                             get_core_from_task(TaskId::I2c3)};
    for (int i = 0; i < DownStreamNum; ++i) {
        infos[idx++] = TimerInfo{
            static_cast<TimerId>(static_cast<int>(TimerId::EndpointStatusChangeStart) + i),
            get_core_from_task(TaskId::Mctp)};
    }

    infos[idx++] = TimerInfo{TimerId::I2CTargetTimeoutCheck, get_core_from_task(TaskId::I2c0)};

    return infos;
}
}  // namespace

constexpr inline std::array<TimerInfo, int(TimerId::End)> TimerInfos = make_timer_infos();

using I2cTimerInfo = std::tuple<TimerId, QueueId>;

constexpr inline std::array<I2cTimerInfo, 1> I2cTimerInfos{
    I2cTimerInfo{TimerId::I2c3RepeatedStartTimeout, QueueId::I2c3}
};

// Limits
[[maybe_unused]] constexpr auto MaxQueuesPerTask = 4;
[[maybe_unused]] constexpr auto MaxEventsPerTask = 4;

constexpr uint8_t GpioNum = 6;
enum Port : gpio::GpioPort
{
    GlobalWpPort = nv::gpio::InvalidGpioPort,
    ApGoodPort   = nv::gpio::InvalidGpioPort,
    // Port 0
    GPIO_LED_PORT = 0,
    GPIO_INT_PORT = 0,
    // Port 1
    CPU0_SSIF_PORT = 1,
    // Port 3
    SPI0_CS0_PORT = 3,
    SPI0_CS1_PORT = 3,
    // Port 4
    THERM_OVERT_N_PORT = 4
};

enum Pin : gpio::GpioPin
{
    GlobalWpPin = nv::gpio::InvalidGpioPin,
    ApGoodPin   = nv::gpio::InvalidGpioPin,
    // Port 0 pins
    GPIO_LED_PIN = 5,
    GPIO_INT_PIN = 26,
    // Port 1 pins
    CPU0_SSIF_PIN = 6,
    // Port 3
    SPI0_CS0_PIN = 0,
    SPI0_CS1_PIN = 19,
    // Port 4 pins
    THERM_OVERT_N_PIN = 15
};

using Gpios = std::tuple<gpio::GpioPort, gpio::GpioPin>;

constexpr inline std::array<Gpios, GpioNum> GpioSetup{
    Gpios{     GPIO_LED_PORT,      GPIO_LED_PIN}, // OUT
    Gpios{     GPIO_INT_PORT,      GPIO_INT_PIN}, // IN
    Gpios{    CPU0_SSIF_PORT,     CPU0_SSIF_PIN}, // IN
    Gpios{     SPI0_CS0_PORT,      SPI0_CS0_PIN},
    Gpios{     SPI0_CS1_PORT,      SPI0_CS1_PIN},
    Gpios{THERM_OVERT_N_PORT, THERM_OVERT_N_PIN}  // IN
};

// Product-specific NSM Event GPIO configuration
constexpr uint8_t                                            NsmEventGpioNum   = 0;
constexpr std::array<nv::ipc::NsmEventGpio, NsmEventGpioNum> GpioNsmEventSetup = {};

// GPIO NSM Event mask arrays - automatically generated from GpioNsmEventSetup
constexpr inline std::array<uint32_t, sys::gpio::PortsNumber + 1>
    GpioNsmEventMask = nv::ipc::PopulateGpioNsmEventMaskArray(GpioNsmEventSetup);

constexpr inline std::array<uint32_t, sys::gpio::PortsNumber + 1>
    GpioNsmEventAssertMask = nv::ipc::PopulateGpioNsmEventAssertMaskArray(GpioNsmEventSetup);

// TODO: confirming Rising/Falling edge trigger for each interrupt
using GpioInterruptConfig = std::tuple<nv::gpio::GpioPort,
                                       nv::gpio::GpioPin,
                                       nv::gpio::InterruptDetection,
                                       nv::gpio::InterruptSelect>;

constexpr int GpioInterruptNum = 5;

constexpr inline std::array<GpioInterruptConfig, GpioInterruptNum> GpioInterruptSetup{
    GpioInterruptConfig{4,
                        12,nv::gpio::InterruptDetection::InterruptRising,
                        nv::gpio::InterruptSelect::InterruptSelect0}, //  GA_GPIO9_IROT_ERROR_N
    GpioInterruptConfig{
                        4,                    13,
                        nv::gpio::InterruptDetection::InterruptRising,
                        nv::gpio::InterruptSelect::InterruptSelect0}, //  GA_GPIO10_IROT_AP_BOOT_COMPLETE
    GpioInterruptConfig{            4,
                        15,   nv::gpio::InterruptDetection::InterruptRising,
                        nv::gpio::InterruptSelect::InterruptSelect0}, //  THERM_OVERT
    GpioInterruptConfig{            4,
                        20,   nv::gpio::InterruptDetection::InterruptRising,
                        nv::gpio::InterruptSelect::InterruptSelect0},
    GpioInterruptConfig{GPIO_INT_PORT,
                        GPIO_INT_PIN, nv::gpio::InterruptDetection::InterruptDisabled,
                        nv::gpio::InterruptSelect::InterruptSelect0},
};  // PS_BOARD_PGOOD

constexpr uint32_t CtimerFrequency = 48000000;

constexpr auto UartInstance = NV_UART_INSTANCE;

enum BootedEventBits : uint32_t
{
    Mctp  = nv::common::bit(0),
    I2c0  = nv::common::bit(1),
    I2c1  = nv::common::bit(2),
    I2c2  = nv::common::bit(3),
    I2c3  = nv::common::bit(4),
    I3c0  = nv::common::bit(5),
    Spi0  = nv::common::bit(6),
    Usb   = nv::common::bit(7),
    Flash = nv::common::bit(8),
#ifdef NV_UNITTEST
    Unittest,
#endif
    Pldm           = nv::common::bit(9),
    Logger         = nv::common::bit(10),
    Spdm           = nv::common::bit(11),
    Lstp           = nv::common::bit(12),
    Ssif           = nv::common::bit(13),
    Ubridge        = nv::common::bit(14),
    BootStatusMask = (nv::common::bit(15) - 1),
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
    bool                  need_debug_token     = false;
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
constexpr uint32_t SmbSensorUpdateMs   = 50 * 1000;
constexpr uint32_t I3cSensorUpdateMs   = 50 * 1000;
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

/******** ******** Iox Emulation Config Starts ******** ********/
constexpr bool                                          EnableIoxEmulation = false;
constexpr inline size_t                                 IoxNum             = 0;
constexpr inline uint8_t                                IoxI2cBaseAddr     = 0x50;
constexpr uint32_t                                      IoxFilterSeconds   = 0;
constexpr inline std::array<nv::iox::IoxConfig, IoxNum> IoxConfigs{};
/******** ******** Iox Emulation Config Ends ******** ********/

// SMA_READY pin configuration
constexpr bool               EnableMcuReadyPin = false;
constexpr nv::gpio::GpioPort McuReadyPinPort   = 0;
constexpr nv::gpio::GpioPin  McuReadyPinPin    = 0;

constexpr bool EnableForwardNvlInfo = false;

constexpr bool EnableI2CErrorInjection = false;

// EEPROM bridge configuration (disabled)
constexpr bool            EnableEepromBridge = false;
constexpr uint8_t         EepromDstAddress   = 0x50;
constexpr nv::ipc::TaskId EepromTaskId       = nv::ipc::TaskId::Invalid;
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

namespace nv::lstp {
using namespace nv::ipc;
// clang-format off
constexpr inline std::array<LstpGpioPinInfo, LstpGpioNum> PinConfigs{
    {
        {GPIO_LED_PORT,GPIO_LED_PIN,{{"GPIO_LED"},LstpGpioDirection::Output,LstpGpioState::High,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::NoPull,0,10,100,100,0,0,0}},
        {GPIO_INT_PORT,GPIO_INT_PIN,{{"GPIO_INT"},LstpGpioDirection::Input, LstpGpioState::Low, LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0},true},
    }
};
// clang-format on
}  // namespace nv::lstp

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
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdGetDeviceCapabilitiesV2));
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
    // Default does nothing
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

constexpr auto mcuTemperatureSensorsSize = 1;
/** List of active MCU sensors for NSM type 3 Temperature Reading */
constexpr inline std::array<Type3TemperatureSensors, mcuTemperatureSensorsSize>
    mcuTemperatureSensors{TempSMAInternal};

constexpr auto mcuPowerSensorsSize = 0;
/** List of active MCU sensors for NSM type 3 Power Draw */
constexpr inline std::array<Type3PowerSensors, mcuPowerSensorsSize> mcuPowerSensors{};

constexpr auto mcuVoltageSensorsSize = 0;
/** List of active MCU sensors for NSM type 3 Voltage */
constexpr inline std::array<T3Voltage, mcuVoltageSensorsSize> mcuVoltageSensors{};

// Type 4 Diagnostics Telemetries
constexpr auto T4TelemetriesSize = 2;
using T3TagId                    = uint8_t;
constexpr T3TagId NoT3Tag        = 0xff;
using McuDiagnosticTelemetry     = std::
    tuple<Type4McuDiagnosticEntries, Type4TelemetryTypes, T3TagId>;
constexpr inline std::array<McuDiagnosticTelemetry, T4TelemetriesSize> mcuDiagnosticTelemetries{
    McuDiagnosticTelemetry{DIAG_GPIO_VALUE_BITMAP,        GpioTelemetry,         NoT3Tag},
    McuDiagnosticTelemetry{    DIAG_INTERNAL_TEMP, TemperatureTelemetry, TempSMAInternal},
};

// Empty temperature sensor tables for compatibility
// Temperature Sensors Config for VR products
constexpr uint8_t I2cTempSensorSize = 0;

// This data is only shared between main.cpp and mctp task. Should not impact when migrating to
// dual core.
NV_SHARED_DATA inline std::array<nv::i2c::I2cTempSensorConfig, I2cTempSensorSize>
    I2cTempSensorList{};

}  // namespace nv::mctp

namespace nv::vrot {
constexpr inline std::array<ApInfo, 0> ApList = {{}};
}  // namespace nv::vrot

namespace nv::i2c {
constexpr size_t                      I2cBufferSize                   = 512;
constexpr bool                        EnableI2cPeripheralRecovery     = false;
constexpr uint32_t                    I2CTargetTimeoutUs              = 1000 * 1000;
constexpr uint32_t                    I2CTargetTimeoutCheckIntervalMs = 0;
constexpr std::array<mctp::Client, 0> I2CTargetTimeoutCheckClients{};
constexpr mctp::Client                I2cTargetTimeoutTimerClient = mctp::Client::End;

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

static_assert(!(SmbusCacheRefreshMs > 0 && nv::ipc::SmbSensorUpdateMs > 0),
              "Only One Timer for SMbus Direct can be enabled for I2c0 task!!!");

}  // namespace nv::i2c

namespace nv::lstp {
// Device specific constraint on LSTP I2C buffer size. Make sure you really want to set this
constexpr bool I2cSmallBuffer = false;

static_assert(EnableI2c ? (I2cSmallBuffer ? nv::i2c::I2cBufferSize < 512
                                          : nv::i2c::I2cBufferSize == 512)
                        : !I2cSmallBuffer);
}  // namespace nv::lstp

/******** ******** Voltage Monitor Config (Disabled) ******** ********/
// clang-format off
namespace nv::ipc::voltage_monitor_config {
using namespace nv::volt_mon;
// MCU_INTERNAL_TEMP_USE_LEGACY_API  (expanded inline per code review)
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
}  // namespace nv::ipc::voltage_monitor_config
// clang-format on

#endif
