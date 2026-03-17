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

#include <cstdint>
#include <cstring>

#include "nv/vruart/bridge.h"
#include "sys/uart/bridge.h"

static_assert(nv::vruart::Bridge::Buffsz == 2U + sys::uart::edmaXferBufSize,
              "Buffer size must be 2 + UART RX eDMA payload size");

#include "nv/common/preproc.h"
#include "nv/ctimer/ctimer.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/event.h"
#include "nv/ipc/supervisor.h"

#include "sys/usb/usb.h"

using namespace nv::ipc;
using namespace std::chrono_literals;

#include NV_IPC_CONFIG_H

namespace nv::vruart {

// Global instances (single core access only, no need for NV_SHARED)
Bridge bridge;  // NOLINT(*-non-const-global-variables)
static sys::uart::Bridge
    uart_impl;  // NOLINT(*-non-const-global-variables,misc-use-anonymous-namespace)

Bridge& Bridge::inst()
{
    return bridge;
}

// UART interface - forward to uart_impl
Status Bridge::init(Instance      uartInstance,
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

Status Bridge::tx(std::span<uint8_t> data)
{
    auto status = uart_impl.tx(data);
    return (status == sys::uart::Status::Ok) ? Status::Ok : Status::TxFail;
}

bool Bridge::ready() const
{
    return uart_impl.ready();
}

uint8_t Bridge::usb_rx_callback(uint8_t* data, uint32_t length)
{
    if (length == 0) {
        // ZLP - just re-arm
        sys::usb::Driver::vcom_rearm_rx(sys::usb::Driver::get_vcom_handle(), data);
        return 0;
    }

    // Strict backpressure: save pointer, don't re-arm USB (NAK host)
    Bridge::inst().pending_usb_data = data;
    Bridge::inst().pending_usb_len  = length;
    // volatile ensures writes complete; FreeRTOS event has internal barrier

    // Notify task
    set_usb_rx_done_event();

    return 0;
}

// Event setters (called from ISR or task context)
// Event::set() automatically handles ISR context via xPortIsInsideInterrupt()
void Bridge::set_usb_rx_done_event()
{
    auto& event = Event::make(EventId::UartBridgeEvent);
    (void)event.set(UsbRxDoneBit);
}

void Bridge::set_uart_rx_done_event()
{
    auto& event = Event::make(EventId::UartBridgeEvent);
    (void)event.set(UartRxDoneBit);
}

void Bridge::set_uart_tx_done_event()
{
    auto& event = Event::make(EventId::UartBridgeEvent);
    (void)event.set(UartTxDoneBit);
}

// Flush pending TX queue (called when USB port is closed to avoid stale data)
void Bridge::flush_tx_queue()
{
    auto&            queue = Queue::make(QueueId::UbridgeTx);
    ipc::Queue::Item item;
    // Drain all pending items from the queue
    while (queue.recv(item, 0ms) == Queue::Status::Ok) {
        // Discard
    }
}

// Enqueue UART RX data for USB TX (called from ISR)
uint8_t Bridge::enqueue(const uint8_t* data, uint32_t length)
{
    if (data == nullptr || length == 0) {
        return 1;
    }

    if (!sys::usb::Driver::is_vcom_ready()) {
        return 0;  // USB not ready, silently drop
    }

    /*
     * Buffer format: [2 bytes length (little-endian)] + [up to 512 bytes data]
     *
     * Note: length is guaranteed <= 512 by caller (LPUART_UserCallback), which passes
     * data from eDMA receive buffer (edmaXferBufSize = 512). No explicit bounds check
     * needed here - the hardware/eDMA configuration enforces the limit.
     */
    Buffer buf{};
    std::memcpy(buf.data() + 2, data, length);
    std::memcpy(buf.data(), &length, sizeof(uint16_t));

    auto&                       queue = Queue::make(QueueId::UbridgeTx);
    const ipc::Queue::ConstItem item(buf.data(), buf.size());

    // Note: Both send_isr() and set_uart_rx_done_event() call portYIELD_FROM_ISR(),
    // but only the event triggers a context switch since no task blocks on the queue
    // (Bridge task uses non-blocking queue.recv(0ms)).
    // Retry a few times if queue full - USB TX is fast (~10µs), queue may free up quickly
    Queue::Status err = Queue::Status::Full;
    for (int retry = 0; retry < 3; ++retry) {
        err = queue.send_isr(item);
        if (err == Queue::Status::Ok) {
            break;
        }
        // Brief delay before retry (in ISR, can't block)
        nv::ctimer::Driver::delay_for_us(5);  // ~5µs delay between retries
    }
    if (err != Queue::Status::Ok) {
        // Queue still full after retries. No flow control (only TX/RX lines, no RTS/CTS),
        // so backpressure to UART sender is impossible - data loss is inevitable.
        return 1;
    }
    set_uart_rx_done_event();

    return 0;
}

// Event-driven task loop
[[noreturn]] void Bridge::main()
{
    auto&            queue = Queue::make(QueueId::UbridgeTx);
    auto&            event = Event::make(EventId::UartBridgeEvent);
    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&tx_buf), tx_buf.size());

