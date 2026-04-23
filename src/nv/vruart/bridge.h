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

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "nv/vruart/common.h"

namespace nv::vruart {

/**
 * Bridge class - UART to USB CDC ACM bridge
 * Handles bidirectional data transfer between UART and USB virtual COM port
 *
 * Data flow (event-driven, single task):
 *   USB RX → UsbRxDoneBit → Task → UART TX → re-arm USB
 *   UART RX → enqueue + UartRxDoneBit → Task → USB TX
 */
class Bridge
{
public:
    static constexpr uint32_t Buffsz = 514;  // 2 bytes length (little-endian) + 512 bytes data
    using Buffer                     = std::array<uint8_t, Buffsz>;

    // Event bits for task notification
    enum EventBits : uint32_t
    {
        UsbRxDoneBit  = 1U << 0,  // USB RX complete, need to send to UART
        UartRxDoneBit = 1U << 1,  // UART RX complete (in queue), need to send to USB
        UartTxDoneBit = 1U << 2,  // UART TX complete, can re-arm USB RX
        UsbTxDoneBit  = 1U << 3,  // USB TX complete (not used, see main loop comment)
    };

    // UART initialization
    Status init(Instance      uartInstance,
                const Signal& tx,
                const Signal& rx,
                Baudrate      baudrate,
                EdmaInst      edmaInstance,
                EdmaChn       edmaTxChn,
                EdmaChn       edmaRxChn);

    // UART transmit
    Status tx(std::span<uint8_t> data);

    // UART ready check
    bool ready() const;

    // Get singleton instance
    static Bridge& inst();

    // Enqueue UART RX data for USB TX (called from ISR)
    static uint8_t enqueue(const uint8_t* data, uint32_t length);

    // USB CDC ACM recv callback (registered with sys::usb::Driver)
    static uint8_t usb_rx_callback(uint8_t* data, uint32_t length);

    // Event setters (called from ISR)
    static void set_usb_rx_done_event();
    static void set_uart_rx_done_event();
    static void set_uart_tx_done_event();

    // Flush pending TX queue (called when USB port is closed)
    static void flush_tx_queue();

    // Single task entry point (event-driven loop)
    [[noreturn]] void main();

private:
    // Single-core: USB RX callback saves pointer here
    volatile uint8_t* pending_usb_data = nullptr;
    volatile uint32_t pending_usb_len  = 0;

    // RX buffer: single-core callback copy / dual-core queue recv
    Buffer rx_buf{};

    // TX buffer for UbridgeTx queue receive
    Buffer tx_buf{};
};

extern Bridge bridge;

}  // namespace nv::vruart
