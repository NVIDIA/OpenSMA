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
#include "nv/lstp/lstp_router.h"
#include "nv/lstp/lstp_task.h"

#include <cstring>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <tuple>
#include <variant>

#include "nv/i2c/common.h"
#include "nv/ipc/event.h"
#include "nv/nv.h"
#include "nv/ipc/queue.h"
#include "nv/logger/log.h"
#include "nv/spi/flashrom.h"
#include "nv/usb/task.h"
#include "nv/ssif/task.h"
#include "nv/i2c/task.h"
#include "nv/spi/flashrom_task.h"
#include "nv/vruart/lstp_bridge.h"

namespace nv::lstp {

/*****************************************************
 * LSTP General
 *****************************************************/

void LstpRouter::init()
{
    for (size_t channel_id = 0; channel_id < LstpNumChannels; channel_id++) {
        const auto& channel_info    = std::get<0>(LstpChannels.at(channel_id));
        bool        channel_enabled = false;

        switch (channel_info.type) {
            case LstpChannelType::Ipmi: {
                if constexpr (EnableIpmi) {
                    channel_enabled = nv::ssif::Task::is_default_enabled();
                }
                break;
            }
            default: {
                // Default to all present channels enabled
                // Channel types are validated at compile time
                channel_enabled = true;
                break;
            }
        }

        _channels.at(channel_id).enabled = channel_enabled;
    }
}

void LstpRouter::receive(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& buffer,
                         uint32_t                                      buffer_len)
{
    if (buffer.size() != LstpMsgSize) {
        return;
    }

    if (buffer_len < sizeof(LstpHdr)) {
        return;
    }

    auto& hdr = from<LstpHdr>(buffer.data());

    const LstpStatus status = receive_helper(buffer, buffer_len);

    // Only handle failure responses here for convenience
    if (status != LstpStatus::Success) {
        std::array<uint8_t, nv::ipc::UsbLstpMsgSize> resp_buf{};
        auto&                                        resp_hdr = from<LstpHdr>(resp_buf.data());

        resp_hdr.channel_id      = hdr.channel_id;
        resp_hdr.cmd_status_code = static_cast<uint8_t>(status) | LstpResponseBit;
        resp_hdr.len_lsb         = 0;
        resp_hdr.len_msb         = 0;

        std::span<uint8_t> item(resp_buf.data(), resp_buf.size());
        nv::usb::Task::to_usbLstp(item);
    }
}

LstpStatus LstpRouter::receive_helper(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& buffer,
                                      uint32_t                                      buffer_len)
{
    auto& hdr = from<LstpHdr>(buffer.data());

    if (static_cast<size_t>(hdr.len_lsb) + static_cast<size_t>(hdr.len_msb << BYTE1_SHIFT)
            + sizeof(LstpHdr)
        > buffer_len) {
        return LstpStatus::Error;
    }

    if (hdr.channel_id >= LstpNumChannels) {
        return LstpStatus::Error;
    }

    // Flashrom (NV_SMA_SPI) may use channel ID = 0 in the request packet
    // For backward compatibility, treat channel 0 commands 0x0..0x7 as SPI commands
    // This assumes there is no aliasing between channel 0 and Flashom on command byte bits 3..0
    auto channel_type = std::get<0>(LstpChannels.at(hdr.channel_id)).type;
    if constexpr (EnableSpi) {
        static_assert(static_cast<uint8_t>(LstpManagementCommand::Start)
                      > static_cast<uint8_t>(spi::FlashromCmdCode::SPI_CMD_END));
        static_assert(static_cast<uint8_t>(LstpManagementCommand::End)
                      <= spi::Flashrom::CMD_CODE_MASK);
        if ((hdr.channel_id == 0)
            && (hdr.cmd_status_code & spi::Flashrom::CMD_CODE_MASK)
                   <= spi::FlashromCmdCode::SPI_CMD_END) {
            channel_type = LstpChannelType::Spi;
        }
    }

    LstpStatus status = LstpStatus::NotSupported;
    switch (channel_type) {
        case LstpChannelType::Management: {
            status = receive_ch0(buffer);
            break;
        }
        case LstpChannelType::Spi: {
            if constexpr (EnableSpi) {
                status = receive_spi(buffer);
            }
            break;
        }
        case LstpChannelType::Gpio: {
            if constexpr (EnableGpio) {
                status = receive_gpio(buffer);
            }
            break;
        }
        case LstpChannelType::Ipmi: {
            if constexpr (EnableIpmi) {
                status = receive_ipmi(buffer);
            }
            break;
        }
        case LstpChannelType::I2c: {
            if constexpr (EnableI2c) {
                status = receive_i2c(buffer);
            }
            break;
        }
        case LstpChannelType::Uart: {
            if constexpr (EnableUart) {
                status = receive_uart(buffer);
            }
            break;
        }
        default: break;
    }

    return status;
}

/*****************************************************
 * LSTP Management Channel (Ch0)
 *****************************************************/

LstpStatus LstpRouter::receive_ch0(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& buffer)
{
    auto& hdr = from<LstpHdr>(buffer.data());

    if (hdr.cmd_status_code & LstpResponseBit) {
        // Don't send response to spurious response
        return LstpStatus::Success;
    }

    auto cmd = static_cast<LstpManagementCommand>(hdr.cmd_status_code);

    switch (cmd) {
        case LstpManagementCommand::ReadConfig : return read_channel_config(buffer);
        case LstpManagementCommand::WriteConfig: return write_channel_config(buffer);
        case LstpManagementCommand::Lock       : return LstpStatus::NotSupported;
        default                                : return LstpStatus::NotSupported;
    }
}

LstpStatus
LstpRouter::read_channel_config(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& req_buffer)
{
    static_assert(sizeof(LstpChannelConfigReadRequest) <= LstpMaxPayloadSize);

    size_t req_offset  = 0;
    auto&  req_hdr     = from<LstpHdr>(req_buffer.data());
    req_offset        += sizeof(LstpHdr);

    const size_t req_size = static_cast<size_t>(req_hdr.len_lsb)
                          | (static_cast<size_t>(req_hdr.len_msb) << BYTE1_SHIFT);
    if (req_size < sizeof(LstpChannelConfigReadRequest)) {
        return LstpStatus::Error;
    }

    auto& channel_config_request = from<LstpChannelConfigReadRequest>(
        &req_buffer.data()[req_offset]);

    if (channel_config_request.channel_id >= LstpNumChannels) {
        return LstpStatus::Error;
    }

    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> resp_buffer{};
    size_t                                       resp_offset = 0;

    auto& resp_hdr  = from<LstpHdr>(resp_buffer.data());
    resp_offset    += sizeof(LstpHdr);

    auto& resp_data  = from<LstpChannelConfigBlob>(&resp_buffer.data()[resp_offset]);
    resp_offset     += sizeof(LstpChannelConfigBlob);

    const auto& channel_entry = LstpChannels.at(channel_config_request.channel_id);
    const auto& channel_info  = std::get<0>(channel_entry);
    const auto& channel_cfg   = std::get<1>(channel_entry);
    const auto& channel_state = _channels.at(channel_config_request.channel_id);

    resp_data.channel_type    = static_cast<uint8_t>(channel_info.type);
    resp_data.channel_enabled = channel_state.enabled;

    memcpy(&resp_data.channel_name[0],
           &channel_info.channel_name[0],
           sizeof(resp_data.channel_name));

    // Add channel-specific config based on type
    switch (channel_info.type) {
        case LstpChannelType::Management: {
            static_assert(sizeof(LstpChannelConfigBlob) + sizeof(LstpManagementConfig)
                          <= LstpMaxPayloadSize);
            auto& managementConfigResp = from<LstpManagementConfig>(
                &resp_buffer.data()[resp_offset]);
            resp_offset += sizeof(LstpManagementConfig);

            // Config type validated at compile time via static_assert in config.h
            const auto& mgmt_cfg              = std::get<LstpManagementConfig>(channel_cfg);
            managementConfigResp.lstp_version = LSTP_VERSION;
            managementConfigResp.num_channels = mgmt_cfg.num_channels - 1;  // Mngnt ch0 not
                                                                            // included
            break;
        }
        case LstpChannelType::Spi: {
            static_assert(sizeof(LstpChannelConfigBlob) + sizeof(LstpSpiChannelConfig)
                          <= LstpMaxPayloadSize);
            if constexpr (EnableSpi) {
                auto& spiConfigResp = from<LstpSpiChannelConfig>(
                    &resp_buffer.data()[resp_offset]);
                resp_offset += sizeof(LstpSpiChannelConfig);

                // Config type validated at compile time via static_assert in config.h
                const auto& spi_cfg          = std::get<LstpSpiChannelConfig>(channel_cfg);
                spiConfigResp.channel_num_cs = spi_cfg.channel_num_cs;
                spiConfigResp.freq_hz        = spi_cfg.freq_hz;
            }
            else {
                return LstpStatus::NotSupported;
            }
            break;
        }
        case LstpChannelType::Gpio: {
            if constexpr (EnableGpio) {
                static_assert(sizeof(LstpChannelConfigBlob) + sizeof(LstpGpioChannelConfig)
                                  + MaxGpiosPerPacket * sizeof(LstpGpioConfig)
                              <= LstpMaxPayloadSize);
                const LstpStatus gpio_status = read_gpio_config_helper(
                    channel_config_request.offset,
                    channel_config_request.length,
                    resp_buffer,
                    resp_offset);
                if (gpio_status != LstpStatus::Success) {
                    return gpio_status;
                }
            }
            else {
                return LstpStatus::NotSupported;
            }
            break;
        }
        case LstpChannelType::I2c: {
            if constexpr (EnableI2c) {
                static_assert(sizeof(LstpChannelConfigBlob) + sizeof(LstpI2cChannelConfig)
                              <= LstpMaxPayloadSize);
                auto& i2cConfigResp = from<LstpI2cChannelConfig>(
                    &resp_buffer.data()[resp_offset]);
                resp_offset += sizeof(LstpI2cChannelConfig);

                // Config type validated at compile time via static_assert in config.h
                const auto& i2c_cfg = std::get<LstpI2cChannelConfig>(channel_cfg);
                i2cConfigResp.speed = i2c_cfg.speed;
            }
            else {
                return LstpStatus::NotSupported;
            }
            break;
        }
        case LstpChannelType::Ipmi: {
            static_assert(sizeof(LstpChannelConfigBlob) + sizeof(LstpIpmiChannelConfig)
                          <= LstpMaxPayloadSize);
            if constexpr (EnableIpmi) {
                // Empty config for IPMI channel
            }
            else {
                return LstpStatus::NotSupported;
            }
            break;
        }
        case LstpChannelType::Uart: {
            if constexpr (EnableUart) {
                static_assert(sizeof(LstpChannelConfigBlob) + sizeof(LstpUartChannelConfig)
                              <= LstpMaxPayloadSize);
                auto& uartConfigResp = from<LstpUartChannelConfig>(
                    &resp_buffer.data()[resp_offset]);
                resp_offset += sizeof(LstpUartChannelConfig);

                const auto& uart_cfg     = std::get<LstpUartChannelConfig>(channel_cfg);
                uartConfigResp.speed     = uart_cfg.speed;
                uartConfigResp.data_bits = uart_cfg.data_bits;
                uartConfigResp.stop_bits = uart_cfg.stop_bits;
                uartConfigResp.parity    = uart_cfg.parity;
            }
            else {
                return LstpStatus::NotSupported;
            }
            break;
        }
        default: {
            static_assert(sizeof(LstpChannelConfigBlob) <= LstpMaxPayloadSize);
            return LstpStatus::NotSupported;
            break;
        }
    }

    const auto len           = static_cast<uint16_t>(resp_offset - sizeof(LstpHdr));
    resp_hdr.channel_id      = req_hdr.channel_id;
    resp_hdr.cmd_status_code = static_cast<uint8_t>(LstpStatus::Success) | LstpResponseBit;
    resp_hdr.len_lsb         = len & LSB_MASK;
    resp_hdr.len_msb         = len >> BYTE1_SHIFT;

    std::span<uint8_t> item(resp_buffer.data(), resp_buffer.size());
    nv::usb::Task::to_usbLstp(item);

    return LstpStatus::Success;
}

LstpStatus
LstpRouter::read_gpio_config_helper(uint16_t                                      req_offset,
                                    uint16_t                                      req_length,
                                    std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& resp_buffer,
                                    size_t&                                       resp_offset)
{
    uint16_t start_pin = 0;
    uint16_t n_pins    = 0;
    if (req_offset == 0) {
        auto& gpioConfigResp = from<LstpGpioChannelConfig>(&resp_buffer.data()[resp_offset]);
        gpioConfigResp.channel_num_gpio  = LstpGpioNum;
        resp_offset                     += sizeof(LstpGpioChannelConfig);
        start_pin                        = 0;
        if (req_length == 0) {
            n_pins = static_cast<uint16_t>(std::min(static_cast<size_t>(LstpGpioNum),
                                                    static_cast<size_t>(MaxGpiosPerPacket)));
        }
        else {
            if ((req_length - sizeof(LstpGpioChannelConfig)) % sizeof(LstpGpioConfig) != 0) {
                return LstpStatus::Error;  // length not aligned to GPIO config
            }
            n_pins = static_cast<uint16_t>((req_length - sizeof(LstpGpioChannelConfig))
                                           / sizeof(LstpGpioConfig));
        }
    }
    else {
        if ((req_offset - sizeof(LstpGpioChannelConfig)) % sizeof(LstpGpioConfig) != 0
            || req_length % sizeof(LstpGpioConfig) != 0) {
            return LstpStatus::Error;  // offset or length not aligned to GPIO config
        }
        start_pin = static_cast<uint16_t>((req_offset - sizeof(LstpGpioChannelConfig))
                                          / sizeof(LstpGpioConfig));
        n_pins    = static_cast<uint16_t>(req_length / sizeof(LstpGpioConfig));
    }
    if (n_pins > MaxGpiosPerPacket) {
        return LstpStatus::Error;
    }
    if (start_pin + n_pins > LstpGpioNum) {
        return LstpStatus::Error;
    }
    for (uint16_t i = 0; i < n_pins; i++) {
        auto& pin_config_resp  = from<LstpGpioConfig>(&resp_buffer.data()[resp_offset]);
        pin_config_resp        = PinConfigs.at(start_pin + i).config;
        resp_offset           += sizeof(LstpGpioConfig);
    }
    return LstpStatus::Success;
}

LstpStatus
LstpRouter::write_channel_config(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& req_buffer)
{
    static_assert(sizeof(LstpChannelConfigWriteRequest) + sizeof(LstpChannelConfigBlob)
                  <= LstpMaxPayloadSize);

    size_t req_offset  = 0;
    auto&  req_hdr     = from<LstpHdr>(req_buffer.data());
    req_offset        += sizeof(LstpHdr);

    const size_t req_size = static_cast<size_t>(req_hdr.len_lsb)
                          | (static_cast<size_t>(req_hdr.len_msb) << BYTE1_SHIFT);
    size_t cfg_size = 0;
    if (req_size >= sizeof(LstpChannelConfigWriteRequest)) {
        cfg_size = req_size - sizeof(LstpChannelConfigWriteRequest);
    }
    else {
        return LstpStatus::Error;
    }

    auto& channel_config_request = from<LstpChannelConfigWriteRequest>(
        &req_buffer.data()[req_offset]);
    req_offset += sizeof(LstpChannelConfigWriteRequest);

    if (channel_config_request.channel_id >= LstpNumChannels) {
        return LstpStatus::Error;
    }

    // Only supports single chunk per channel
    if (channel_config_request.offset != 0) {
        return LstpStatus::NotSupported;
    }

    // Process the channel config write
    auto& channel_config    = from<LstpChannelConfigBlob>(&req_buffer.data()[req_offset]);
    const LstpStatus status = write_channel_config_helper(
        channel_config_request.channel_id, channel_config, cfg_size);
    if (status != LstpStatus::Success) {
        return status;
    }

    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> resp_buffer{};
    auto&                                        resp_hdr = from<LstpHdr>(resp_buffer.data());

    resp_hdr.channel_id      = req_hdr.channel_id;
    resp_hdr.cmd_status_code = static_cast<uint8_t>(LstpStatus::Success) | LstpResponseBit;
    resp_hdr.len_lsb         = 0;
    resp_hdr.len_msb         = 0;

    std::span<uint8_t> item(resp_buffer.data(), resp_buffer.size());
    nv::usb::Task::to_usbLstp(item);

    return LstpStatus::Success;
}

LstpStatus LstpRouter::write_channel_config_helper(uint8_t                channel_id,
                                                   LstpChannelConfigBlob& cfg,
                                                   size_t                 cfg_size)
{
    LstpStatus status     = LstpStatus::Success;
    size_t     cfg_offset = 0;

    if ((status != LstpStatus::Success) || (cfg_offset + sizeof(cfg.channel_type) > cfg_size)) {
        return status;
    }

    // Channel type - ignore, RO field
    status = LstpStatus::Success;

    cfg_offset += sizeof(cfg.channel_type);
    if ((status != LstpStatus::Success)
        || (cfg_offset + sizeof(cfg.channel_enabled) > cfg_size)) {
        return status;
    }

    // Channel enabled
    auto& channel_state = _channels.at(channel_id);
    switch (static_cast<LstpChannelType>(cfg.channel_type)) {
        case LstpChannelType::Ipmi: {
            status = LstpStatus::NotSupported;
            if constexpr (EnableIpmi) {
                if (static_cast<bool>(cfg.channel_enabled) == true) {
                    nv::ssif::Task::enable();
                    channel_state.enabled = true;
                    status                = LstpStatus::Success;
                }
            }
            break;
        }
        default: {
            if (static_cast<bool>(cfg.channel_enabled) != channel_state.enabled) {
                status = LstpStatus::NotSupported;
            }
            break;
        }
    }

    // Subsequent fields are ignored for now
    return status;
}

/*****************************************************
 * LSTP SPI Channel
 *****************************************************/

LstpStatus LstpRouter::receive_spi(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& buffer)
{
    auto& hdr = from<LstpHdr>(buffer.data());

    if (hdr.cmd_status_code & LstpResponseBit) {
        // Don't send response to spurious response
        return LstpStatus::Success;
    }

    std::span<uint8_t> item(buffer.data(), buffer.size());
    return LstpStatus_from(nv::spi::FlashromTask::to_spi(item));
}

/*****************************************************
 * LSTP GPIO Channel
 *****************************************************/

LstpStatus LstpRouter::receive_gpio(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& req_buffer)
{
    auto& hdr = from<LstpHdr>(req_buffer.data());

    if (hdr.cmd_status_code & LstpResponseBit) {
        // Don't send response to spurious response
        return LstpStatus::Success;
    }

    if constexpr (EnableGpio) {
        return LstpTask::submit_gpio_req(req_buffer);
    }
    return LstpStatus::NotSupported;
}

void LstpRouter::send_gpio(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& resp_buffer)
{
    std::span<uint8_t> item(resp_buffer.data(), resp_buffer.size());
    nv::usb::Task::to_usbLstp(item);
}

void LstpRouter::send_gpio_irq_event(uint8_t ch_id, uint16_t gpio_index, uint8_t value)
{
    if (sizeof(LstpHdr) + sizeof(LstpGpioIrqEventRequest) > LstpMaxPayloadSize) {
        return;
    }

    // Build and send IRQ event packet
    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> irq_buffer{};

    auto&      irq_hdr      = from<LstpHdr>(irq_buffer.data());
    const auto resp_len     = sizeof(LstpGpioIrqEventRequest);
    irq_hdr.channel_id      = ch_id;
    irq_hdr.cmd_status_code = static_cast<uint8_t>(LstpGpioCommand::IrqEvent);
    irq_hdr.len_lsb         = resp_len & LSB_MASK;
    irq_hdr.len_msb         = resp_len >> BYTE1_SHIFT;

    constexpr size_t irq_offset = sizeof(LstpHdr);
    auto&            req        = from<LstpGpioIrqEventRequest>(&irq_buffer.data()[irq_offset]);
    req.gpio_index              = gpio_index;
    req.value                   = static_cast<LstpGpioState>(value);

    std::span<uint8_t> item(irq_buffer.data(), irq_buffer.size());
    nv::usb::Task::to_usbLstp(item);
}

/*****************************************************
 * LSTP I2C Channel
 *****************************************************/

LstpStatus LstpRouter::receive_i2c(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& req_buffer)
{
    auto&  req_hdr    = from<LstpHdr>(req_buffer.data());
    size_t req_offset = sizeof(LstpHdr);

    if (req_hdr.cmd_status_code & LstpResponseBit) {
        // Don't send response to spurious response
        return LstpStatus::Success;
    }

    const uint16_t payload_len = req_hdr.len_lsb | (req_hdr.len_msb << BYTE1_SHIFT);
    // TODO: Add multipacket support
    // NOLINTNEXTLINE: expected to not work if EnableLstp is false
    if (payload_len > LstpMaxPayloadSize) {
        return LstpStatus::Error;
    }

    uint8_t            address   = 0;
    uint16_t           write_len = 0;
    uint16_t           read_len  = 0;
    nv::i2c::I2cFlags  flags     = nv::i2c::I2cFlags::NoFlag;
    std::span<uint8_t> write_data;

    auto cmd = static_cast<LstpI2cCommand>(req_hdr.cmd_status_code & LstpI2cCmdMask);

    switch (cmd) {
        case LstpI2cCommand::BusRecovery: return LstpStatus::NotSupported;

        case LstpI2cCommand::Read: {
            if (req_offset + sizeof(LstpI2cReadRequest) > LstpMaxPayloadSize) {
                return LstpStatus::Error;
            }
            if (payload_len < sizeof(LstpI2cReadRequest)) {
                return LstpStatus::Error;
            }
            auto& read_req = from<LstpI2cReadRequest>(&req_buffer.data()[req_offset]);
            // req_offset     += sizeof(LstpI2cReadRequest);

            address  = read_req.address;
            read_len = read_req.read_len;
            if (read_len == 0) {
                flags = nv::i2c::I2cFlags::QuickRead;
            }
            break;
        }

        case LstpI2cCommand::Write: {
            if (req_offset + sizeof(LstpI2cWriteRequest) > LstpMaxPayloadSize) {
                return LstpStatus::Error;
            }
            // Validate payload contains at least the write request header
            if (payload_len < sizeof(LstpI2cWriteRequest)) {
                return LstpStatus::Error;
            }
            auto& write_req  = from<LstpI2cWriteRequest>(&req_buffer.data()[req_offset]);
            req_offset      += sizeof(LstpI2cWriteRequest);

            address = write_req.address;

            // Safe subtraction: validation above ensures payload_len >=
            // sizeof(LstpI2cWriteRequest)
            write_len  = static_cast<uint16_t>(payload_len + sizeof(LstpHdr) - req_offset);
            write_data = std::span<uint8_t>(&req_buffer.data()[req_offset], write_len);

            if (write_len == 0) {
                flags = nv::i2c::I2cFlags::QuickWrite;
            }
            break;
        }

        case LstpI2cCommand::ReadRecvLen: {
            if (req_offset + sizeof(LstpI2cReadRecvLenRequest) > LstpMaxPayloadSize) {
                return LstpStatus::Error;
            }
            if (payload_len < sizeof(LstpI2cReadRecvLenRequest)) {
                return LstpStatus::Error;
            }
            auto& read_recv_len_req = from<LstpI2cReadRecvLenRequest>(
                &req_buffer.data()[req_offset]);
            // req_offset += sizeof(LstpI2cReadRecvLenRequest);

            address = read_recv_len_req.address;

            // Response length determined by first byte received (SMBus block read)
            read_len = static_cast<uint16_t>(nv::ipc::UsbLstpMsgSize - sizeof(LstpHdr));
            flags    = nv::i2c::I2cFlags::RecvLen;
            break;
        }

        case LstpI2cCommand::WriteRead: {
            if (req_offset + sizeof(LstpI2cWriteReadRequest) > LstpMaxPayloadSize) {
                return LstpStatus::Error;
            }
            // Validate payload contains at least the write-read request header
            if (payload_len < sizeof(LstpI2cWriteReadRequest)) {
                return LstpStatus::Error;
            }
            auto& write_read_req = from<LstpI2cWriteReadRequest>(
                &req_buffer.data()[req_offset]);
            req_offset += sizeof(LstpI2cWriteReadRequest);

            address  = write_read_req.address;
            read_len = write_read_req.read_len;

            // Safe subtraction: validation above ensures payload_len >=
            // sizeof(LstpI2cWriteReadRequest)
            write_len  = static_cast<uint16_t>(payload_len + sizeof(LstpHdr) - req_offset);
            write_data = std::span<uint8_t>(&req_buffer.data()[req_offset], write_len);
            break;
        }

        default: return LstpStatus::NotSupported;
    }

    // NOLINTNEXTLINE: expected to not work if EnableLstp is false
    if (read_len > LstpMaxPayloadSize) {
        return LstpStatus::Error;
    }

    if (req_hdr.cmd_status_code & LstpI2cCommandFlags::NoStop) {
        flags |= nv::i2c::I2cFlags::NoStop;
    }

    auto ipchandler_id = static_cast<ipchandler::Id>(
        std::get<0>(LstpChannels.at(req_hdr.channel_id)).id);
    auto status = LstpStatus_from(nv::i2c::Task::to_i2c(
        ipchandler::Id::Lstp, address, ipchandler_id, write_len, read_len, flags, write_data));

    return status;
}

void LstpRouter::send_i2c(ipchandler::Id    src_id,
                          uint16_t          read_len,
                          ipc::Queue::Item& read_data,
                          i2c::I2cStatus    i2c_status)
{
    const LstpStatus                             status = LstpStatus_from(i2c_status);
    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> resp_buffer{};
    auto&                                        resp_hdr = from<LstpHdr>(resp_buffer.data());

    // TODO: Optimize with LUT
    uint8_t channel_id = 0;
    for (channel_id = 1; channel_id < LstpNumChannels; channel_id++) {
        const auto& channel_info = std::get<0>(LstpChannels.at(channel_id));
        if ((channel_info.type == LstpChannelType::I2c)
            && (static_cast<ipchandler::Id>(channel_info.id) == src_id)) {
            break;
        }
    }
    if (channel_id >= LstpNumChannels) {
        return;
    }

    resp_hdr.channel_id      = channel_id;
    resp_hdr.cmd_status_code = static_cast<uint8_t>(status) | LstpResponseBit;

    if (status == LstpStatus::Success) {
        // Clamp read_len to valid bounds
        const uint16_t safe_len = std::min(
            {read_len, static_cast<uint16_t>(LstpMaxPayloadSize)});

        resp_hdr.len_lsb = safe_len & LSB_MASK;
        resp_hdr.len_msb = safe_len >> BYTE1_SHIFT;

        const std::span<uint8_t> payload_data(&resp_buffer.data()[sizeof(LstpHdr)],
                                              LstpMaxPayloadSize);
        std::copy_n(read_data.begin(), safe_len, payload_data.begin());
    }
    else {
        resp_hdr.len_lsb = 0;
        resp_hdr.len_msb = 0;
    }

    std::span<uint8_t> item(resp_buffer.data(), resp_buffer.size());
    nv::usb::Task::to_usbLstp(item);
}

/*****************************************************
 * LSTP IPMI Channel
 *****************************************************/

LstpStatus LstpRouter::receive_ipmi(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& buffer)
{
    auto& hdr = from<LstpHdr>(buffer.data());

    if (hdr.cmd_status_code & LstpResponseBit) {
        // Don't send response to spurious response
        return LstpStatus::Success;
    }

    std::span<uint8_t> item(buffer.data(), buffer.size());
    (void)nv::ssif::Task::to_ssif(item);
    return LstpStatus::Success;
}

void LstpRouter::send_ipmi(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& buffer, size_t size)
{
    auto& hdr = from<LstpHdr>(buffer.data());

    hdr.channel_id      = GetFirstChannelId(LstpChannels, LstpChannelType::Ipmi);
    hdr.cmd_status_code = static_cast<uint8_t>(LstpIpmiCommand::IpmiMessage);
    hdr.len_lsb         = size & LSB_MASK;
    hdr.len_msb         = size >> BYTE1_SHIFT;

    auto item = std::span<uint8_t>(buffer);
    nv::usb::Task::to_usbLstp(item);
}

/*****************************************************
 * LSTP UART Channel
 *****************************************************/

LstpStatus LstpRouter::receive_uart(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& buffer)
{
    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> resp_buf{};
    auto&                                        req_hdr  = from<LstpHdr>(buffer.data());
    auto&                                        resp_hdr = from<LstpHdr>(resp_buf.data());

    // Flag bits ignored without error for now
    const auto cmd = static_cast<LstpUartCommand>(req_hdr.cmd_status_code & LstpUartCmdMask);

    if (req_hdr.cmd_status_code & LstpResponseBit) {
        // ACK/NACK response from the host - notify UART bridge task
        // We intentionally don't check the status and don't retry on NACK. In this
        // implementation MCU side is the console so host is expected to always ACK anyway
        // The response is only used for serialization
        if (cmd == LstpUartCommand::Write) {
            nv::vruart::LstpBridge::set_usb_tx_done_event();
        }
        else {
            // Response to non-existent request - ignore
        }
    }
    else if (cmd == LstpUartCommand::Write) {
        // USB RX data to be sent over UART
        auto& queue   = ipc::Queue::make(ipc::QueueId::UbridgeRx);
        auto  rx_item = ipc::Queue::Item(buffer.data(), buffer.size());
        if (queue.send(rx_item, 0s) != ipc::Queue::Status::Ok) {
            // Queue full - backpressure the host (will cause retry)
            return LstpStatus::Nak;
        }
        else {
            nv::vruart::LstpBridge::set_usb_rx_done_event();
        }

        // ACK response
        resp_hdr.channel_id      = req_hdr.channel_id;
        resp_hdr.cmd_status_code = static_cast<uint8_t>(LstpStatus::Success) | LstpResponseBit;
        resp_hdr.len_lsb         = 0;
        resp_hdr.len_msb         = 0;
        std::span<uint8_t> item(resp_buf.data(), resp_buf.size());
        nv::usb::Task::to_usbLstp(item);
    }
    else {
        // Non-existent request
        return LstpStatus::NotSupported;
    }

    return LstpStatus::Success;
}

bool LstpRouter::send_uart(std::span<uint8_t>& buffer)
{
    auto& hdr = from<LstpHdr>(buffer.data());

    hdr.channel_id      = GetFirstChannelId(LstpChannels, LstpChannelType::Uart);
    hdr.cmd_status_code = static_cast<uint8_t>(LstpUartCommand::Write);
    // Length is already populated by UART bridge task

    auto item = std::span<uint8_t>(buffer);
    return (nv::usb::Task::to_usbLstp(item) == usb::Status::Ok);
}

}  // namespace nv::lstp
