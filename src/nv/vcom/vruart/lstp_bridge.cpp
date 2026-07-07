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

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "nv/vcom/vruart/lstp_bridge.h"
#include "sys/uart/bridge.h"

#include "nv/common/preproc.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/event.h"
#include "nv/ipc/supervisor.h"
#include "nv/ctimer/ctimer.h"
#include "nv/lstp/lstp_router.h"
#include "sys/ctimer/ctimer.h"

using namespace nv::ipc;
using namespace std::chrono_literals;

#include NV_IPC_CONFIG_H

namespace nv::vruart {

// Global instances (single core access only, no need for NV_SHARED)
LstpBridge lstp_bridge;  // NOLINT(*-non-const-global-variables)
static sys::uart::Bridge
    uart_impl;  // NOLINT(*-non-const-global-variables,misc-use-anonymous-namespace)

LstpBridge& LstpBridge::inst()
{
    return lstp_bridge;
}

// UART interface - forward to uart_impl
Status LstpBridge::init(Instance      uartInstance,
                        const Signal& tx,
                        const Signal& rx,
                        Baudrate      baudrate,
                        EdmaInst      edmaInstance,
                        EdmaChn       edmaTxChn,
                        EdmaChn       edmaRxChn)
{
    auto status = uart_impl.init(
        uartInstance, tx, rx, baudrate, edmaInstance, edmaTxChn, edmaRxChn);
    return (status == sys::uart::Status::Ok) ? Status::Ok : Status::NotInit;
}

Status LstpBridge::tx(std::span<uint8_t> data)
{
    auto status = uart_impl.tx(data);
    return (status == sys::uart::Status::Ok) ? Status::Ok : Status::TxFail;
}

bool LstpBridge::ready() const
{
    return uart_impl.ready();
}

// Event setters (called from ISR or task context)
// Event::set() automatically handles ISR context via xPortIsInsideInterrupt()
void LstpBridge::set_usb_rx_done_event()
{
    auto& event = Event::make(EventId::UartBridgeEvent);
    (void)event.set(UsbRxDoneBit);
}

void LstpBridge::set_uart_rx_done_event()
{
    auto& event = Event::make(EventId::UartBridgeEvent);
    (void)event.set(UartRxDoneBit);
}

void LstpBridge::set_uart_tx_done_event()
{
    auto& event = Event::make(EventId::UartBridgeEvent);
    (void)event.set(UartTxDoneBit);
}

void LstpBridge::set_usb_tx_done_event()
{
    auto& event = Event::make(EventId::UartBridgeEvent);
    (void)event.set(UsbTxDoneBit);
}

// Enqueue UART RX data for USB TX (called from ISR)
uint8_t LstpBridge::enqueue(const uint8_t* data, uint32_t length)
{
    if (data == nullptr || length == 0) {
        return 1;
    }

    // Length ≤ edmaXferBufSize (508) - enforced by eDMA config
    Buffer     buf{};
    const auto len = static_cast<uint16_t>(length);
    std::memcpy(buf.data() + offsetof(nv::lstp::LstpHdr, len_lsb), &len, sizeof(len));  // LE
    std::memcpy(buf.data() + sizeof(nv::lstp::LstpHdr), data, length);

    auto&                       queue = Queue::make(QueueId::UbridgeTx);
    const ipc::Queue::ConstItem item(buf.data(), buf.size());

    // Note: Both send_isr() and set_uart_rx_done_event() call portYIELD_FROM_ISR(),
    // but only the event triggers a context switch since no task blocks on the queue
    // (Bridge task uses non-blocking queue.recv(0ms)).
    if (queue.send_isr(item) != Queue::Status::Ok) {
        return 1;
    }
    set_uart_rx_done_event();

    return 0;
}

// Event-driven task loop
[[noreturn]] void LstpBridge::main()
{
    auto&            tx_queue = Queue::make(QueueId::UbridgeTx);
    auto&            rx_queue = Queue::make(QueueId::UbridgeRx);
    auto&            event    = Event::make(EventId::UartBridgeEvent);
    ipc::Queue::Item tx_item(tx_buf.data(), tx_buf.size());
    ipc::Queue::Item rx_item(rx_buf.data(), rx_buf.size());

    bool               waiting_uart_tx = false;
    bool               waiting_usb_tx  = false;
    sys::ctimer::Ticks usb_tx_timer    = 0;

    constexpr uint32_t WaitBits = UsbRxDoneBit | UartRxDoneBit | UartTxDoneBit | UsbTxDoneBit;

    while (true) {
        // 100ms timeout
        auto     bits   = event.wait(WaitBits, false, false, 100ms);
        uint32_t active = 0U;  // Continue on timeout to check usb_tx_timeout
        if (bits.has_value()) {
            active = bits.value() & WaitBits;
        }

        // Handle USB RX -> UART TX
        if (active & UsbRxDoneBit) {
            event.clear(UsbRxDoneBit);

            // Data comes via UbridgeRx queue from lstp_router
            // Dequeue one item per iteration, otherwise we will interrupt current TX
            // UsbRxDoneBit will be set on UART TX done if there is data in UbridgeRx queue
            // If tx fails, USB RX data is discarded
            if (!waiting_uart_tx && rx_queue.recv(rx_item, 0ms) == Queue::Status::Ok) {
                uint16_t len = 0;
                std::memcpy(&len,
                            rx_buf.data() + offsetof(nv::lstp::LstpHdr, len_lsb),
                            sizeof(len));  // LE
                if ((len > 0) && (len <= rx_buf.size() - sizeof(nv::lstp::LstpHdr))
                    && (tx(std::span<uint8_t>(rx_buf.data() + sizeof(nv::lstp::LstpHdr), len))
                        == Status::Ok)) {
                    waiting_uart_tx = true;
                }
            }
        }

        // Handle UART TX complete
        if (active & UartTxDoneBit) {
            event.clear(UartTxDoneBit);
            waiting_uart_tx = false;

            if (rx_queue.size() > 0) {
                // Queue not drained, transfer next item in subsequent iteration
                set_usb_rx_done_event();
            }
        }

        // Handle UART RX -> USB TX from queue
        if (active & UartRxDoneBit) {
            event.clear(UartRxDoneBit);

            // LSTP cannot rely on USB for flow control - wait for explicit host ACK packet
            // UartRxDoneBit will be set again on USB TX done if there is data in UbridgeTx
            // If send_uart fails, UART RX data is discarded
            if (!waiting_usb_tx && (tx_queue.recv(tx_item, 0ms) == Queue::Status::Ok)
                && (nv::lstp::LstpRouter::send_uart(tx_item) == true)) {
                waiting_usb_tx = true;
                usb_tx_timer   = nv::ctimer::Driver::read_ticks();
            }
        }

        // USB TX done (ACK by host) - LSTP serialization
        // Unblock USB TX on 100ms timeout in case of misbehaving host
        const bool usb_tx_timeout = waiting_usb_tx
                                 && (sys::ctimer::Driver::get_counter_difference(
                                         usb_tx_timer, nv::ctimer::Driver::read_ticks())
                                     > UsbTxTimeoutUs);
        if ((active & UsbTxDoneBit) || usb_tx_timeout) {
            event.clear(UsbTxDoneBit);
            waiting_usb_tx = false;

            if (tx_queue.size() > 0) {
                // Queue not drained, transfer next item in subsequent iteration
                set_uart_rx_done_event();
            }
        }
    }
}

}  // namespace nv::vruart
