/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES.
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

// USB configuration for C and C++ definitions
// #define USB_CONFIG_MCTP (1U)
#define USB_CONFIG_COMPOSITE (1U)
#define USB_CONFIG_LSTP      (1U)

// Debug console configuration (NV_UART_INSTANCE, etc.)
#include "DebugConsoleConfig.h"

#ifdef __cplusplus
#pragma once
#include <array>
#include <cstdint>
#include <tuple>
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

namespace nv::telemetry {

/** A mapping of telemetry id to index in the cache */
constexpr uint8_t TelemIndexMapSize = 12;
using TelemIndexMap                 = std::tuple<nv::telemetry::TelemId, int>;
constexpr inline std::array<TelemIndexMap, TelemIndexMapSize> TelemIndexMapList{
    TelemIndexMap{     nv::telemetry::TelemId::Gpu1Temp, -1},
    TelemIndexMap{     nv::telemetry::TelemId::Gpu2Temp, -1},
    TelemIndexMap{    nv::telemetry::TelemId::Gpu1Power, -1},
    TelemIndexMap{    nv::telemetry::TelemId::Gpu2Power, -1},
    TelemIndexMap{  nv::telemetry::TelemId::ModulePower, -1},
    TelemIndexMap{  nv::telemetry::TelemId::ModuleTemp1, -1},
    TelemIndexMap{  nv::telemetry::TelemId::ModuleTemp2, -1},
    TelemIndexMap{ nv::telemetry::TelemId::InternalTemp,  0},
    TelemIndexMap{nv::telemetry::TelemId::MaxModuleTemp, -1},
    TelemIndexMap{         nv::telemetry::TelemId::Gpio, -1},
    TelemIndexMap{   nv::telemetry::TelemId::CX8_1_Temp, -1},
    TelemIndexMap{   nv::telemetry::TelemId::CX8_2_Temp, -1},
};

}  // namespace nv::telemetry

namespace nv::ipc {
constexpr bool EnableLstp   = true;
constexpr auto UartInstance = NV_UART_INSTANCE;
}  // namespace nv::ipc

namespace nv::lstp {

constexpr uint8_t  LstpNumChannels = (nv::ipc::UartInstance == 2) ? 9 : 10;
constexpr uint16_t LstpGpioNum     = 8;

// clang-format off
constexpr inline std::array<LstpChannelEntry, LstpNumChannels> LstpChannels{
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::Management, 0, "P3957_MCU"},
        LstpManagementConfig{LSTP_VERSION, LstpNumChannels}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c1), "I2C_SSD0"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c2), "I2C_SSD1"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
#if !defined(NV_UART_INSTANCE) || (NV_UART_INSTANCE != 2)
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c3), "I2C_SSD2"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
#endif  // #if !defined(NV_UART_INSTANCE) || (NV_UART_INSTANCE != 2)
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c4), "I2C_SSD3"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c5), "I2C_SSD4"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c6), "I2C_SSD5"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c7), "I2C_SSD6"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::I2c, static_cast<uint8_t>(ipchandler::Id::I2c8), "I2C_SSD7"},
        LstpI2cChannelConfig{static_cast<uint8_t>(LstpI2cSpeed::Fast)}
    },
    LstpChannelEntry{
        LstpChannelInfo{LstpChannelType::Gpio, 0, "GPIO"},
        LstpGpioChannelConfig{LstpGpioNum}
    },
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
constexpr bool EnableDualCore = false;
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
    Lstp,
    I2c1,
    I2c2,
    I2c3,
    I2c4,
    I2c5,
    I2c6,
    I2c7,
    I2c8,
#ifdef NV_UNITTEST
    Unittest,
#endif
    Pldm,
    Logger,
    Privileged = Logger + 1,
    Usb        = Privileged + 0,
    Flash      = Privileged + 1,
    NHP        = Privileged + 2,
    Diag       = Privileged + 3,
    Spdm       = Privileged + 4,
    EndPrivileged,
    End   = EndPrivileged,
    Timer = End,
    Idle,
    KernelEnd,
    Invalid = KernelEnd,
    I2c0    = Invalid,
};

using TaskInfo = std::tuple<TaskId, CoreId>;

