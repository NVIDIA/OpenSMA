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

#include "nv/flash/common.h"
#include "nv/gpio/common.h"
#include "nv/logger/common.h"
#include "nv/mctp/router.h"
#include "nv/watchdog/notify_interface.h"
#include "nv/i2c/port.h"
#include "nv/ipchandler/enums.h"
#include "nv/mctp/enums.h"
#include "nv/telemetry/utils.h"

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
    TelemIndexMap{ nv::telemetry::TelemId::InternalTemp, -1},
    TelemIndexMap{nv::telemetry::TelemId::MaxModuleTemp, -1},
    TelemIndexMap{         nv::telemetry::TelemId::Gpio, -1},
    TelemIndexMap{   nv::telemetry::TelemId::CX8_1_Temp, -1},
    TelemIndexMap{   nv::telemetry::TelemId::CX8_2_Temp, -1},
};

}  // namespace nv::telemetry

namespace nv::ipc {

/// All Tasks must be part of this enum.
enum class TaskId
{
    Begin,
    Mctp = Begin,
    I2c1,
    I2c2,
    Pldm,
#ifdef NV_UNITTEST
    Unittest,
#endif
    Logger,
    Spdm,
    Privileged = Spdm + 1,
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
    MctpUsI2c0Request = Begin,
    MctpUsUsbRequest,
    MctpDsI2c0Request,
    MctpDsI2c1Request,
    MctpDsI2c2Request,
    MctpDsI2c3Request,
    MctpDsI3c0Request,
    MctpDsI3c1Request,
    MctpSpi0Request,
    MctpSpi1Request,
    MctpSpi2Request,
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
    MctpFake,
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
    I3c1,
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
    End
};

// Down stream Information
constexpr uint8_t I2cDownStreamNum        = 2;
constexpr uint8_t I3cDownStreamNum        = 0;
constexpr uint8_t DownStreamNum           = I2cDownStreamNum + I3cDownStreamNum;
constexpr uint8_t UpStreamNum             = 1;
constexpr uint8_t DefaultRoutingTableSize = DownStreamNum + UpStreamNum;
constexpr uint8_t RoutingInfoUpdateSize   = 0;
constexpr uint8_t RoutingTableSize        = DefaultRoutingTableSize + RoutingInfoUpdateSize;

constexpr inline std::array<mctp::DownStreamInfo, DownStreamNum> DownStreamInfos{
    mctp::DownStreamInfo{mctp::PhyId::MctpOverSmbus,
                         mctp::PhyMediumId::Smbus30I2cFast,
                         mctp::Client::DsI2c0,
                         0x32                                                                                     },
    {mctp::PhyId::MctpOverSmbus, mctp::PhyMediumId::Smbus30I2cFast, mctp::Client::DsI2c1, 0x32},
};

constexpr uint32_t SpdmRequestQueueSize      = 2048;
constexpr uint32_t SpdmRxQueueSize           = 72;
constexpr uint32_t SpdmCryptoHelperQueueSize = 12;
constexpr bool     SpdmI2cResponder          = false;
constexpr bool     SpdmDummyCertificates     = true;
using QueueInfo                              = std::tuple<QueueId, uint8_t, uint16_t>;

/// define all queue lengths and item_sizes here
constexpr inline std::array<QueueInfo, int(QueueId::End)> QueueInfos{
    // id, len, item_size
    QueueInfo{  QueueId::MctpUsI2c0Request,1,                                                 72                                           },
    {   QueueId::MctpUsUsbRequest, 130,                                                 72},
    {  QueueId::MctpDsI2c0Request,   1,                                                 72},
    {  QueueId::MctpDsI2c1Request,   1,                                                 72},
    {  QueueId::MctpDsI2c2Request,   1,                                                 72},
    {  QueueId::MctpDsI2c3Request,   1,                                                 72},
    {  QueueId::MctpDsI3c0Request,   1,                                                 72},
    {  QueueId::MctpDsI3c1Request,   1,                                                 72},
    {    QueueId::MctpSpi0Request,   1,                                                  1},
    {    QueueId::MctpSpi1Request,   1,                                                  1},
    {    QueueId::MctpSpi2Request,   1,                                                  1},
    {    QueueId::MctpPldmRequest,   1,                                                256},
    {    QueueId::MctpSpdmRequest,   1,                               SpdmRequestQueueSize},
    {            QueueId::MctpCmd, 130,                                                  4},
    {               QueueId::I2c0,  64,                                                 80},
    {               QueueId::I2c1,  64,                                                 80},
    {               QueueId::I2c2,  64,                                                 80},
    {               QueueId::I2c3,   1,                                                  1},
    {               QueueId::I2c4,   1,                                                  1},
    {               QueueId::I2c5,   1,                                                  1},
    {               QueueId::I3c0,  64,                                                 72},
    {               QueueId::I3c1,  64,                                                 72},
    {               QueueId::Spi0,   1,                                                  1},
    {               QueueId::Spi1,   1,                                                  1},
    {               QueueId::Spi2,   1,                                                  1},
    {             QueueId::PldmRx,   1,                                                 72},
    {           QueueId::PldmRx4k,   1,                                          4096 + 32},
    {              QueueId::UsbTx,   1,                                                 72},
    {           QueueId::MctpFake,   1,                                                 16},
    {       QueueId::FlashRequest,   1,                         sizeof(nv::flash::Request)}, // payload size = 256 bytes, header
                                                             // size = 12 bytes
    {      QueueId::FlashResponse,   1,                        sizeof(nv::flash::Response)}, // payload size = 256 bytes,
                                                               // header size = 16 bytes
    {       QueueId::RoutingTable,
              RoutingTableSize, sizeof(mctp::ShardRoutingTable) * RoutingTableSize                 },
    {         QueueId::LogRequest,  64,                           sizeof(nv::logger::Item)},
    {QueueId::LogResponseBlocking,   1,                           sizeof(nv::logger::Item)},
    {        QueueId::LogDownload,   1,                          sizeof(nv::logger::Dlreq)},
    {    QueueId::LogDownloadResp,   1,                          sizeof(nv::logger::Dlreq)},
    {             QueueId::LogISR,   6,                           sizeof(nv::logger::Item)},
    {          QueueId::FlashSema,   1,                                                  1},
    {             QueueId::SpdmRx,   1,                                    SpdmRxQueueSize},
    {   QueueId::SpdmCryptoHelper,   1,                          SpdmCryptoHelperQueueSize},
    {             QueueId::UsbHid,   1,                                                 68}
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
    Ap3Status,
    End
};

// Limits
[[maybe_unused]] constexpr auto MaxQueuesPerTask = 4;
[[maybe_unused]] constexpr auto MaxEventsPerTask = 4;

constexpr uint8_t GpioNum = 6;

enum Port : gpio::GpioPort
{
    GlobalWpPort = nv::gpio::InvalidGpioPort,

