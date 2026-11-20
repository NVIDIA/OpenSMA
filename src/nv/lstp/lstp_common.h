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
#include <span>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <variant>

#include "nv/gpio/common.h"
#include "nv/i2c/i2c_types.h"
#include "nv/i2c/port.h"
#include "nv/spi/spi_types.h"
#include "sys/common/common.h"

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
    Management = 0x00,
    Spi        = 0x01,
    Gpio       = 0x02,
    I2c        = 0x03,
    Uart       = 0x04,
    Ipmi       = 0x05,
    /* 0x06 - 0x7E Reserved for future use
       0x7E can be used to reserve a channel ID, driver will skip with a warning */
    Reserved = 0x7E,
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
    SilentDrop   = 0xFE,  // Private driver status, don't send over USB
    IrqInterrupt = 0xFF,  // Used for interrupt-driven communication
};

inline LstpStatus LstpStatus_from(bool success)
{
    return success ? LstpStatus::Success : LstpStatus::Error;
}

inline LstpStatus LstpStatus_from(i2c::I2cStatus status)
{
    switch (status) {
        case i2c::I2cStatus::Ok        : return LstpStatus::Success;
        case i2c::I2cStatus::Error     : return LstpStatus::Error;
        case i2c::I2cStatus::Busy      : return LstpStatus::Busy;
        case i2c::I2cStatus::Nak       : return LstpStatus::Nak;
        case i2c::I2cStatus::Timeout   : return LstpStatus::Timeout;
        case i2c::I2cStatus::ArbLost   : return LstpStatus::ArbLost;
        case i2c::I2cStatus::MutexError: return LstpStatus::Error;
        default                        : return LstpStatus::Error;
    }

    return LstpStatus::Error;
}

inline LstpStatus LstpStatus_from(nv::gpio::Status status)
{
    switch (status) {
        case nv::gpio::Status::Ok          : return LstpStatus::Success;
        case nv::gpio::Status::Error       : return LstpStatus::Error;
        case nv::gpio::Status::InvalidParam: return LstpStatus::NotSupported;
        default                            : return LstpStatus::Error;
    }

    return LstpStatus::Error;
}

inline LstpStatus LstpStatus_from(nv::spi::bm::SpiStatus status)
{
    switch (status) {
        case nv::spi::bm::SpiStatus::Ok            : return LstpStatus::Success;
        case nv::spi::bm::SpiStatus::Busy          : return LstpStatus::Busy;
        case nv::spi::bm::SpiStatus::EventTimeout  : return LstpStatus::Timeout;
        case nv::spi::bm::SpiStatus::EventClearFail: return LstpStatus::Error;
        case nv::spi::bm::SpiStatus::EventXferFail : return LstpStatus::Error;
        case nv::spi::bm::SpiStatus::Error         : return LstpStatus::Error;
        default                                    : return LstpStatus::Error;
    }

    return LstpStatus::Error;
}

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
    ReadConfig      = 0x08,
    WriteConfig     = 0x09,
    WriteConfigMode = 0x0A,
    ReadConfigMode  = 0x0B,
    Start           = ReadConfig,
    End             = ReadConfigMode
};

struct [[gnu::packed]] LstpManagementConfig
{
    uint8_t lstp_version;
    uint8_t num_channels;
};

enum class LstpConfigMode : uint8_t
{
    NonVolatile = 0x00,
    Locked      = 0x01,
    Volatile    = 0x02,
};

struct [[gnu::packed]] LstpConfigModePayload
{
    LstpConfigMode config_mode;
};

constexpr LstpConfigMode CurrentConfigMode = LstpConfigMode::Locked;

/*****************************************************
 * LSTP SPI Channel
 *****************************************************/

constexpr uint8_t LstpSpiMaxCs   = 2U;
constexpr uint8_t LstpSpiCmdMask = 0xFU;

enum class LstpSpiCommand : uint8_t
{
    Config      = 0x00,
    Read        = 0x01,
    Write       = 0x02,
    WriteRead   = 0x03,
    PostedWrite = 0x04,
    End         = 0x07,
};

enum LstpSpiCommandFlags : uint8_t
{
    CsDeassert = 0x10,
    CsAssert   = 0x20,
    // Bit 6 selects which CS pin to operate on for this transfer.
    // 0 = CS0, 1 = CS1. One bit is sufficient because LstpSpiMaxCs == 2.
    CsSelect = 0x40,
};

struct [[gnu::packed]] LstpSpiChannelConfig
{
    uint8_t  channel_num_cs;
    uint32_t freq_hz;
    uint16_t output_drive_strength_ma = 0U;
    uint16_t slew_rate_100_ps         = 0U;
    uint16_t input_debounce_time_us   = 0U;
    uint16_t glitch_filter_width_ns   = 0U;
};