constexpr inline std::array<TaskInfo, int(TaskId::KernelEnd) + 1> TaskInfos{
    TaskInfo{    TaskId::Mctp,    CoreId::Core0},
    TaskInfo{    TaskId::Lstp,    CoreId::Core0},
    TaskInfo{    TaskId::I2c1,    CoreId::Core0},
    TaskInfo{    TaskId::I2c2,    CoreId::Core0},
    TaskInfo{    TaskId::I2c3,    CoreId::Core0},
    TaskInfo{    TaskId::I2c4,    CoreId::Core0},
    TaskInfo{    TaskId::I2c5,    CoreId::Core0},
    TaskInfo{    TaskId::I2c6,    CoreId::Core0},
    TaskInfo{    TaskId::I2c7,    CoreId::Core0},
    TaskInfo{    TaskId::I2c8,    CoreId::Core0},
#ifdef NV_UNITTEST
    TaskInfo{TaskId::Unittest,    CoreId::Core0},
#endif
    TaskInfo{    TaskId::Pldm,    CoreId::Core0},
    TaskInfo{  TaskId::Logger,    CoreId::Core0},
    TaskInfo{     TaskId::Usb,    CoreId::Core0},
    TaskInfo{   TaskId::Flash,    CoreId::Core0},
    TaskInfo{     TaskId::NHP,    CoreId::Core0},
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
    HardwareInfo{      HardwareId::USB,     get_core_from_task(TaskId::Usb)},
    HardwareInfo{    HardwareId::I3C_0, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{    HardwareId::I3C_1, get_core_from_task(TaskId::Invalid)},
    HardwareInfo{HardwareId::FLEXCOMM0,    get_core_from_task(TaskId::I2c8)},
    HardwareInfo{HardwareId::FLEXCOMM1,    get_core_from_task(TaskId::I2c2)},
    HardwareInfo{HardwareId::FLEXCOMM2,    get_core_from_task(TaskId::I2c3)},
    HardwareInfo{HardwareId::FLEXCOMM3,    get_core_from_task(TaskId::I2c7)},
    HardwareInfo{HardwareId::FLEXCOMM4,    get_core_from_task(TaskId::I2c4)},
    HardwareInfo{HardwareId::FLEXCOMM5,    get_core_from_task(TaskId::I2c5)},
    HardwareInfo{HardwareId::FLEXCOMM6,     get_core_from_task(TaskId::NHP)},
    HardwareInfo{HardwareId::FLEXCOMM7,    get_core_from_task(TaskId::I2c1)},
    HardwareInfo{HardwareId::FLEXCOMM8,    get_core_from_task(TaskId::I2c6)},
    HardwareInfo{HardwareId::FLEXCOMM9,     get_core_from_task(TaskId::NHP)},
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
    Nhp,
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
    End
};

using EventInfo = std::tuple<EventId, CoreId>;

constexpr inline std::array<EventInfo, int(EventId::End)> EventInfos{
    EventInfo{              EventId::Begin,                     CoreId::Invalid},
    EventInfo{                EventId::Nhp,     get_core_from_task(TaskId::NHP)},
    EventInfo{               EventId::I2c0, get_core_from_task(TaskId::Invalid)},
    EventInfo{               EventId::I2c1,    get_core_from_task(TaskId::I2c1)},
    EventInfo{               EventId::I2c2,    get_core_from_task(TaskId::I2c2)},
    EventInfo{               EventId::I2c3,    get_core_from_task(TaskId::I2c3)},
    EventInfo{               EventId::I2c4,    get_core_from_task(TaskId::I2c4)},
    EventInfo{               EventId::I2c5,    get_core_from_task(TaskId::I2c5)},
    EventInfo{               EventId::I2c6,    get_core_from_task(TaskId::I2c6)},
    EventInfo{               EventId::I2c7,    get_core_from_task(TaskId::I2c7)},
    EventInfo{               EventId::I2c8,    get_core_from_task(TaskId::I2c8)},
    EventInfo{               EventId::I2c9, get_core_from_task(TaskId::Invalid)},
    EventInfo{               EventId::I3c0, get_core_from_task(TaskId::Invalid)},
    EventInfo{               EventId::I3c1, get_core_from_task(TaskId::Invalid)},
    EventInfo{           EventId::PldmTask,    get_core_from_task(TaskId::Pldm)},
    EventInfo{            EventId::UsbTask,     get_core_from_task(TaskId::Usb)},
    EventInfo{         EventId::FlashEvent,   get_core_from_task(TaskId::Flash)},
    EventInfo{          EventId::Spi0Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{          EventId::Spi1Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{          EventId::Spi2Event, get_core_from_task(TaskId::Invalid)},
    EventInfo{           EventId::LogEvent,  get_core_from_task(TaskId::Logger)},
    EventInfo{           EventId::SpdmTask,    get_core_from_task(TaskId::Spdm)},
    EventInfo{          EventId::DiagEvent,    get_core_from_task(TaskId::Diag)},
    EventInfo{     EventId::TaskBootStatus,                       CoreId::Core0},
    EventInfo{    EventId::TaskAliveStatus,                       CoreId::Core0},
    EventInfo{EventId::Spi0EdmaDriverEvent, get_core_from_task(TaskId::Invalid)},
    EventInfo{               EventId::Lstp,    get_core_from_task(TaskId::Lstp)},
};

using ClientInfo = std::tuple<mctp::Client, TaskId>;

constexpr inline std::array<ClientInfo, int(mctp::Client::End)> ClientInfos{
    ClientInfo{ mctp::Client::UsI2c, TaskId::Invalid},
    ClientInfo{ mctp::Client::UsUsb,     TaskId::Usb},
    ClientInfo{mctp::Client::DsI2c0,    TaskId::I2c1},
    ClientInfo{mctp::Client::DsI2c1,    TaskId::I2c2},
    ClientInfo{mctp::Client::DsI2c2,    TaskId::I2c3},
    ClientInfo{mctp::Client::DsI2c3,    TaskId::I2c4},
    ClientInfo{mctp::Client::DsI2c4,    TaskId::I2c5},
    ClientInfo{mctp::Client::DsI2c5,    TaskId::I2c6},
    ClientInfo{mctp::Client::DsI2c6,    TaskId::I2c7},
    ClientInfo{mctp::Client::DsI2c7,    TaskId::I2c8},
    ClientInfo{mctp::Client::DsI3c0, TaskId::Invalid},
    ClientInfo{mctp::Client::DsI3c1, TaskId::Invalid},
    ClientInfo{  mctp::Client::Pldm,    TaskId::Pldm},
    ClientInfo{  mctp::Client::Spdm,    TaskId::Spdm},
    ClientInfo{mctp::Client::Spdm4K,    TaskId::Spdm},
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

constexpr uint32_t StreamBufferRingOverhead = 0;
constexpr uint32_t C2CUsbPldmUpdate4KSize   = 0;
constexpr uint32_t C2CBufferExtraSpace      = 0;
constexpr uint32_t C2CBufferSize            = 0;
constexpr inline std::array<StreamBufferInfo, int(StreamBufferId::End)> StreamBufferInfos{};

// Down stream Information
constexpr uint8_t I2cDownStreamMctpNum   = 8;
constexpr uint8_t I2cDownStreamCp2112Num = 0;
constexpr uint8_t I2cDownStreamNum       = I2cDownStreamMctpNum + I2cDownStreamCp2112Num;
constexpr uint8_t I3cDownStreamNum       = 0;
constexpr uint8_t DownStreamNum          = I2cDownStreamNum + I3cDownStreamNum;

constexpr uint8_t I2cUpStreamNum          = 0;
constexpr uint8_t UpStreamNum             = 2;
constexpr uint8_t DefaultRoutingTableSize = DownStreamNum + UpStreamNum;
constexpr uint8_t RoutingInfoUpdateSize   = 4;
constexpr uint8_t RoutingTableSize        = DefaultRoutingTableSize + RoutingInfoUpdateSize;
constexpr bool    EnableEndpointStatusChangeDebounce = false;
constexpr auto    EndpointStatusChangePeriodMs       = 0;

constexpr inline std::array<mctp::DownStreamInfo, DownStreamNum> DownStreamInfos{
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c0,
                         0x1d},
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c1,
                         0x1d},
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c2,
                         0x1d},
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c3,
                         0x1d},
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c4,
                         0x1d},
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c5,
                         0x1d},
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c6,
                         0x1d},
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c7,
                         0x1d},
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
constexpr uint32_t UsbLstpMsgSize        = EnableLstp ? 512 : 1;
constexpr uint32_t LstpToGpioSize        = nv::lstp::EnableGpio ? UsbLstpMsgSize : 1;
constexpr uint32_t LstpGpioIrqQueueSize  = nv::lstp::EnableGpio
                                             ? sizeof(nv::lstp::LstpGpioIrqEventRequest)
                                             : 1;