    MCU_RECOV_N_PORT      = 0,
    S_MCU_MUX_SEL_PORT    = 0,
    S_BOARD_PG_PORT       = 0,
    CX8_A_BOOT_CMPLT_PORT = 2,
    CX8_B_BOOT_CMPLT_PORT = 2,
    MCU_GPIO_PORT         = 3,
};

enum Pin : gpio::GpioPin
{
    GlobalWpPin          = nv::gpio::InvalidGpioPin,
    MCU_RECOV_N_PIN      = 6,
    S_MCU_MUX_SEL_PIN    = 4,
    S_BOARD_PG_PIN       = 5,
    CX8_A_BOOT_CMPLT_PIN = 1,
    CX8_B_BOOT_CMPLT_PIN = 3,
    MCU_GPIO_PIN         = 10,
};

using Gpios = std::tuple<gpio::GpioPort, gpio::GpioPin>;

constexpr inline std::array<Gpios, GpioNum> GpioSetup{
    Gpios{     MCU_RECOV_N_PORT,      MCU_RECOV_N_PIN},
    Gpios{   S_MCU_MUX_SEL_PORT,    S_MCU_MUX_SEL_PIN},
    Gpios{      S_BOARD_PG_PORT,       S_BOARD_PG_PIN},
    Gpios{CX8_A_BOOT_CMPLT_PORT, CX8_A_BOOT_CMPLT_PIN},
    Gpios{CX8_B_BOOT_CMPLT_PORT, CX8_B_BOOT_CMPLT_PIN},
    Gpios{        MCU_GPIO_PORT,         MCU_GPIO_PIN},
};

// TODO: confirming Rising/Falling edge trigger for each interrupt
using GpioInterruptConfig = std::tuple<nv::gpio::GpioPort,
                                       nv::gpio::GpioPin,
                                       nv::gpio::InterruptDetection,
                                       nv::gpio::InterruptSelect>;

constexpr int GpioInterruptNum = 0;

constexpr inline std::array<GpioInterruptConfig, GpioInterruptNum> GpioInterruptSetup{};

constexpr uint32_t CtimerFrequency = 48000000;

constexpr auto UartInstance = 0xfe;

enum BootedEventBits : uint32_t
{
    Mctp  = nv::common::bit(0),
    I2c1  = nv::common::bit(1),
    I2c2  = nv::common::bit(2),
    Pldm  = nv::common::bit(3),
    Usb   = nv::common::bit(4),
    Flash = nv::common::bit(5),
#ifdef NV_UNITTEST
    Unittest,
#endif
    Logger         = nv::common::bit(6),
    Spdm           = nv::common::bit(7),
    BootStatusMask = (nv::common::bit(8) - 1),
};

constexpr uint32_t WatchdogResetMs       = 2000;
constexpr uint32_t CheckTaskBootStatusMs = 1000;

// USB config
constexpr uint16_t UsbDeviceVid = 0x0955U;
constexpr uint16_t UsbDevicePid = 0xCF11U;

// Runtime WDT
constexpr uint32_t RuntimeWatchdogResetMs = 2000;
constexpr uint32_t SupervisorCheckMs      = 500;
constexpr uint32_t SupervisorCheckUs      = SupervisorCheckMs * 1000;

constexpr std::array<nv::watchdog::TaskMonitorIndex, 6> TaskMonitorList{
    nv::watchdog::TaskMonitorIndex::Flash,
    nv::watchdog::TaskMonitorIndex::Logger,
    nv::watchdog::TaskMonitorIndex::Usb,
    nv::watchdog::TaskMonitorIndex::Pldm,
    nv::watchdog::TaskMonitorIndex::I2c1,
    nv::watchdog::TaskMonitorIndex::I2c2,
};

constexpr bool EnableRuntimeWdt = true;

// Debugtoken config
constexpr bool DebugTokenEnabled = true;

// FPGA I2C Re-Enumeration
constexpr bool EnableFpgaI2cReEnumeration = false;
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
using ModuleTempSensor = std::tuple<nv::i2c::Port, uint8_t, nv::telemetry::TelemId, Gpios>;
constexpr inline std::array<ModuleTempSensor, ModuleTempSensorSize> ModuleTempSensorList{};

// FPGA WAR: Bug ?
constexpr bool I2cIsEndpoint = false;

// For CX8 I3C init flow, CX8 need time to handle RSTDAA
constexpr bool EnableDelayInI3CInit = true;
// AP status ping interval for discovery notify when no GPIO is available
constexpr uint32_t CheckApStatusTimerUs = 5 * 1000 * 1000;
// Use the I2C AP status timer if I3C is not enabled
constexpr bool UseI2cApStatusTimer = true;

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

}  // namespace nv::mctp

#endif
