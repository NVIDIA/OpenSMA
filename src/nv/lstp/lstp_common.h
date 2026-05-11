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
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <variant>

#include "nv/gpio/common.h"

namespace nv::lstp {

template<typename T>
constexpr inline const T& from(const void* ptr)
{
    return *reinterpret_cast<const T*>(ptr);  // NOLINT(*-reinterpret-cast)
}

template<typename T, typename = std::enable_if_t<!std::is_const_v<T>>>
constexpr inline T& from(void* ptr)
{
    return *reinterpret_cast<T*>(ptr);  // NOLINT(*-reinterpret-cast)
}

/*****************************************************
 * Bit manipulation constants
 *****************************************************/

constexpr uint32_t BYTE1_SHIFT = 8;
constexpr uint32_t BYTE2_SHIFT = 16;
constexpr uint32_t BYTE3_SHIFT = 24;
constexpr uint16_t LSB_MASK    = 0xff;
constexpr uint16_t MSB_MASK    = 0xff00;

/*****************************************************
 * LSTP General
 *****************************************************/

constexpr uint8_t LSTP_VERSION      = 1U;
constexpr size_t  LSTP_MAX_CHANNELS = 255;

enum class LstpChannelType : uint8_t
{
    Management = 0,
    Spi        = 1,
    Gpio       = 2,
    I2c        = 3,
    Uart       = 4,
    Ipmi       = 5
    /* 0x06 - 0x7E Reserved for future use */
    /* 0x7F - 0xFF is OEM Defined */
};

struct LstpChannelInfo
{
    LstpChannelType type;
    uint8_t         id;
    uint8_t         channel_name[16];
};

constexpr uint8_t LstpResponseBit = 0b10000000;

enum class LstpStatus : uint8_t
{
    Success      = 0x00,
    Error        = 0x01,
    Timeout      = 0x02,
    Busy         = 0x03,
    Nak          = 0x04,
    ArbLost      = 0x05,
    NotSupported = 0x06,
    TooLarge     = 0x07,
    IrqInterrupt = 0xFF,  // Used for interrupt-driven communication
};

struct [[gnu::packed]] LstpHdr
{
    uint8_t channel_id;
    uint8_t cmd_status_code;  // Bit 7 is reserved for request (=0) and response (=1) flag
    uint8_t len_lsb;
    uint8_t len_msb;
};

constexpr size_t LstpMsgSize        = 512;
constexpr size_t LstpMaxPayloadSize = LstpMsgSize - sizeof(LstpHdr);

struct [[gnu::packed]] LstpChannelConfigWriteRequest
{
    uint8_t  channel_id;
    uint16_t offset;
};

struct [[gnu::packed]] LstpChannelConfigReadRequest
{
    uint8_t  channel_id;
    uint16_t offset;
    uint16_t length;
};

struct [[gnu::packed]] LstpChannelConfigBlob
{
    uint8_t channel_type;
    uint8_t channel_enabled;
    uint8_t channel_name[16];
    // Followed by channel-specific config data
};

/*****************************************************
 * LSTP Management Channel (Ch0)
 *****************************************************/

enum class LstpManagementCommand : uint8_t
{
    ReadConfig  = 0x08,
    WriteConfig = 0x09,
    Lock        = 0x0A,
    Start       = ReadConfig,
    End         = Lock
};

struct [[gnu::packed]] LstpManagementConfig
{
    uint8_t lstp_version;
    uint8_t num_channels;
};

/*****************************************************
 * LSTP SPI Channel
 *****************************************************/

constexpr uint8_t LstpSpiMaxCs = 4U;

struct [[gnu::packed]] LstpSpiChannelConfig
{
    uint8_t  channel_num_cs;
    uint32_t freq_hz;
};

/*****************************************************
 * LSTP GPIO Channel
 *****************************************************/

struct [[gnu::packed]] LstpGpioChannelConfig
{
    uint8_t channel_num_gpio;
    // Followed by channel_num_gpio * LstpGpioConfig
};

enum class LstpGpioDirection : uint8_t
{
    Output = 0U,
    Input  = 1U,
};

enum class LstpGpioState : uint8_t
{
    Low  = 0U,
    High = 1U,
};

enum class LstpGpioOutputDriveConfig : uint8_t
{
    PushPull   = 0U,
    OpenDrain  = 1U,
    OpenSource = 2U,
};

enum class LstpGpioBiasPullConfig : uint8_t
{
    NoPull   = 0U,
    PullUp   = 1U,
    PullDown = 2U,
};

enum class LstpGpioSlewRate : uint16_t
{
    Slow = 0U,
    Fast = 1U,
};

static constexpr uint8_t MaxGpioNameLength = 32U;
static constexpr uint8_t MaxGpiosPerPacket = 10U;
constexpr uint8_t        LstpGpioCmdMask   = 0x7F;  // Bits [0:6] for command

struct [[gnu::packed]] LstpGpioConfig
{
    std::array<uint8_t, MaxGpioNameLength> gpio_name;
    LstpGpioDirection                      direction;
    LstpGpioState                          default_output;
    LstpGpioOutputDriveConfig              output_drive_config;
    uint8_t                                output_persist_state;  // Bool
    LstpGpioBiasPullConfig                 bias_pull_config;
    uint16_t                               bias_pull_strength;     // 16 ohm increments
    uint16_t                               output_drive_strength;  // in mA
    uint16_t                               slew_rate;  // in 100 ps (0.1 ns) increments
    uint16_t                               input_debounce_time;  // in us
    uint8_t                                RSVD_0;
    uint8_t                                RSVD_1;
    uint8_t                                RSVD_2;
};
static_assert(sizeof(LstpGpioConfig) == 48U, "LstpGpioConfig size is not 48 bytes");

struct [[gnu::packed]] LstpGpioPinInfo
{
    gpio::GpioPort port;
    gpio::GpioPin  pin;
    LstpGpioConfig config;
    bool           allow_set_irq = false;  // optional field in PinConfigs, default false
};

enum class LstpGpioIrqConfig : uint8_t
{
    IrqDisabled = 0U,
    Rising      = 1U,
    Falling     = 2U,
    BothEdge    = 3U,
    High        = 4U,
    Low         = 5U,
    Max         = Low,
};

enum class LstpGpioCommand : uint8_t
{
    GetValue     = 0x00,
    SetValue     = 0x01,
    GetIrqConfig = 0x02,
    SetIrqConfig = 0x03,
    IrqEvent     = 0x04,
};

using GpioIndex = uint16_t;

struct [[gnu::packed]] LstpGpioGetValueRequest
{
    GpioIndex gpio_index;
};

struct [[gnu::packed]] LstpGpioGetValueResponse
{
    LstpGpioState value;
};

struct [[gnu::packed]] LstpGpioSetValueRequest
{
    GpioIndex     gpio_index;
    LstpGpioState value;
};

struct [[gnu::packed]] LstpGpioGetIrqConfigRequest
{
    GpioIndex gpio_index;
};

struct [[gnu::packed]] LstpGpioGetIrqConfigResponse
{
    LstpGpioIrqConfig irq_type;
};

struct [[gnu::packed]] LstpGpioSetIrqConfigRequest
{
    GpioIndex         gpio_index;
    LstpGpioIrqConfig irq_type;
};

struct [[gnu::packed]] LstpGpioIrqEventRequest
{
    GpioIndex     gpio_index;
    LstpGpioState value;
};

/*****************************************************
 * LSTP I2C Channel
 *****************************************************/

enum class LstpI2cSpeed : uint8_t
{
    Standard = 0x00,  // 100kHz
    Fast     = 0x01,  // 400kHz
    FastPlus = 0x02,  // 1MHz
    High     = 0x03,  // 3.4MHz
};

struct [[gnu::packed]] LstpI2cChannelConfig
{
    uint8_t speed;
};

constexpr uint8_t LstpI2cCmdMask = 0xBF;  // Bits [0:5] for command
enum LstpI2cCommandFlags : uint8_t
{
    NoStop = 0x40,  // Bit 6: don't send STOP condition
};

enum class LstpI2cCommand : uint8_t
{
    BusRecovery = 0x00,
    Read        = 0x01,
    Write       = 0x02,
    ReadRecvLen = 0x03,
    WriteRead   = 0x04,
};

// I2C Request Structs
struct [[gnu::packed]] LstpI2cReadRequest
{
    uint8_t  address;
    uint16_t read_len;
};

struct [[gnu::packed]] LstpI2cWriteRequest
{
    uint8_t address;
    // Followed by N bytes of write data
};

struct [[gnu::packed]] LstpI2cReadRecvLenRequest
{
    uint8_t address;
};

struct [[gnu::packed]] LstpI2cWriteReadRequest
{
    uint8_t  address;
    uint16_t read_len;
    // Followed by M bytes of write data
};

/*****************************************************
 * LSTP IPMI Channel
 *****************************************************/

enum class LstpIpmiCommand : uint8_t
{
    IpmiMessage = 0x00
};

struct [[gnu::packed]] LstpIpmiChannelConfig
{};

/*****************************************************
 * LSTP UART Channel
 *****************************************************/

constexpr uint8_t LstpUartCmdMask = 0x03U;  // Bits [6:2] reserved for flags

enum class LstpUartCommand : uint8_t
{
    Write = 0x00
};

enum class LstpUartParity : uint8_t
{
    None  = 0,
    Odd   = 1,
    Even  = 2,
    Mark  = 3,
    Space = 4,
};

struct [[gnu::packed]] LstpUartChannelConfig
{
    uint32_t speed;                                                   // Baud rate in Hz
    uint8_t  data_bits = 8;                                           // Number of data bits
    uint8_t  stop_bits = 1;                                           // Number of stop bits
    uint8_t  parity    = static_cast<uint8_t>(LstpUartParity::None);  // [4:7] reserved for flow
                                                                      // control
};

/*****************************************************
 * Compile-time config helpers
 *****************************************************/

using LstpChannelConfig = std::variant<LstpManagementConfig,
                                       LstpSpiChannelConfig,
                                       LstpGpioChannelConfig,
                                       LstpI2cChannelConfig,
                                       LstpIpmiChannelConfig,
                                       LstpUartChannelConfig>;
using LstpChannelEntry  = std::tuple<LstpChannelInfo, LstpChannelConfig>;

template<size_t N>
constexpr bool ValidateLstpChannelConfigs(const std::array<LstpChannelEntry, N>& channels)
{
    if (channels.size() > LSTP_MAX_CHANNELS) {
        return false;
    }

    for (const auto& entry : channels) {
        const auto& info = std::get<0>(entry);
        const auto& cfg  = std::get<1>(entry);

        switch (info.type) {
            case LstpChannelType::Management:
                if (!std::holds_alternative<LstpManagementConfig>(cfg)) {
                    return false;
                }
                break;
            case LstpChannelType::Spi: {
                if (!std::holds_alternative<LstpSpiChannelConfig>(cfg)) {
                    return false;
                }
                const auto& spi_cfg = std::get<LstpSpiChannelConfig>(cfg);
                if (spi_cfg.channel_num_cs == 0 || spi_cfg.channel_num_cs > LstpSpiMaxCs) {
                    return false;
                }
                break;
            }
            case LstpChannelType::Gpio:
                if (!std::holds_alternative<LstpGpioChannelConfig>(cfg)) {
                    return false;
                }
                break;
            case LstpChannelType::I2c:
                if (!std::holds_alternative<LstpI2cChannelConfig>(cfg)) {
                    return false;
                }
                break;
            case LstpChannelType::Ipmi:
                if (!std::holds_alternative<LstpIpmiChannelConfig>(cfg)) {
                    return false;
                }
                break;
            case LstpChannelType::Uart:
                if (!std::holds_alternative<LstpUartChannelConfig>(cfg)) {
                    return false;
                }
                break;
            default: return false;
        }
    }
    return true;
}

template<size_t N>
constexpr bool IsChannelEnabled(const std::array<LstpChannelEntry, N>& channels,
                                const LstpChannelType                  channel_type)
{
    return std::any_of(channels.begin(), channels.end(), [channel_type](const auto& entry) {
        return std::get<0>(entry).type == channel_type;
    });
}

template<size_t N>
constexpr uint8_t GetFirstChannelId(const std::array<LstpChannelEntry, N>& channels,
                                    const LstpChannelType                  channel_type)
{
    for (uint8_t i = 0; i < channels.size(); ++i) {
        if (std::get<0>(channels[i]).type == channel_type) {
            return i;
        }
    }
    return channels.size();
}

}  // namespace nv::lstp
