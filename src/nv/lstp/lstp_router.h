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

#include <array>
#include <span>

#include NV_IPC_CONFIG_H
#include "nv/ipc/queue.h"
#include "nv/uart/driver.h"
#include "nv/gpio/common.h"
#include "nv/lstp/lstp_common.h"

namespace nv::lstp {

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
    }

    return LstpStatus::Error;
}

inline LstpStatus LstpStatus_from(nv::gpio::Status status)
{
    switch (status) {
        case nv::gpio::Status::Ok          : return LstpStatus::Success;
        case nv::gpio::Status::Error       : return LstpStatus::Error;
        case nv::gpio::Status::InvalidParam: return LstpStatus::NotSupported;
    }

    return LstpStatus::Error;
}

constexpr std::array<nv::gpio::InterruptDetection, 6> LstpToGpioIrq = {
    nv::gpio::InterruptDetection::InterruptDisabled,  // IrqDisabled
    nv::gpio::InterruptDetection::InterruptRising,    // Rising
    nv::gpio::InterruptDetection::InterruptFalling,   // Falling
    nv::gpio::InterruptDetection::InterruptBothEdge,  // BothEdge
    nv::gpio::InterruptDetection::InterruptHigh,      // High
    nv::gpio::InterruptDetection::InterruptLow,       // Low
};

constexpr std::array<LstpGpioIrqConfig, 6> GpioToLstpIrq = {
    LstpGpioIrqConfig::Rising,       // InterruptRising
    LstpGpioIrqConfig::Falling,      // InterruptFalling
    LstpGpioIrqConfig::BothEdge,     // InterruptBothEdge
    LstpGpioIrqConfig::High,         // InterruptHigh
    LstpGpioIrqConfig::Low,          // InterruptLow
    LstpGpioIrqConfig::IrqDisabled,  // InterruptDisabled
};

inline nv::gpio::InterruptDetection InterruptDetection_from(LstpGpioIrqConfig config)
{
    return LstpToGpioIrq[static_cast<size_t>(config)];
}

inline LstpGpioIrqConfig LstpGpioIrqConfig_from(nv::gpio::InterruptDetection detection)
{
    return GpioToLstpIrq[static_cast<size_t>(detection)];
}

class LstpRouter
{
public:
    LstpRouter() = default;

    /**
     * @brief Initialize the LSTP router
     *
     * Initializes default enabled state of all present channels
     */
    void init();

    /**
     * @brief Receives an LSTP message and route to appropriate channel
     *
     * @param buffer The buffer with the message data (validated)
     * @return LstpStatus
     */
    LstpStatus receive(std::span<uint8_t>& buffer);

    /**
     * @brief Send an error response for a failed request
     *
     * @param buffer The original request buffer (used to read the channel ID)
     * @param status The status to report in the error response
     */
    static void send_error(std::span<uint8_t>& buffer, LstpStatus status);

    /**
     * @brief Callback from GPIO task when done
     * @param resp_buffer Populated send buffer
     */
    static void send_gpio(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& resp_buffer);

    /**
     * @brief Send GPIO IRQ event to the host
     * @param ch_id The channel ID
     * @param gpio_index The GPIO index that triggered the interrupt
     * @param value The current GPIO value
     */
    static void send_gpio_irq_event(uint8_t ch_id, uint16_t gpio_index, uint8_t value);

    /**
     * @brief Callback from I2C task with response to be sent to the USB task.
     * @param src_id The source IPCHANDLER ID
     * @param read_len The length of the read data
     * @param read_data The read data
     * @param i2c_status The I2C status
     */
    static void send_i2c(ipchandler::Id    src_id,
                         uint16_t          read_len,
                         ipc::Queue::Item& read_data,
                         i2c::I2cStatus    i2c_status);

    /**
     * @brief Sends an LSTP message to the IPMI channel
     *
     * @param buffer The buffer with the message data
     * @param size The length of the message data
     */
    static void send_ipmi(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& buffer, size_t size);

    /**
     * @brief Send a message to the UART channel
     *
     * @param buffer The buffer with the message data
     *
     * @return True if the message enqueued successfully, false otherwise
     */
    static bool send_uart(std::span<uint8_t>& buffer);

private:
    struct ChannelState
    {
        bool enabled;
    };

    std::array<ChannelState, LstpNumChannels> _channels{};

    /**
     * @brief Receive a message from the management channel (channel 0) and sends response
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    LstpStatus receive_ch0(std::span<uint8_t>& buffer);

    /**
     * @brief Read the configuration of a channel and send response
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    LstpStatus read_channel_config(std::span<uint8_t>& buffer);

    /**
     * @brief Helper to read GPIO channel config and append to response buffer
     * @param req_offset Requested offset from the read request
     * @param req_length Requested length from the read request
     * @param resp_buffer The response buffer to write to
     * @param resp_offset Current offset in response buffer (updated on success)
     * @return The status of the operation
     */
    LstpStatus
    read_gpio_config_helper(uint16_t                                      req_offset,
                            uint16_t                                      req_length,
                            std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& resp_buffer,
                            size_t&                                       resp_offset);

    /**
     * @brief Write the configuration of a channel and send response
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    LstpStatus write_channel_config(std::span<uint8_t>& buffer);

    /**
     * @brief Helper function to execute write to channel configuration
     * @param channel_id Channel ID
     * @param cfg The configuration to write
     * @param cfg_size The size of the configuration
     * @return The status of the operation
     */
    LstpStatus write_channel_config_helper(uint8_t                channel_id,
                                           LstpChannelConfigBlob& cfg,
                                           size_t                 cfg_size);

    /**
     * @brief Receive a message from the SPI channel
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    LstpStatus receive_spi(std::span<uint8_t>& buffer);

    /**
     * @brief Receive a message from the GPIO channel
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    LstpStatus receive_gpio(std::span<uint8_t>& buffer);

    /**
     * @brief Receive a message from the I2C channel
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    LstpStatus receive_i2c(std::span<uint8_t>& buffer);

    /**
     * @brief Receive a message from the IPMI channel
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    LstpStatus receive_ipmi(std::span<uint8_t>& buffer);

    /**
     * @brief Receive a message from the UART channel
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    LstpStatus receive_uart(std::span<uint8_t>& buffer);
};

}  // namespace nv::lstp