struct [[gnu::packed]] LstpSpiReadRequest
{
    uint32_t read_len;
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
    Low           = 0U,
    High          = 1U,
    HighImpedance = 2U,
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
// GPIO config is not forward compatible
static_assert(sizeof(LstpGpioConfig) == 48U, "LstpGpioConfig size must be 48 bytes");

struct [[gnu::packed]] LstpGpioPinInfo
{
    gpio::GpioPort port;
    gpio::GpioPin  pin;
    LstpGpioConfig config;
    bool           allow_set_irq      = false;  // Do not enable for GPIOs owned by other tasks
    bool allow_lstp_direction_control = false;  // Do not enable for GPIOs owned by other tasks
                                                // or system critical GPIOs
};

// vrPort pins have no hardware direction register — report configured direction.
// Defined in lstp_router.cpp to keep nv/gpio/driver.h out of this header
// (its <assert.h> shadows nv::assert).
nv::gpio::Status gpio_get_direction(const LstpGpioPinInfo& info, nv::gpio::Direction& dir);

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

/// Origin of a GPIO IRQ event. GPIO spoofing isolates LSTP from the pins, so while it is active
/// only levels it commands may surface as events.
enum class GpioIrqSource : uint8_t
{
    Pin,       ///< Edge observed on the physical or virtual pin.
    Spoofing,  ///< Level change commanded through NSM Type 5 GPIO spoofing.
};

template<typename GpioIndexMap>
constexpr GpioIndex LstpGpioIdFromPortPin(const GpioIndexMap& gpio_index_map,
                                          nv::gpio::GpioPort  port,
                                          nv::gpio::GpioPin   pin)
{
    return gpio_index_map[port][pin];
}

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

/// Payload of the LSTP GPIO IrqEvent packet sent to the host. Its size is what goes into the
/// LSTP header length field, so the layout is fixed by the protocol.
struct [[gnu::packed]] LstpGpioIrqEvent
{
    GpioIndex     gpio_index;
    LstpGpioState value;
};
static_assert(sizeof(LstpGpioIrqEvent) == sizeof(GpioIndex) + sizeof(LstpGpioState),
              "LSTP GPIO IrqEvent payload layout is fixed by the protocol");

/// Item queued from the producer to the LSTP task for a pending GPIO IRQ. Kept separate from
/// the wire payload so it can carry data the host never sees.
struct [[gnu::packed]] LstpGpioIrqEventRequest
{
    GpioIndex     gpio_index;
    LstpGpioState value;
    /// Lets the consumer tell a physical edge queued before GPIO spoofing began from a level
    /// change commanded while it is active.
    GpioIrqSource source;
};

inline LstpGpioDirection LstpGpioDirection_from(nv::gpio::Direction dir)
{
    return (dir == nv::gpio::Direction::Output) ? LstpGpioDirection::Output
                                                : LstpGpioDirection::Input;
}

inline nv::gpio::Direction GpioDirection_from(LstpGpioDirection dir)
{
    return (dir == LstpGpioDirection::Output) ? nv::gpio::Direction::Output
                                              : nv::gpio::Direction::Input;
}

inline nv::gpio::GpioState GpioState_from(LstpGpioState state)
{
    return (state == LstpGpioState::High) ? nv::gpio::GpioState::High
                                          : nv::gpio::GpioState::Low;
}

inline nv::gpio::GpioOpenDrain GpioOpenDrain_from(LstpGpioOutputDriveConfig cfg)
{
    return (cfg == LstpGpioOutputDriveConfig::OpenDrain) ? nv::gpio::GpioOpenDrain::Enable
                                                         : nv::gpio::GpioOpenDrain::Disable;
}

inline nv::gpio::GpioPullDir GpioPullDir_from(LstpGpioBiasPullConfig cfg)
{
    switch (cfg) {
        case LstpGpioBiasPullConfig::PullUp  : return nv::gpio::GpioPullDir::PullUp;
        case LstpGpioBiasPullConfig::PullDown: return nv::gpio::GpioPullDir::PullDown;
        default                              : return nv::gpio::GpioPullDir::Disabled;
    }
}

inline nv::gpio::GpioPullStrength GpioPullStrength_from(uint16_t bias_pull_strength)
{
    return (bias_pull_strength == 0) ? nv::gpio::GpioPullStrength::Low
                                     : nv::gpio::GpioPullStrength::High;
}

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
    uint8_t  speed;
    uint16_t output_drive_strength_ma = 0U;
    uint16_t slew_rate_100_ps         = 0U;
    uint16_t input_debounce_time_us   = 0U;
    uint16_t glitch_filter_width_ns   = 0U;
    uint8_t  bus_low_timeout_ms       = 0U;
    uint8_t  bus_arbitration_retries  = 0U;
};

