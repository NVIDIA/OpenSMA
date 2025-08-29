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
    TelemIndexMap{ nv::telemetry::TelemId::InternalTemp,  0},
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
    NHP,
    I2c0,
#ifdef NV_UNITTEST
    Unittest,
#endif
    Pldm,
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
    Nhp,
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
    End
};

// Down stream Information
constexpr uint8_t I2cUpStreamNum          = 1;
constexpr uint8_t DownStreamNum           = 0;
constexpr uint8_t UpStreamNum             = 2;
constexpr uint8_t DefaultRoutingTableSize = DownStreamNum + UpStreamNum;
constexpr uint8_t RoutingInfoUpdateSize   = 0;
constexpr uint8_t RoutingTableSize        = DefaultRoutingTableSize + RoutingInfoUpdateSize;

constexpr inline std::array<mctp::DownStreamInfo, DownStreamNum> DownStreamInfos{};

constexpr uint32_t SpdmRequestQueueSize      = 2048;
constexpr uint32_t SpdmRxQueueSize           = 72;
constexpr uint32_t SpdmCryptoHelperQueueSize = 12;
constexpr bool     SpdmI2cResponder          = true;
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
    {               QueueId::I2c1,   1,                                                  1},
    {               QueueId::I2c2,   1,                                                  1},
    {               QueueId::I2c3,   1,                                                  1},
    {               QueueId::I2c4,   1,                                                  1},
    {               QueueId::I2c5,   1,                                                  1},
    {               QueueId::I3c0,   1,                                                  1},
    {               QueueId::I3c1,   1,                                                  1},
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

constexpr uint8_t GpioNum = 7 + 11 + 12 + 18 + 9 + 7 + 1;
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
    MCU_SSD3_PERST_L_PORT = 0,
    MCU_SSD6_PERST_L_PORT = 0,
    MCU_SSD5_PERST_L_PORT = 0,
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
    SSD7_PRSNT_L_PORT      = 2,
    MCU_CLK_SSD6_EN_N_PORT = 2,
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
    SSD3_PWRDIS_PORT       = 3,
    MCU_CLK_SSD2_EN_N_PORT = 3,
    SSD5_PWRDIS_PORT       = 3,
    SSD3_LED_PORT          = 3,
    MCU_CLK_SSD3_EN_N_PORT = 3,
    SSD7_PWRDIS_PORT       = 3,
    SSD01_PWRBRK_PORT      = 3,
    VPP_CPU0_ALERT_L_PORT  = 3,
    SSD23_PWRBRK_PORT      = 3,
    SSD45_PWRBRK_PORT      = 3,
    SSD0_LED_PORT          = 3,
    SSD4_PWRDIS_PORT       = 3,
    SSD1_LED_PORT          = 3,
    VPP_CPU0B_ALERT_L_PORT = 3,
    SSD2_LED_PORT          = 3,
    // Port 4
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
    MCU_SSD3_PERST_L_PIN = 20,
    MCU_SSD6_PERST_L_PIN = 21,
    MCU_SSD5_PERST_L_PIN = 23,
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
    SSD7_PRSNT_L_PIN      = 3,
    MCU_CLK_SSD6_EN_N_PIN = 4,
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
    SSD2_PWRDIS_PIN       = 2,
    SSD3_PWRDIS_PIN       = 6,
    MCU_CLK_SSD2_EN_N_PIN = 7,
    SSD5_PWRDIS_PIN       = 8,
    SSD3_LED_PIN          = 9,
    MCU_CLK_SSD3_EN_N_PIN = 10,
    SSD7_PWRDIS_PIN       = 11,
    SSD01_PWRBRK_PIN      = 12,
    VPP_CPU0_ALERT_L_PIN  = 13,
    SSD23_PWRBRK_PIN      = 14,
    SSD45_PWRBRK_PIN      = 15,
    SSD0_LED_PIN          = 16,
    SSD4_PWRDIS_PIN       = 17,
    SSD1_LED_PIN          = 18,
    VPP_CPU0B_ALERT_L_PIN = 22,
    SSD2_LED_PIN          = 23,
    // Port 4 Pins
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
    MCU_V_SSD0_DIS_PIN    = 6,
    MCU_V_SSD1_DIS_PIN    = 7
};