    constexpr uint32_t WaitBits = UsbRxDoneBit | UartRxDoneBit | UartTxDoneBit | UsbTxDoneBit;

    while (true) {
        /*
         * Wait for any event (block until event occurs)
         *
         * Note: Using Event Group instead of Task Notification for:
         *   1. Architecture consistency - nv/ layer should not call FreeRTOS directly
         *   2. Dual-core compatibility - Task Notification doesn't work cross-core
         *   3. EventId abstraction - decouples sender from receiver
         *
         * Trade-off: Event Group from ISR requires 2 context switches (via daemon task)
         * vs Task Notification's 1 switch. No issues observed so far.
         */
        auto bits   = event.wait(WaitBits, false, false);
        auto active = bits.value() & WaitBits;

        // Handle USB RX → UART TX
        // Note: No need to check pending_usb_data/len validity:
        // - UsbRxDoneBit is only set after usb_recv() saves valid data
        // - USB RX is only re-armed after UART TX completes, so no overlap
        if (active & UsbRxDoneBit) {
            event.clear(UsbRxDoneBit);
            // FreeRTOS event.wait() guarantees visibility of ISR writes
            // NOLINT justified: volatile→non-volatile, ISR won't overlap (USB re-arm gate)
            tx(std::span<uint8_t>(
                const_cast<uint8_t*>(pending_usb_data),  // NOLINT(*-const-cast)
                pending_usb_len));
        }

        // Handle UART TX complete → re-arm USB RX
        // Note: No need to check uart_tx_pending - UartTxDoneBit is only set when TX completes
        if (active & UartTxDoneBit) {
            event.clear(UartTxDoneBit);

            auto* data       = const_cast<uint8_t*>(pending_usb_data);  // NOLINT(*-const-cast)
            pending_usb_data = nullptr;
            pending_usb_len  = 0;
            // vcom_rearm_rx() is the gate - ISR won't fire until called

            // Re-arm USB RX
            sys::usb::Driver::vcom_rearm_rx(sys::usb::Driver::get_vcom_handle(), data);
        }

        // Handle UART RX → USB TX from queue
        if (active & UartRxDoneBit) {
            event.clear(UartRxDoneBit);

            // Drain queue: USB TX is fast, bounded retry if busy
            // Note: len is guaranteed valid (0 < len <= 512) - see enqueue() comment for
            // details
            void* handle = sys::usb::Driver::get_vcom_handle();
            while (queue.recv(item, 0ms) == Queue::Status::Ok) {
                uint16_t len = 0;
                std::memcpy(&len, tx_buf.data(), sizeof(len));  // Little-endian

                // Bounded retry for USB TX - avoid indefinite blocking if USB fails
                // Max wait: 100 * 20µs = 2ms per packet (USB HS TX is ~10µs for 512B)
                constexpr int MaxRetries = 100;
                for (int retry = 0; retry < MaxRetries; ++retry) {
                    if (sys::usb::Driver::vcom_send(handle, tx_buf.data() + 2, len) == 0) {
                        break;  // Success
                    }
                    constexpr uint32_t RetryDelayUs = 20;
                    nv::ctimer::Driver::delay_for_us(RetryDelayUs);
                }
                // If still failing after MaxRetries, data is dropped - USB may be disconnected
            }
        }

        // UsbTxDoneBit: intentionally not used - no code sets this event.
        // USB HS TX is very fast (~10µs for 512B at 480Mbps), spin-wait above
        // is more efficient than context switch overhead (~10-50µs).
        if (active & UsbTxDoneBit) {
            event.clear(UsbTxDoneBit);  // Dead code, kept for completeness
        }
    }
}

}  // namespace nv::vruart

// Fixed hooks consumed by sys::usb (no runtime callback registration)
extern "C" uint8_t nv_usb_vcom_rx(uint8_t* data, uint32_t length)
{
    return nv::vruart::Bridge::usb_rx_callback(data, length);
}

extern "C" void nv_usb_vcom_close()
{
    nv::vruart::Bridge::flush_tx_queue();
}