struct LstpI2cChannelPriv
{
    nv::i2c::AddressMask need_debug_token;
};

constexpr uint8_t LstpI2cCmdMask = 0xBF;  // Bits [0:5] for command
enum LstpI2cCommandFlags : uint8_t
{
    NoStop = 0x40,  // Bit 6: don't send STOP condition
};

enum class LstpI2cCommand : uint8_t
{
    BusRecovery           = 0x00,
    Read                  = 0x01,
    Write                 = 0x02,
    DeprecatedReadRecvLen = 0x03,  // Deprecated: Do not use
    WriteRead             = 0x04,
    SmbusBlockRead        = 0x05,
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

struct [[gnu::packed]] LstpI2cDeprecatedReadRecvLenRequest
{
    uint8_t address;
};

struct [[gnu::packed]] LstpI2cWriteReadRequest
{
    uint8_t  address;
    uint16_t read_len;
    // Followed by M bytes of write data
};

struct [[gnu::packed]] LstpI2cSmbusBlockReadRequest
{
    uint8_t  address;
    uint16_t include_smbus_pec;
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
{
    // Firmware-private SSIF binding. IPMI has no host-visible channel configuration payload.
    nv::i2c::Port      i2c_port = nv::i2c::Port::End;
    nv::gpio::GpioPort alert_port;
    nv::gpio::GpioPin  alert_pin;
    bool               default_enabled = false;
};

constexpr uint8_t LstpIpmiMaxInstances = 2;

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
    uint16_t output_drive_strength_ma = 0U;
    uint16_t slew_rate_100_ps         = 0U;
    uint16_t input_debounce_time_us   = 0U;
    uint16_t glitch_filter_width_ns   = 0U;
};

/*****************************************************
 * Dummy config scructure for reserved channel IDs
 *****************************************************/

struct LstpReservedChannelConfig
{};

/*****************************************************
 * Compile-time config helpers
 *****************************************************/

using LstpChannelConfig = std::variant<LstpManagementConfig,
                                       LstpSpiChannelConfig,
                                       LstpGpioChannelConfig,
                                       LstpI2cChannelConfig,
                                       LstpIpmiChannelConfig,
                                       LstpUartChannelConfig,
                                       LstpReservedChannelConfig>;

using LstpChannelPriv = std::variant<std::monostate, LstpI2cChannelPriv>;

struct LstpChannelEntry
{
    LstpChannelInfo   info;
    LstpChannelConfig config;
    nv::ipc::CoreId   core_id;
    LstpChannelPriv   priv;
};

template<size_t N>
constexpr bool ValidateLstpChannelConfigs(const std::array<LstpChannelEntry, N>& channels)
{
    if (channels.size() > LSTP_MAX_CHANNELS) {
        return false;
    }

    for (const auto& entry : channels) {
        const auto& info = entry.info;
        const auto& cfg  = entry.config;

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
            case LstpChannelType::Ipmi: {
                if (!std::holds_alternative<LstpIpmiChannelConfig>(cfg)) {
                    return false;
                }
                const auto& ipmi_cfg = std::get<LstpIpmiChannelConfig>(cfg);
                if (info.id >= LstpIpmiMaxInstances || ipmi_cfg.i2c_port >= nv::i2c::Port::End
                    || ipmi_cfg.alert_port == nv::gpio::InvalidGpioPort
                    || ipmi_cfg.alert_pin == nv::gpio::InvalidGpioPin) {
                    return false;
                }
                break;
            }
            case LstpChannelType::Uart:
                if (!std::holds_alternative<LstpUartChannelConfig>(cfg)) {
                    return false;
                }
                break;
            case LstpChannelType::Reserved:
                if (!std::holds_alternative<LstpReservedChannelConfig>(cfg)) {
                    return false;
                }
                break;
            default: return false;
        }
    }

    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i].info.type != LstpChannelType::Ipmi) {
            continue;
        }
        const auto& lhs = std::get<LstpIpmiChannelConfig>(channels[i].config);
        for (size_t j = i + 1; j < channels.size(); ++j) {
            if (channels[j].info.type != LstpChannelType::Ipmi) {
                continue;
            }
            const auto& rhs = std::get<LstpIpmiChannelConfig>(channels[j].config);
            if (channels[i].info.id == channels[j].info.id || lhs.i2c_port == rhs.i2c_port
                || (lhs.alert_port == rhs.alert_port && lhs.alert_pin == rhs.alert_pin)) {
                return false;
            }
        }
    }
    return true;
}