using Gpios = std::tuple<gpio::GpioPort, gpio::GpioPin>;

constexpr inline std::array<Gpios, GpioNum> GpioSetup{
    // Port 0
    Gpios{      MCU_RECOV_N_PORT,       MCU_RECOV_N_PIN},
    Gpios{ MCU_SSD0_PERST_L_PORT,  MCU_SSD0_PERST_L_PIN},
    Gpios{ MCU_SSD1_PERST_L_PORT,  MCU_SSD1_PERST_L_PIN},
    Gpios{ MCU_SSD4_PERST_L_PORT,  MCU_SSD4_PERST_L_PIN},
    Gpios{ MCU_SSD2_PERST_L_PORT,  MCU_SSD2_PERST_L_PIN},
    Gpios{ MCU_SSD3_PERST_L_PORT,  MCU_SSD3_PERST_L_PIN},
    Gpios{ MCU_SSD6_PERST_L_PORT,  MCU_SSD6_PERST_L_PIN},
    // Port 1
    Gpios{ MCU_SSD7_PERST_L_PORT,  MCU_SSD7_PERST_L_PIN},
    Gpios{VPP_CPU1B_ALERT_L_PORT, VPP_CPU1B_ALERT_L_PIN},
    Gpios{MCU_CLK_SSD0_EN_N_PORT, MCU_CLK_SSD0_EN_N_PIN},
    Gpios{ VPP_CPU1_ALERT_L_PORT,  VPP_CPU1_ALERT_L_PIN},
    Gpios{   SSD67_PWRBRK_L_PORT,    SSD67_PWRBRK_L_PIN},
    Gpios{MCU_CLK_SSD1_EN_N_PORT, MCU_CLK_SSD1_EN_N_PIN},
    Gpios{      SSD6_PWRDIS_PORT,       SSD6_PWRDIS_PIN},
    Gpios{     SSD0_PRSNT_L_PORT,      SSD0_PRSNT_L_PIN},
    Gpios{     SSD1_PRSNT_L_PORT,      SSD1_PRSNT_L_PIN},
    Gpios{   MCU_V_SSD5_DIS_PORT,    MCU_V_SSD5_DIS_PIN},
    Gpios{   MCU_V_SSD6_DIS_PORT,    MCU_V_SSD6_DIS_PIN},
    // Port 2
    Gpios{     SSD4_PRSNT_L_PORT,      SSD4_PRSNT_L_PIN},
    Gpios{     SSD5_PRSNT_L_PORT,      SSD5_PRSNT_L_PIN},
    Gpios{     SSD6_PRSNT_L_PORT,      SSD6_PRSNT_L_PIN},
    Gpios{     SSD7_PRSNT_L_PORT,      SSD7_PRSNT_L_PIN},
    Gpios{MCU_CLK_SSD6_EN_N_PORT, MCU_CLK_SSD6_EN_N_PIN},
    Gpios{MCU_CLK_SSD7_EN_N_PORT, MCU_CLK_SSD7_EN_N_PIN},
    Gpios{MCU_CLK_SSD5_EN_N_PORT, MCU_CLK_SSD5_EN_N_PIN},
    Gpios{MCU_CLK_SSD4_EN_N_PORT, MCU_CLK_SSD4_EN_N_PIN},
    Gpios{         SSD4_LED_PORT,          SSD4_LED_PIN},
    Gpios{         SSD5_LED_PORT,          SSD5_LED_PIN},
    Gpios{         SSD6_LED_PORT,          SSD6_LED_PIN},
    Gpios{         SSD7_LED_PORT,          SSD7_LED_PIN},
    // Port 3
    Gpios{      SSD0_PWRDIS_PORT,       SSD0_PWRDIS_PIN},
    Gpios{      SSD1_PWRDIS_PORT,       SSD1_PWRDIS_PIN},
    Gpios{      SSD2_PWRDIS_PORT,       SSD2_PWRDIS_PIN},
    Gpios{      SSD3_PWRDIS_PORT,       SSD3_PWRDIS_PIN},
    Gpios{MCU_CLK_SSD2_EN_N_PORT, MCU_CLK_SSD2_EN_N_PIN},
    Gpios{      SSD5_PWRDIS_PORT,       SSD5_PWRDIS_PIN},
    Gpios{         SSD3_LED_PORT,          SSD3_LED_PIN},
    Gpios{MCU_CLK_SSD3_EN_N_PORT, MCU_CLK_SSD3_EN_N_PIN},
    Gpios{      SSD7_PWRDIS_PORT,       SSD7_PWRDIS_PIN},
    Gpios{     SSD01_PWRBRK_PORT,      SSD01_PWRBRK_PIN},
    Gpios{ VPP_CPU0_ALERT_L_PORT,  VPP_CPU0_ALERT_L_PIN},
    Gpios{     SSD23_PWRBRK_PORT,      SSD23_PWRBRK_PIN},
    Gpios{     SSD45_PWRBRK_PORT,      SSD45_PWRBRK_PIN},
    Gpios{         SSD0_LED_PORT,          SSD0_LED_PIN},
    Gpios{      SSD4_PWRDIS_PORT,       SSD4_PWRDIS_PIN},
    Gpios{         SSD1_LED_PORT,          SSD1_LED_PIN},
    Gpios{VPP_CPU0B_ALERT_L_PORT, VPP_CPU0B_ALERT_L_PIN},
    Gpios{         SSD2_LED_PORT,          SSD2_LED_PIN},
    // Port 4
    Gpios{   MCU_V_SSD3_DIS_PORT,    MCU_V_SSD3_DIS_PIN},
    Gpios{PCIE_SSD5_PERST_L_PORT, PCIE_SSD5_PERST_L_PIN},
    Gpios{PCIE_SSD6_PERST_L_PORT, PCIE_SSD6_PERST_L_PIN},
    Gpios{PCIE_SSD7_PERST_L_PORT, PCIE_SSD7_PERST_L_PIN},
    Gpios{   MCU_V_SSD2_DIS_PORT,    MCU_V_SSD2_DIS_PIN},
    Gpios{   MCU_V_SSD4_DIS_PORT,    MCU_V_SSD4_DIS_PIN},
    Gpios{     SSD2_PRSNT_L_PORT,      SSD2_PRSNT_L_PIN},
    Gpios{     SSD3_PRSNT_L_PORT,      SSD3_PRSNT_L_PIN},
    Gpios{   MCU_V_SSD7_DIS_PORT,    MCU_V_SSD7_DIS_PIN},
    // Port 5
    Gpios{PCIE_SSD0_PERST_L_PORT, PCIE_SSD0_PERST_L_PIN},
    Gpios{PCIE_SSD1_PERST_L_PORT, PCIE_SSD1_PERST_L_PIN},
    Gpios{PCIE_SSD4_PERST_L_PORT, PCIE_SSD4_PERST_L_PIN},
    Gpios{PCIE_SSD3_PERST_L_PORT, PCIE_SSD3_PERST_L_PIN},
    Gpios{PCIE_SSD2_PERST_L_PORT, PCIE_SSD2_PERST_L_PIN},
    Gpios{   MCU_V_SSD0_DIS_PORT,    MCU_V_SSD0_DIS_PIN},
    Gpios{   MCU_V_SSD1_DIS_PORT,    MCU_V_SSD1_DIS_PIN}
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

constexpr auto UartInstance = 2;

enum BootedEventBits : uint32_t
{
    Mctp  = nv::common::bit(0),
    I2c0  = nv::common::bit(1),
    Usb   = nv::common::bit(2),
    Flash = nv::common::bit(3),
#ifdef NV_UNITTEST
    Unittest,
#endif
    Pldm           = nv::common::bit(4),
    Logger         = nv::common::bit(5),
    Spdm           = nv::common::bit(6),
    BootStatusMask = (nv::common::bit(7) - 1),
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

constexpr bool EnableRuntimeWdt = false;

// Debugtoken config
constexpr bool DebugTokenEnabled = false;

// FPGA I2C Re-Enumeration
constexpr bool EnableFpgaI2cReEnumeration = false;
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
constexpr bool EnableDelayInI3CInit = false;
// AP status ping interval for discovery notify when no GPIO is available
constexpr uint32_t CheckApStatusTimerUs = 0;
// Use the I2C AP status timer if I3C is not enabled
constexpr bool UseI2cApStatusTimer = false;

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

enum AdcPeripheral : uint32_t
{
    MCU_VMON_12V_SSD0_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD1_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD2_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD3_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD4_DIV_PERIPH = 0,
    MCU_VMON_12V_SSD5_DIV_PERIPH = 0,
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

enum I2cPort : uint8_t
{
    VPP_CPU0_I2C_PORT  = 6,
    VPP_CPU0B_I2C_PORT = 1,
    VPP_CPU1_I2C_PORT  = 4,
    VPP_CPU1B_I2C_PORT = 3
};

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

namespace nv::nhp {

// -------------------------------------
// Hotplug Configuration - Number of Instances
// Uncomment one (and only one) of the following lines
// Valid/Supported Values are 1, 2, and 4
// #define NUM_HOTPLUG_INSTANCES 1
#define NUM_HOTPLUG_INSTANCES 2
// #define NUM_HOTPLUG_INSTANCES 4
constexpr static uint8_t NumNhpInstances = static_cast<uint8_t>(NUM_HOTPLUG_INSTANCES);

// -------------------------------------
// Hotplug Configuration - Number of Drives (per instance)
constexpr static uint8_t NumE1sDrives = (8U / NumNhpInstances);

// -------------------------------------
// Hotplug Configuration - ADC Trigger Setup
// NOTE: This should match the configuration of triggers found in the board-specific
//       "*.mex" configuration file.
constexpr static uint8_t                       NumAdcTriggers = 2U;
constexpr std::array<uint32_t, NumAdcTriggers> AdcTriggers    = {(1U << 0U), (1U << 1U)};
constexpr static int                           MAX_ADC_POLL_ITERATIONS = 1000;

// -------------------------------------
// Hotplug Configuration - Mapping Version
// NHP Mapping Version - 3 bits
constexpr static uint16_t MappingVersion = 0b001;

// -------------------------------------
// Hotplug Configuration - pgood threshold
// pgood value 0x0000 - 0xFFFF (represents 0 - 3.3V)
// pgood is resistor divided from 12V to 3.3V
constexpr static uint16_t PgoodThreshold = 0xE8BAU;

// -------------------------------------
// Hotplug Configuration - Power Sequencing Delays
// -----------------------------------------------------
// needed since ssd main rail could come on later then MCU power
constexpr static uint32_t McuOnToPwrEnDelayUs = 0;  // Not needed
// needed for stabilizing ssd power before enabling clocks
constexpr static uint32_t PwrEnToClkEnDelayUs = 0;  // Not needed per LMKDB1204REXT
// needed for stabilizing clock before turning off perst
constexpr static uint32_t ClkEnToPerstDelayUs = 400;  // 400us per LMKDB1204REXT datasheet
// needed to know if pgood being down is fault or not
// setting to ClkEnToPerstDelayUs to avoid having to deinit ctimer
constexpr static uint32_t PwrEnToPgoodDelayUs = ClkEnToPerstDelayUs;

// -------------------------------------
// Hotplug Configuration - Calculate Number of Drives
constexpr uint8_t calculate_partition_register(uint16_t num_of_drives)
{
    switch (num_of_drives) {
        case 1U: {
            return 0b00;
        }
        case 2U: {
            return 0b01;
        }
        case 4U: {
            return 0b10;
        }
        case 8U: {
            return 0b11;
        }
        default: {
            return 0xffU;
        }
    }
}
// 2 bits max - 8/4/2/1 drives = 3/2/1/0
constexpr static uint16_t NumOfPartitions = calculate_partition_register(NumE1sDrives);

}  // namespace nv::nhp

#endif
