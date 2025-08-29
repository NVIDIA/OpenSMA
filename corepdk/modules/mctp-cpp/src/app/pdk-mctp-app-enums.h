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
#include "pdk-mctp-platforms-enums.h"

namespace pdk::mctp::app {

// Define base on
// https://www.dmtf.org/sites/default/files/standards/documents/DSP0239_1.6.0.pdf
// Only define part of PhyId & PhyMediumId compared to SPEC
enum class PhyId : uint8_t
{
    Reserved       = 0x00,
    MctpOverSmbus  = 0x01,
    MctpOverPcie   = 0x02,
    MctpOverUsb    = 0x03,
    MctpOverKcs    = 0x04,
    MctpOverSerial = 0x05,
    MctpOverI3c    = 0x06,
    VendorDefined  = 0xFF,
};

enum class PhyMediumId : uint8_t
{
    Unspecified = 0x00,

    // SMBus / I2C
    Smbus20_100kHz         = 0x01,  // SMBus 2.0 100 kHz
    Smbus20I2c100kHz       = 0x02,  // SMBus 2.0 + I2C 100 kHz
    I2cStandard            = 0x03,  // I2C 100 kHz (Standard-mode)
    Smbus30I2cFast         = 0x04,  // SMBus 3.0 or I2C 400 kHz (Fast-mode)
    Smbus30I2cFastModePlus = 0x05,  // SMBus 3.0 or I2C 1 MHz (Fast-mode Plus)
    I2cHighSpeed           = 0x06,  // I2C 3.4 MHz (High-speed mode)

    // PCIe / PCI
    Pcie11 = 0x08,
    Pcie20 = 0x09,
    Pcie21 = 0x0A,
    Pcie30 = 0x0B,
    Pcie40 = 0x0C,
    Pcie50 = 0x0D,

    PciCompatible = 0x0F,  // PCI 1.0 ~ PCI-X 2.0

    // USB
    Usb11 = 0x10,  // USB 1.1 compatible
    Usb20 = 0x11,  // USB 2.0 compatible
    Usb30 = 0x12,  // USB 3.0 compatible

    NcSiRbt = 0x18,  // NC-SI over RBT (RMII based)

    // KCS / Serial Host
    KcsLegacy        = 0x20,  // KCS/ Legacy (Fixed Address Decoding)
    KcsPci           = 0x21,  // KCS/ PCI (Base Class 0xC0 Subclass 0x01)
    SerialHostLegacy = 0x22,  // Serial Host/ Legacy (Fixed Address Decoding)
    SerialHostPci    = 0x23,  // Serial Host/ PCI (Base Class 0x07 Subclass 0x00)
    AsyncSerial      = 0x24,  // Asynchronous Serial (Between MCs and IMDs)

    // I3C
    I3c       = 0x30,  // I3C 12.5 MHz compatible (SDR)
    I3c_25MHz = 0x31,  // I3C 25 MHz compatible (HDR-DDR)

    VendorDefined = 0xFF
};

enum class Cmd : uint8_t
{
    SetEpId                 = 0x01,
    GetEpId                 = 0x02,
    GetEpUuid               = 0x03,
    GetMctpVerSupport       = 0x04,
    GetMsgTypeSupport       = 0x05,
    GetVndrMsgSupport       = 0x06,
    ResolveEpId             = 0x07,
    AllocateEpId            = 0x08,
    RoutingInfoUpdate       = 0x09,
    GetRoutTableEntry       = 0x0A,
    PrepareEpDiscovery      = 0x0B,
    EpDiscovery             = 0x0C,
    DiscoveryNotify         = 0x0D,
    GetNetworkId            = 0x0E,
    QueryHop                = 0x0F,
    ResolveUuid             = 0x10,
    QueryRateLimit          = 0x11,
    RequestTxRateLimit      = 0x12,
    UpdateRateLimit         = 0x13,
    QuerySupportedInterface = 0x14,
};

enum class MsgType : uint8_t
{
    Control    = 0x00,
    Pldm       = 0x01,
    Ncsi       = 0x02,
    Eth        = 0x03,
    Nvm        = 0x04,
    Spdm       = 0x05,
    VendorPci  = 0x7e,
    VendorIani = 0x7f,
};

enum class PacketType : uint8_t
{
    Response = 0,
    Request  = 1,
    Datagram = 2,
    Other    = 0xFF,
};

constexpr uint8_t BROADCAST_EID = 0xFF;
constexpr uint8_t NULL_EID      = 0x00;

enum class SetEndpoint : uint8_t
{
    SetEidNormal            = 0x00,
    SetEidForced            = 0x01,
    SetEidReset             = 0x02,
    SetEidDiscovered        = 0x03,
    EidAcceptedAndNoEidPool = 0x00,
    EidAcceptedAndEidPool   = 0x01,
    EidRejectedAndNoEidPool = 0x10,
    NoDynamicEidPool        = 0x00,
};

enum class EndpointType : uint8_t
{
    SimpleEndpoint = 0x00,
    BusOwnerBridge = 0x01,
};

enum class EndpointIdType : uint8_t
{
    DynamicEid                         = 0x00,
    StaticEidSupport                   = 0x01,
    StaticEidSupportPresentEidMatch    = 0x02,
    StaticEidSupportPresentEidNotMatch = 0x03,
};

enum class EidAssignStatus : uint8_t
{
    Accept = 0x00,
    Reject = 0x01,
};

enum class AllocateEndpoint : uint8_t
{
    Allocate      = 0x00,
    ForceAllocate = 0x01,
    GetInfo       = 0x02,
    Reserved      = 0x03,
};

enum class AllocateEndpointStatus : uint8_t
{
    Accept = 0x00,
    Reject = 0x01,
};

// Routing Infomation Update Request EntryType
// [7:4] Reserved, [3:0] EntryType
enum class EntryType : uint8_t
{
    SingleEndpoint        = 0x00,
    EidRangeIncludeBridge = 0x01,
    SingleEndpointBridge  = 0x02,
    EidRangeExcludeBridge = 0x03,
};

enum class VersionType : uint8_t
{
    Base       = 0xFF,
    ControlMsg = 0x00,
    Pldm       = 0x01,
    Vendor     = 0x7f,
};

// MSB
enum class Version : uint32_t
{
    Base       = 0x00F1F3F1,
    ControlMsg = 0x00F1F3F1,
    Pldm       = 0x00F0F0F1,
    Vendor     = 0x00FFF0F1,
};
}  // namespace pdk::mctp::app