constexpr uint32_t LstpGpioIrqQueueDepth = nv::lstp::EnableGpio ? nv::lstp::LstpGpioNum : 1;

/// define all queue lengths and item_sizes here
constexpr inline std::array<QueueInfo, int(QueueId::End)> QueueInfos{
    // id, len, item_size
    QueueInfo{    QueueId::MctpDataRequest,130,                                            72,get_core_from_task(TaskId::Mctp)                                                                                                    },
    {    QueueId::MctpPldmRequest,   1,                                                256,    get_core_from_task(TaskId::Mctp)},
    {    QueueId::MctpSpdmRequest,   1,                               SpdmRequestQueueSize,    get_core_from_task(TaskId::Mctp)},
    {            QueueId::MctpCmd, 130,                                                 12,    get_core_from_task(TaskId::Mctp)},
    {               QueueId::I2c0,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I2c1,  16,                                                528,    get_core_from_task(TaskId::I2c1)},
    {               QueueId::I2c2,  16,                                                528,    get_core_from_task(TaskId::I2c2)},
    {               QueueId::I2c3,  16,                                                528,    get_core_from_task(TaskId::I2c3)},
    {               QueueId::I2c4,  16,                                                528,    get_core_from_task(TaskId::I2c4)},
    {               QueueId::I2c5,  16,                                                528,    get_core_from_task(TaskId::I2c5)},
    {               QueueId::I2c6,  16,                                                528,    get_core_from_task(TaskId::I2c6)},
    {               QueueId::I2c7,  16,                                                528,    get_core_from_task(TaskId::I2c7)},
    {               QueueId::I2c8,  16,                                                528,    get_core_from_task(TaskId::I2c8)},
    {               QueueId::I2c9,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I3c0,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::I3c1,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi0,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi1,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {               QueueId::Spi2,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {             QueueId::PldmRx,   1,                                                 72,    get_core_from_task(TaskId::Pldm)},
    {           QueueId::PldmRx4k,   1,                                          4096 + 32,    get_core_from_task(TaskId::Pldm)},
    {              QueueId::UsbTx,   1,                                                 72,     get_core_from_task(TaskId::Usb)},
    {       QueueId::FlashRequest,   1,                                                280,   get_core_from_task(TaskId::Flash)},
    {      QueueId::FlashResponse,   1,                                                272,   get_core_from_task(TaskId::Flash)},
    {       QueueId::RoutingTable,
              RoutingTableSize, sizeof(mctp::ShardRoutingTable) * RoutingTableSize,
              CoreId::Core0                                                                                                             },
    {         QueueId::LogRequest,  64,                                                 22,  get_core_from_task(TaskId::Logger)},
    {QueueId::LogResponseBlocking,   1,                                                 22,  get_core_from_task(TaskId::Logger)},
    {        QueueId::LogDownload,   1,                                                 22,  get_core_from_task(TaskId::Logger)},
    // LogDownloadResp is related to Mctp since queue recv only use in
    // nv::logger::Task::download, which is only used in VDM command (But it doesn't block other
    // task from using nv::logger::Task::download)
    {    QueueId::LogDownloadResp,   1,                                                 22,    get_core_from_task(TaskId::Mctp)},
    {             QueueId::LogISR,   6,                                                 22,  get_core_from_task(TaskId::Logger)},
    {          QueueId::FlashSema,   1,                                                  1,   get_core_from_task(TaskId::Flash)},
    {             QueueId::SpdmRx,   1,                                    SpdmRxQueueSize,    get_core_from_task(TaskId::Spdm)},
    {   QueueId::SpdmCryptoHelper,
              SpdmCryptoHelperMaxItems,                          SpdmCryptoHelperQueueSize,
              get_core_from_task(TaskId::Spdm)                                                                                          },
    {             QueueId::UsbHid,   1,                                                517,     get_core_from_task(TaskId::Usb)},
    {          QueueId::LstpToSpi,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {             QueueId::LstpTx,
              nv::lstp::LstpNumChannels,
              UsbLstpMsgSize,     get_core_from_task(TaskId::Usb)                                                                       },
    {        QueueId::LstpGpioIrq,
              LstpGpioIrqQueueDepth,                               LstpGpioIrqQueueSize,
              get_core_from_task(TaskId::Lstp)                                                                                          },
    {         QueueId::LstpToGpio,   1,                                     LstpToGpioSize,    get_core_from_task(TaskId::Lstp)},
    {          QueueId::UbridgeTx,   1,                                                  1, get_core_from_task(TaskId::Invalid)},
    {          QueueId::UbridgeRx,   1,                                                  1, get_core_from_task(TaskId::Invalid)}
};

constexpr uint32_t I3cQueueMaxTxSize = 1;

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
    Ap4Status,
    Ap5Status,
    Ap6Status,
    Ap7Status,
    Ap8Status,
    EepromUpdate,
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
    infos[idx++] = TimerInfo{TimerId::Ap1Status, get_core_from_task(TaskId::I2c1)};
    infos[idx++] = TimerInfo{TimerId::Ap2Status, get_core_from_task(TaskId::I2c2)};
    infos[idx++] = TimerInfo{TimerId::Ap3Status, get_core_from_task(TaskId::I2c3)};
    infos[idx++] = TimerInfo{TimerId::Ap4Status, get_core_from_task(TaskId::I2c4)};
    infos[idx++] = TimerInfo{TimerId::Ap5Status, get_core_from_task(TaskId::I2c5)};
    infos[idx++] = TimerInfo{TimerId::Ap6Status, get_core_from_task(TaskId::I2c6)};
    infos[idx++] = TimerInfo{TimerId::Ap7Status, get_core_from_task(TaskId::I2c7)};
    infos[idx++] = TimerInfo{TimerId::Ap8Status, get_core_from_task(TaskId::I2c8)};
    infos[idx++] = TimerInfo{TimerId::EepromUpdate, get_core_from_task(TaskId::Invalid)};

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

constexpr inline std::array<I2cTimerInfo, 0> I2cTimerInfos{};

// Limits
[[maybe_unused]] constexpr auto MaxQueuesPerTask = 4;
[[maybe_unused]] constexpr auto MaxEventsPerTask = 4;

constexpr uint8_t GpioNum = 8 + 11 + 10 + 18 + 11 + 8;
enum Port : gpio::GpioPort
{
    GlobalWpPort = nv::gpio::InvalidGpioPort,
    ApGoodPort   = nv::gpio::InvalidGpioPort,
    // Port 0
    MCU_RECOV_N_PORT      = 0,
    MCU_SSD0_PERST_L_PORT = 0,
    MCU_SSD1_PERST_L_PORT = 0,
    MCU_SSD4_PERST_L_PORT = 0,
    MCU_SSD2_PERST_L_PORT = 0,
    MCU_SSD5_PERST_L_PORT = 0,
    MCU_SSD3_PERST_L_PORT = 0,
    MCU_SSD6_PERST_L_PORT = 0,
    // Port 1
    MCU_SSD7_PERST_L_PORT  = 1,
    VPP_CPU1B_ALERT_L_PORT = 1,
    MCU_CLK_SSD0_EN_N_PORT = 1,
    VPP_CPU1_ALERT_L_PORT  = 1,
    SSD67_PWRBRK_L_PORT    = 1,
    MCU_CLK_SSD1_EN_N_PORT = 1,
    SSD6_PWRDIS_PORT       = 1,
    SSD0_PRSNT_L_PORT      = 1,
    SSD1_PRSNT_L_PORT      = 1,
    MCU_V_SSD5_DIS_PORT    = 1,
    MCU_V_SSD6_DIS_PORT    = 1,
    // Port 2
    SSD4_PRSNT_L_PORT      = 2,
    SSD5_PRSNT_L_PORT      = 2,
    SSD6_PRSNT_L_PORT      = 2,
    MCU_CLK_SSD7_EN_N_PORT = 2,
    MCU_CLK_SSD5_EN_N_PORT = 2,
    MCU_CLK_SSD4_EN_N_PORT = 2,
    SSD4_LED_PORT          = 2,
    SSD5_LED_PORT          = 2,
    SSD6_LED_PORT          = 2,
    SSD7_LED_PORT          = 2,
    // Port 3
    SSD0_PWRDIS_PORT       = 3,
    SSD1_PWRDIS_PORT       = 3,
    SSD2_PWRDIS_PORT       = 3,
    SSD45_PWRBRK_PORT      = 3,
    SSD3_PWRDIS_PORT       = 3,
    MCU_CLK_SSD2_EN_N_PORT = 3,
    SSD5_PWRDIS_PORT       = 3,
    SSD3_LED_PORT          = 3,
    MCU_CLK_SSD3_EN_N_PORT = 3,
    SSD7_PWRDIS_PORT       = 3,
    SSD01_PWRBRK_PORT      = 3,
    VPP_CPU0_ALERT_L_PORT  = 3,
    SSD0_LED_PORT          = 3,
    SSD4_PWRDIS_PORT       = 3,
    SSD1_LED_PORT          = 3,
    SSD23_PWRBRK_PORT      = 3,
    VPP_CPU0B_ALERT_L_PORT = 3,
    SSD2_LED_PORT          = 3,
    // Port 4
    SSD7_PRSNT_L_PORT      = 4,
    MCU_CLK_SSD6_EN_N_PORT = 4,
    MCU_V_SSD3_DIS_PORT    = 4,
    PCIE_SSD5_PERST_L_PORT = 4,
    PCIE_SSD6_PERST_L_PORT = 4,
    PCIE_SSD7_PERST_L_PORT = 4,
    MCU_V_SSD2_DIS_PORT    = 4,
    MCU_V_SSD4_DIS_PORT    = 4,
    SSD2_PRSNT_L_PORT      = 4,
    SSD3_PRSNT_L_PORT      = 4,
    MCU_V_SSD7_DIS_PORT    = 4,
    // Port 5
    PCIE_SSD0_PERST_L_PORT = 5,
    PCIE_SSD1_PERST_L_PORT = 5,
    PCIE_SSD4_PERST_L_PORT = 5,
    PCIE_SSD3_PERST_L_PORT = 5,
    PCIE_SSD2_PERST_L_PORT = 5,
    MCU_CLKSEL_MODE_PORT   = 5,
    MCU_V_SSD0_DIS_PORT    = 5,
    MCU_V_SSD1_DIS_PORT    = 5
};

enum Pin : gpio::GpioPin
{
    GlobalWpPin = nv::gpio::InvalidGpioPin,
    ApGoodPin   = nv::gpio::InvalidGpioPin,
    // Port 0 Pins
    MCU_RECOV_N_PIN      = 6,
    MCU_SSD0_PERST_L_PIN = 14,
    MCU_SSD1_PERST_L_PIN = 15,
    MCU_SSD4_PERST_L_PIN = 18,
    MCU_SSD2_PERST_L_PIN = 19,
    MCU_SSD5_PERST_L_PIN = 23,
    MCU_SSD3_PERST_L_PIN = 28,
    MCU_SSD6_PERST_L_PIN = 29,
    // Port 1 Pins
    MCU_SSD7_PERST_L_PIN  = 2,
    VPP_CPU1B_ALERT_L_PIN = 6,
    MCU_CLK_SSD0_EN_N_PIN = 7,
    VPP_CPU1_ALERT_L_PIN  = 10,
    SSD67_PWRBRK_L_PIN    = 11,
    MCU_CLK_SSD1_EN_N_PIN = 14,
    SSD6_PWRDIS_PIN       = 15,
    SSD0_PRSNT_L_PIN      = 16,
    SSD1_PRSNT_L_PIN      = 17,
    MCU_V_SSD5_DIS_PIN    = 18,
    MCU_V_SSD6_DIS_PIN    = 19,
    // Port 2 Pins
    SSD4_PRSNT_L_PIN      = 0,
    SSD5_PRSNT_L_PIN      = 1,
    SSD6_PRSNT_L_PIN      = 2,
    MCU_CLK_SSD7_EN_N_PIN = 5,
    MCU_CLK_SSD5_EN_N_PIN = 6,
    MCU_CLK_SSD4_EN_N_PIN = 7,
    SSD4_LED_PIN          = 8,
    SSD5_LED_PIN          = 9,
    SSD6_LED_PIN          = 10,
    SSD7_LED_PIN          = 11,
    // Port 3 Pins
    SSD0_PWRDIS_PIN       = 0,
    SSD1_PWRDIS_PIN       = 1,
    SSD2_PWRDIS_PIN       = 4,
    SSD45_PWRBRK_PIN      = 5,
    SSD3_PWRDIS_PIN       = 6,
    MCU_CLK_SSD2_EN_N_PIN = 7,
    SSD5_PWRDIS_PIN       = 8,
    SSD3_LED_PIN          = 9,
    MCU_CLK_SSD3_EN_N_PIN = 10,
    SSD7_PWRDIS_PIN       = 11,
    SSD01_PWRBRK_PIN      = 12,
    VPP_CPU0_ALERT_L_PIN  = 13,
    SSD0_LED_PIN          = 16,
    SSD4_PWRDIS_PIN       = 17,
    SSD1_LED_PIN          = 18,
    SSD23_PWRBRK_PIN      = 19,
    VPP_CPU0B_ALERT_L_PIN = 22,
    SSD2_LED_PIN          = 23,
    // Port 4 Pins
    SSD7_PRSNT_L_PIN      = 0,
    MCU_CLK_SSD6_EN_N_PIN = 1,
    MCU_V_SSD3_DIS_PIN    = 3,
    PCIE_SSD5_PERST_L_PIN = 14,
    PCIE_SSD6_PERST_L_PIN = 16,
    PCIE_SSD7_PERST_L_PIN = 17,
    MCU_V_SSD2_DIS_PIN    = 18,
    MCU_V_SSD4_DIS_PIN    = 20,
    SSD2_PRSNT_L_PIN      = 21,
    SSD3_PRSNT_L_PIN      = 22,
    MCU_V_SSD7_DIS_PIN    = 23,
    // Port 5 Pins
    PCIE_SSD0_PERST_L_PIN = 0,
    PCIE_SSD1_PERST_L_PIN = 1,
    PCIE_SSD4_PERST_L_PIN = 2,
    PCIE_SSD3_PERST_L_PIN = 3,
    PCIE_SSD2_PERST_L_PIN = 4,
    MCU_CLKSEL_MODE_PIN   = 5,
    MCU_V_SSD0_DIS_PIN    = 6,
    MCU_V_SSD1_DIS_PIN    = 7
};

using Gpios = std::tuple<gpio::GpioPort, gpio::GpioPin>;

constexpr inline std::array<Gpios, GpioNum> GpioSetup{
    // Port 0
    Gpios{      MCU_RECOV_N_PORT,       MCU_RECOV_N_PIN}, // 0
    Gpios{ MCU_SSD0_PERST_L_PORT,  MCU_SSD0_PERST_L_PIN}, // 1
    Gpios{ MCU_SSD1_PERST_L_PORT,  MCU_SSD1_PERST_L_PIN}, // 2
    Gpios{ MCU_SSD4_PERST_L_PORT,  MCU_SSD4_PERST_L_PIN}, // 3
    Gpios{ MCU_SSD2_PERST_L_PORT,  MCU_SSD2_PERST_L_PIN}, // 4
    Gpios{ MCU_SSD5_PERST_L_PORT,  MCU_SSD5_PERST_L_PIN}, // 5
    Gpios{ MCU_SSD3_PERST_L_PORT,  MCU_SSD3_PERST_L_PIN}, // 6
    Gpios{ MCU_SSD6_PERST_L_PORT,  MCU_SSD6_PERST_L_PIN}, // 7
    // Port 1
    Gpios{ MCU_SSD7_PERST_L_PORT,  MCU_SSD7_PERST_L_PIN}, // 8
    Gpios{VPP_CPU1B_ALERT_L_PORT, VPP_CPU1B_ALERT_L_PIN}, // 9
    Gpios{MCU_CLK_SSD0_EN_N_PORT, MCU_CLK_SSD0_EN_N_PIN}, // 10
    Gpios{ VPP_CPU1_ALERT_L_PORT,  VPP_CPU1_ALERT_L_PIN}, // 11
    Gpios{   SSD67_PWRBRK_L_PORT,    SSD67_PWRBRK_L_PIN}, // 12
    Gpios{MCU_CLK_SSD1_EN_N_PORT, MCU_CLK_SSD1_EN_N_PIN}, // 13
    Gpios{      SSD6_PWRDIS_PORT,       SSD6_PWRDIS_PIN}, // 14
    Gpios{     SSD0_PRSNT_L_PORT,      SSD0_PRSNT_L_PIN}, // 15
    Gpios{     SSD1_PRSNT_L_PORT,      SSD1_PRSNT_L_PIN}, // 16
    Gpios{   MCU_V_SSD5_DIS_PORT,    MCU_V_SSD5_DIS_PIN}, // 17
    Gpios{   MCU_V_SSD6_DIS_PORT,    MCU_V_SSD6_DIS_PIN}, // 18
    // Port 2
    Gpios{     SSD4_PRSNT_L_PORT,      SSD4_PRSNT_L_PIN}, // 19
    Gpios{     SSD5_PRSNT_L_PORT,      SSD5_PRSNT_L_PIN}, // 20
    Gpios{     SSD6_PRSNT_L_PORT,      SSD6_PRSNT_L_PIN}, // 21
    Gpios{MCU_CLK_SSD7_EN_N_PORT, MCU_CLK_SSD7_EN_N_PIN}, // 22
    Gpios{MCU_CLK_SSD5_EN_N_PORT, MCU_CLK_SSD5_EN_N_PIN}, // 23
    Gpios{MCU_CLK_SSD4_EN_N_PORT, MCU_CLK_SSD4_EN_N_PIN}, // 24
    Gpios{         SSD4_LED_PORT,          SSD4_LED_PIN}, // 25
    Gpios{         SSD5_LED_PORT,          SSD5_LED_PIN}, // 26
    Gpios{         SSD6_LED_PORT,          SSD6_LED_PIN}, // 27
    Gpios{         SSD7_LED_PORT,          SSD7_LED_PIN}, // 28
    // Port 3
    Gpios{      SSD0_PWRDIS_PORT,       SSD0_PWRDIS_PIN}, // 29
    Gpios{      SSD1_PWRDIS_PORT,       SSD1_PWRDIS_PIN}, // 30
    Gpios{      SSD2_PWRDIS_PORT,       SSD2_PWRDIS_PIN}, // 31
    Gpios{     SSD45_PWRBRK_PORT,      SSD45_PWRBRK_PIN}, // 32
    Gpios{      SSD3_PWRDIS_PORT,       SSD3_PWRDIS_PIN}, // 33
    Gpios{MCU_CLK_SSD2_EN_N_PORT, MCU_CLK_SSD2_EN_N_PIN}, // 34
    Gpios{      SSD5_PWRDIS_PORT,       SSD5_PWRDIS_PIN}, // 35
    Gpios{         SSD3_LED_PORT,          SSD3_LED_PIN}, // 36
    Gpios{MCU_CLK_SSD3_EN_N_PORT, MCU_CLK_SSD3_EN_N_PIN}, // 37
    Gpios{      SSD7_PWRDIS_PORT,       SSD7_PWRDIS_PIN}, // 38
    Gpios{     SSD01_PWRBRK_PORT,      SSD01_PWRBRK_PIN}, // 39
    Gpios{ VPP_CPU0_ALERT_L_PORT,  VPP_CPU0_ALERT_L_PIN}, // 40
    Gpios{         SSD0_LED_PORT,          SSD0_LED_PIN}, // 41
    Gpios{      SSD4_PWRDIS_PORT,       SSD4_PWRDIS_PIN}, // 42
    Gpios{         SSD1_LED_PORT,          SSD1_LED_PIN}, // 43
    Gpios{     SSD23_PWRBRK_PORT,      SSD23_PWRBRK_PIN}, // 44
    Gpios{VPP_CPU0B_ALERT_L_PORT, VPP_CPU0B_ALERT_L_PIN}, // 45
    Gpios{         SSD2_LED_PORT,          SSD2_LED_PIN}, // 46
    // Port 4
    Gpios{     SSD7_PRSNT_L_PORT,      SSD7_PRSNT_L_PIN}, // 47
    Gpios{MCU_CLK_SSD6_EN_N_PORT, MCU_CLK_SSD6_EN_N_PIN}, // 48
    Gpios{   MCU_V_SSD3_DIS_PORT,    MCU_V_SSD3_DIS_PIN}, // 49
    Gpios{PCIE_SSD5_PERST_L_PORT, PCIE_SSD5_PERST_L_PIN}, // 50
    Gpios{PCIE_SSD6_PERST_L_PORT, PCIE_SSD6_PERST_L_PIN}, // 51
    Gpios{PCIE_SSD7_PERST_L_PORT, PCIE_SSD7_PERST_L_PIN}, // 52
    Gpios{   MCU_V_SSD2_DIS_PORT,    MCU_V_SSD2_DIS_PIN}, // 53
    Gpios{   MCU_V_SSD4_DIS_PORT,    MCU_V_SSD4_DIS_PIN}, // 54
    Gpios{     SSD2_PRSNT_L_PORT,      SSD2_PRSNT_L_PIN}, // 55
    Gpios{     SSD3_PRSNT_L_PORT,      SSD3_PRSNT_L_PIN}, // 56
    Gpios{   MCU_V_SSD7_DIS_PORT,    MCU_V_SSD7_DIS_PIN}, // 57
    // Port 5
    Gpios{PCIE_SSD0_PERST_L_PORT, PCIE_SSD0_PERST_L_PIN}, // 58
    Gpios{PCIE_SSD1_PERST_L_PORT, PCIE_SSD1_PERST_L_PIN}, // 59
    Gpios{PCIE_SSD4_PERST_L_PORT, PCIE_SSD4_PERST_L_PIN}, // 60
    Gpios{PCIE_SSD3_PERST_L_PORT, PCIE_SSD3_PERST_L_PIN}, // 61
    Gpios{PCIE_SSD2_PERST_L_PORT, PCIE_SSD2_PERST_L_PIN}, // 62
    Gpios{  MCU_CLKSEL_MODE_PORT,   MCU_CLKSEL_MODE_PIN}, // 63
    Gpios{   MCU_V_SSD0_DIS_PORT,    MCU_V_SSD0_DIS_PIN}, // 64
    Gpios{   MCU_V_SSD1_DIS_PORT,    MCU_V_SSD1_DIS_PIN}  // 65
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

constexpr int GpioInterruptNum = 0;

constexpr inline std::array<GpioInterruptConfig, GpioInterruptNum> GpioInterruptSetup{};

constexpr uint32_t CtimerFrequency = 48000000;

enum BootedEventBits : uint32_t
{
    Mctp  = nv::common::bit(0),
    Pldm  = nv::common::bit(1),
    Usb   = nv::common::bit(2),
    Flash = nv::common::bit(3),
#ifdef NV_UNITTEST
    Unittest,
#endif
    Logger = nv::common::bit(4),
    Spdm   = nv::common::bit(5),
    Nhp    = nv::common::bit(6),
    I2c1   = nv::common::bit(7),
    I2c2   = nv::common::bit(8),
    I2c4   = nv::common::bit(9),
    I2c5   = nv::common::bit(10),
    I2c6   = nv::common::bit(11),
    I2c7   = nv::common::bit(12),
    I2c8   = nv::common::bit(13),
    Lstp   = nv::common::bit(14),
#if defined(NV_UART_INSTANCE) && (NV_UART_INSTANCE == 2)
    BootStatusMask = (nv::common::bit(15) - 1),
#else
    I2c3           = nv::common::bit(15),
    BootStatusMask = (nv::common::bit(16) - 1),
#endif
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

constexpr std::array<nv::watchdog::TaskMonitorIndex, 4> TaskMonitorList{
    nv::watchdog::TaskMonitorIndex::Flash,
    nv::watchdog::TaskMonitorIndex::Logger,
    nv::watchdog::TaskMonitorIndex::Usb,
    nv::watchdog::TaskMonitorIndex::Pldm,
};

constexpr bool EnableRuntimeWdt       = false;
constexpr bool EnableCP2112NativeGpio = false;

// Debugtoken config
constexpr bool DebugTokenEnabled = false;

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
        std::array<uint8_t, 0x7f + 1> count_table{};
        count_table.fill(0x00);
        for (const auto& MappingItem : I2cAddrMappingList) {
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
constexpr uint32_t SmbSensorUpdateMs   = 0;
constexpr uint32_t I3cSensorUpdateMs   = 0;
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

constexpr bool EnableForwardNvlInfo = false;

// SMA_READY pin configuration
constexpr bool               EnableMcuReadyPin = false;
constexpr nv::gpio::GpioPort McuReadyPinPort   = 0;
constexpr nv::gpio::GpioPin  McuReadyPinPin    = 0;

enum AdcPeripheral : uint32_t
{
    MCU_VMON_12V_SSD0_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD1_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD2_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD3_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD4_DIV_PERIPH = 1,
    MCU_VMON_12V_SSD5_DIV_PERIPH = 1,
    MCU_VMON_12V_SSD6_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD7_DIV_PERIPH = 0
};

enum AdcCommand : uint32_t
{
    MCU_VMON_12V_SSD0_DIV_CMD = 1,
    MCU_VMON_12V_SSD1_DIV_CMD = 2,
    MCU_VMON_12V_SSD2_DIV_CMD = 3,
    MCU_VMON_12V_SSD3_DIV_CMD = 4,
    MCU_VMON_12V_SSD4_DIV_CMD = 5,
    MCU_VMON_12V_SSD5_DIV_CMD = 6,
    MCU_VMON_12V_SSD6_DIV_CMD = 7,
    MCU_VMON_12V_SSD7_DIV_CMD = 8
};

// I2C port mapping for AHS
// NOTE: Names (e.g., SSD0) match the schematic labels.
// Physical mapping of SSD0 drive pairs are swapped from how they are labeled on the schematic.

constexpr nv::i2c::Port VPP_CPU0_I2C_PORT  = nv::i2c::Port::Six;
constexpr nv::i2c::Port VPP_CPU0B_I2C_PORT = nv::i2c::Port::End;
constexpr nv::i2c::Port VPP_CPU1_I2C_PORT  = nv::i2c::Port::Nine;
constexpr nv::i2c::Port VPP_CPU1B_I2C_PORT = nv::i2c::Port::End;
constexpr nv::i2c::Port SMB_SSD0_I2C_PORT  = nv::i2c::Port::Seven;
constexpr nv::i2c::Port SMB_SSD1_I2C_PORT  = nv::i2c::Port::One;
constexpr nv::i2c::Port SMB_SSD2_I2C_PORT  = nv::i2c::Port::Two;
constexpr nv::i2c::Port SMB_SSD3_I2C_PORT  = nv::i2c::Port::Four;
constexpr nv::i2c::Port SMB_SSD4_I2C_PORT  = nv::i2c::Port::Five;
constexpr nv::i2c::Port SMB_SSD5_I2C_PORT  = nv::i2c::Port::Eight;
constexpr nv::i2c::Port SMB_SSD6_I2C_PORT  = nv::i2c::Port::Three;
constexpr nv::i2c::Port SMB_SSD7_I2C_PORT  = nv::i2c::Port::Zero;

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
        {SSD0_PRSNT_L_PORT,SSD0_PRSNT_L_PIN,{{"SSD0_PRSNT_L"},LstpGpioDirection::Input,LstpGpioState::Low,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0}},
        {SSD1_PRSNT_L_PORT,SSD1_PRSNT_L_PIN,{{"SSD1_PRSNT_L"},LstpGpioDirection::Input,LstpGpioState::Low,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0}},
        {SSD2_PRSNT_L_PORT,SSD2_PRSNT_L_PIN,{{"SSD2_PRSNT_L"},LstpGpioDirection::Input,LstpGpioState::Low,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0}},
        {SSD3_PRSNT_L_PORT,SSD3_PRSNT_L_PIN,{{"SSD3_PRSNT_L"},LstpGpioDirection::Input,LstpGpioState::Low,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0}},
        {SSD4_PRSNT_L_PORT,SSD4_PRSNT_L_PIN,{{"SSD4_PRSNT_L"},LstpGpioDirection::Input,LstpGpioState::Low,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0}},
        {SSD5_PRSNT_L_PORT,SSD5_PRSNT_L_PIN,{{"SSD5_PRSNT_L"},LstpGpioDirection::Input,LstpGpioState::Low,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0}},
        {SSD6_PRSNT_L_PORT,SSD6_PRSNT_L_PIN,{{"SSD6_PRSNT_L"},LstpGpioDirection::Input,LstpGpioState::Low,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0}},
        {SSD7_PRSNT_L_PORT,SSD7_PRSNT_L_PIN,{{"SSD7_PRSNT_L"},LstpGpioDirection::Input,LstpGpioState::Low,LstpGpioOutputDriveConfig::OpenDrain,false,LstpGpioBiasPullConfig::PullUp,0,10,100,100,0,0,0}},
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

// Temperature Sensors Config for VR products
constexpr uint8_t I2cTempSensorSize = 0;

// This data is only shared between main.cpp and mctp task. Should not impact when migrating to
// dual core.
NV_SHARED_DATA inline std::array<nv::i2c::I2cTempSensorConfig, I2cTempSensorSize>
    I2cTempSensorList{};

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

}  // namespace nv::mctp

namespace nv::vrot {
constexpr inline std::array<ApInfo, 0> ApList = {{}};
}  // namespace nv::vrot

namespace nv::nhp {

// -------------------------------------
// Hotplug Configuration - Number of Instances
// Overide on the build command line using:
// ./ubs .... GD_NUM_HOTPLUG_INSTANCES=[1|2|4]
// Uncomment one (and only one) of the following lines
#ifndef NUM_HOTPLUG_INSTANCES
// Valid/Supported Values are 1, 2, and 4
// #define NUM_HOTPLUG_INSTANCES 1
#define NUM_HOTPLUG_INSTANCES 2
// #define NUM_HOTPLUG_INSTANCES 4
#endif
constexpr static uint8_t NumNhpInstances = static_cast<uint8_t>(NUM_HOTPLUG_INSTANCES);

// -------------------------------------
// Hotplug Configuration - Number of Drives (per instance)
// Overide on the build command line using:
// ./ubs .... GD_NUM_E1S_DRIVES=[2|4|8]
#ifndef NUM_E1S_DRIVES
#define NUM_E1S_DRIVES (8U / NumNhpInstances)
// #define NUM_E1S_DRIVES 2U
#endif
constexpr static uint8_t NumE1sDrives = static_cast<uint8_t>(NUM_E1S_DRIVES);

// -------------------------------------
// Hotplug Configuration - pgood threshold
// pgood value 0x0000 - 0xFFFF (represents 0 - 3.3V)
// pgood is resistor divided from 12V to 3.3V
constexpr static uint16_t PgoodThreshold = 51494U;  // 10.8V divided to 2.593V

// -------------------------------------
// Hotplug Configuration - Power Sequencing Delays
// -----------------------------------------------------
// needed for stabilizing clock before turning off perst
constexpr static uint32_t ClkEnToPerstDelayUs = 400;  // 400us per LMKDB1204REXT datasheet
// needed to know if pgood being down is fault or not

}  // namespace nv::nhp

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