template<size_t N>
constexpr bool IsLstpEnabled(const std::array<LstpChannelEntry, N>& channels,
                             const nv::ipc::CoreId                  core_id)
{
    return std::any_of(channels.begin(), channels.end(), [core_id](const auto& entry) {
        return entry.core_id == core_id || entry.core_id == nv::ipc::CoreId::Both;
    });
}

template<size_t N>
constexpr bool IsChannelEnabled(const std::array<LstpChannelEntry, N>& channels,
                                const LstpChannelType                  channel_type,
                                const nv::ipc::CoreId core_id = nv::ipc::CoreId::Both)
{
    return std::any_of(
        channels.begin(), channels.end(), [channel_type, core_id](const auto& entry) {
            return entry.info.type == channel_type
                && (entry.core_id == core_id || core_id == nv::ipc::CoreId::Both);
        });
}

template<size_t N>
constexpr uint8_t GetFirstChannelId(const std::array<LstpChannelEntry, N>& channels,
                                    const LstpChannelType                  channel_type)
{
    for (uint8_t i = 0; i < channels.size(); ++i) {
        if (channels[i].info.type == channel_type) {
            return i;
        }
    }
    return channels.size();
}

/** Runtime linear search - for use on span (runtime product tables). */
inline uint8_t GetFirstChannelId(std::span<const LstpChannelEntry> channels,
                                 LstpChannelType                   channel_type)
{
    for (uint8_t i = 0; i < static_cast<uint8_t>(channels.size()); ++i) {
        if (channels[i].info.type == channel_type) {
            return i;
        }
    }
    return static_cast<uint8_t>(channels.size());
}

/**
 * Project-specific LSTP tables.
 * Weak defaults return the envelope compile-time tables (from config.h);
 * multi-product images provide strong overrides in product_config.cpp (Core0)
 * and product_bm_config.cpp (Core1).
 */
std::span<const LstpGpioPinInfo>  project_pin_configs();
std::span<const LstpChannelEntry> project_lstp_channels();

template<size_t N>
constexpr uint8_t GetChannelId(const std::array<LstpChannelEntry, N>& channels,
                               const LstpChannelType                  channel_type,
                               const nv::i2c::Port                    i2c_port)
{
    if (channel_type != LstpChannelType::Ipmi) {
        return channels.size();
    }

    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i].info.type == channel_type
            && std::get<LstpIpmiChannelConfig>(channels[i].config).i2c_port == i2c_port) {
            return static_cast<uint8_t>(i);
        }
    }
    return channels.size();
}

/** Resolves the channel id of one specific peripheral instance, for channel types that a
 *  project may declare more than once (a board with several UARTs, for example). Returns
 *  channels.size() when no such channel exists. */
template<size_t N>
constexpr uint8_t GetChannelIdByInstance(const std::array<LstpChannelEntry, N>& channels,
                                         const LstpChannelType                  channel_type,
                                         const uint8_t                          instance)
{
    for (uint8_t i = 0; i < channels.size(); ++i) {
        if ((channels[i].info.type == channel_type) && (channels[i].info.id == instance)) {
            return i;
        }
    }
    return channels.size();
}

template<size_t N>
constexpr bool projectHasVirtualGpios(const std::array<LstpGpioPinInfo, N>& pin_configs)
{
    return std::any_of(pin_configs.begin(), pin_configs.end(), [](const auto& pin_info) {
        return pin_info.port == nv::gpio::vrPort;
    });
}

/** Preserve legacy per-pin GPIO config writes while the device reports locked mode. */
template<size_t N>
constexpr LstpConfigMode GpioWriteConfigMode(const std::array<LstpGpioPinInfo, N>& pin_configs)
{
    for (const auto& pin_info : pin_configs) {
        if (pin_info.allow_lstp_direction_control) {
            return LstpConfigMode::NonVolatile;
        }
    }
    return LstpConfigMode::Locked;
}

/** Runtime overload -- for product tables selected at runtime (see project_pin_configs). */
inline LstpConfigMode GpioWriteConfigMode(std::span<const LstpGpioPinInfo> pin_configs)
{
    for (const auto& pin_info : pin_configs) {
        if (pin_info.allow_lstp_direction_control) {
            return LstpConfigMode::NonVolatile;
        }
    }
    return LstpConfigMode::Locked;
}

template<size_t N>
constexpr size_t LstpChannelTypeCount(const std::array<LstpChannelEntry, N>& channels,
                                      const LstpChannelType                  channel_type)
{
    size_t count = 0;
    for (const auto& entry : channels) {
        if (entry.info.type == channel_type) {
            count++;
        }
    }
    return count;
}
}  // namespace nv::lstp
