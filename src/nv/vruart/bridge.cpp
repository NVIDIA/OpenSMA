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
#if __has_include("sys/ipc/mcmgr_wrapper.h")
#include "nv/ipc/common.h"
#include "sys/ipc/mcmgr_wrapper.h"
#endif

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

    if constexpr (ipc::EnableNcsi) {
        // Dual-core mode: Copy data immediately to avoid race condition
        // In dual-core, 'data' may point to shared buffer that can be overwritten
        if (length > Bridge::inst().rx_buf.size()) {
            length = Bridge::inst().rx_buf.size();
        }
        std::memcpy(Bridge::inst().rx_buf.data(), data, length);
        Bridge::inst().pending_usb_data = Bridge::inst().rx_buf.data();
        Bridge::inst().pending_usb_len  = length;

        // Re-arm immediately in dual-core mode (no backpressure from USB side)
        sys::usb::Driver::vcom_rearm_rx(sys::usb::Driver::get_vcom_handle(), data);
    }
    else {
        // Single-core mode: Save pointer, don't re-arm USB (provides backpressure)
        Bridge::inst().pending_usb_data = data;
        Bridge::inst().pending_usb_len  = length;
    }

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

    // Length ≤ 512 (enforced by eDMA config)
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
    auto&            tx_queue = Queue::make(QueueId::UbridgeTx);
    auto&            event    = Event::make(EventId::UartBridgeEvent);
    ipc::Queue::Item tx_item(std::bit_cast<uint8_t*>(&tx_buf), tx_buf.size());

    constexpr uint32_t WaitBits = UsbRxDoneBit | UartRxDoneBit | UartTxDoneBit | UsbTxDoneBit;

    while (true) {
        // 100ms timeout
        auto bits = event.wait(WaitBits, false, false, 100ms);
        if (!bits.has_value()) {
            continue;  // Timeout, loop back
        }
        auto active = bits.value() & WaitBits;

        // Handle USB RX -> UART TX
        if (active & UsbRxDoneBit) {
            event.clear(UsbRxDoneBit);

            if constexpr (ipc::EnableNcsi) {
                // Dual-core: drain UbridgeRx queue (data from usb_proxy task)
                // Format: [2-byte length LE][data]
                auto&            rx_queue = Queue::make(QueueId::UbridgeRx);
                ipc::Queue::Item rx_item(rx_buf.data(), rx_buf.size());
                while (rx_queue.recv(rx_item, 0ms) == Queue::Status::Ok) {
                    uint16_t len = 0;
                    std::memcpy(&len, rx_buf.data(), sizeof(len));
                    if (len > 0 && len <= rx_buf.size() - 2) {
                        tx(std::span<uint8_t>(rx_buf.data() + 2, len));
                    }
                }
            }
            else {
                // Single-core: data comes via callback -> pending_usb_data
                tx(std::span<uint8_t>(
                    const_cast<uint8_t*>(pending_usb_data),  // NOLINT(*-const-cast)
                    pending_usb_len));
            }
        }

        // Handle UART TX complete -> re-arm USB RX
        if (active & UartTxDoneBit) {
            event.clear(UartTxDoneBit);

            if constexpr (ipc::EnableNcsi) {
                // Dual-core: notify Core1 so it can re-arm ACM receive
                (void)sys::ipc::task::Mcmgr::trigger_event_force(
                    nv::ipc::CoreId::Core1,
                    nv::ipc::task::EventType::Communication,
                    static_cast<uint16_t>(nv::ipc::task::CmdCode::InterCoreAcmTxDone));
            }
            else {
                // Single-core: Re-arm USB RX to receive next packet
                auto* data = const_cast<uint8_t*>(pending_usb_data);  // NOLINT(*-const-cast)
                pending_usb_data = nullptr;
                pending_usb_len  = 0;

                sys::usb::Driver::vcom_rearm_rx(sys::usb::Driver::get_vcom_handle(), data);
            }
        }

        // Handle UART RX -> USB TX from queue
        if (active & UartRxDoneBit) {
            event.clear(UartRxDoneBit);

            // Drain queue: USB TX is fast, bounded retry if busy
            void* handle = sys::usb::Driver::get_vcom_handle();
            while (tx_queue.recv(tx_item, 0ms) == Queue::Status::Ok) {
                uint16_t len = 0;
                std::memcpy(&len, tx_buf.data(), sizeof(len));  // Little-endian

                // Bounded retry for USB TX - avoid indefinite blocking if USB fails
                constexpr int MaxRetries = 100;
                for (int retry = 0; retry < MaxRetries; ++retry) {
                    if (sys::usb::Driver::vcom_send(handle, tx_buf.data() + 2, len) == 0) {
                        break;  // Success
                    }
                    constexpr uint32_t RetryDelayUs = 20;
                    nv::ctimer::Driver::delay_for_us(RetryDelayUs);
                }
            }
        }

        // UsbTxDoneBit: intentionally not used
        if (active & UsbTxDoneBit) {
            event.clear(UsbTxDoneBit);
        }
    }
}

}  // namespace nv::vruart
