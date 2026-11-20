/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>

#include "sys/usb/usb_device_config.h"
#include "usb_device.h"

namespace sys::usb {

class FsDriver
{
public:
    /**
     * @brief Initialize and start the full-speed USB MCTP device.
     * @param rx_buffer Persistent receive buffer of USB_MCTP_OUT_BUFFER_LENGTH bytes.
     * @return USB status code, or 1 when full-speed MCTP is disabled.
     */
    static uint8_t init(uint8_t* rx_buffer);

    /**
     * @brief Submit an asynchronous MCTP transmit request.
     * @param data Buffer that remains valid until transmit completion.
     * @param length Number of bytes to send.
     * @return USB status code, or 1 when full-speed MCTP is disabled.
     */
    static uint8_t write_mctp(uint8_t* data, uint32_t length);

    /**
     * @brief Atomically consume the most recent completed receive byte count.
     * @return Received length, or USB_CANCELLED_TRANSFER_LENGTH if none is pending.
     */
    static uint32_t take_mctp_rx_length();

    /**
     * @brief Rearm MCTP reception after the USB FS task releases the receive buffer.
     * Already armed, pending-completion, and disconnected states are no-ops.
     * @return USB status code, or kStatus_USB_Error when full-speed MCTP is disabled.
     */
    static usb_status_t enable_mctp_rx();

    /**
     * @brief Check whether the full-speed device is attached and configured.
     * @return True when connected; false when disconnected or MCTP is disabled.
     */
    static bool is_device_connected();
};

}  // namespace sys::usb
