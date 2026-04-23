/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB VCOM implementation for Core0 in dual-core mode.
 * USB hardware runs on Core1; Core0 uses IPC to communicate.
 *
 * Data flow:
 *   USB ACM RX: Core1 USB -> IPC (UsbAcm queue) -> usb_proxy task -> UbridgeRx queue -> Bridge
 * -> UART USB ACM TX: Core0 UART RX -> BridgeTask -> vcom_send() -> IPC (UbridgeTx) -> Core1
 * USB
 */

// Include config.h FIRST so USB_CONFIG_UART_BRIDGE is defined before usb.h
#include NV_IPC_CONFIG_H

#include "usb.h"

#if !defined(CORE1_BARE_METAL) && NCSI_ENABLE

#include "nv/ipc/ipc_task.h"
#include "nv/ipc/queue.h"

namespace sys::usb {

#if defined(USB_CONFIG_UART_BRIDGE)

void Driver::set_vcom_rx_callback(VcomRxCallback /* callback */)
{
    // Dual-core NCSI mode: RX data arrives via UbridgeRx queue, no callback needed
}

void Driver::set_vcom_close_callback(VcomCloseCallback /* callback */)
{
    // In dual-core mode, COM port close events are not forwarded from Core1
}

void Driver::vcom_rearm_rx(void* handle, uint8_t* buffer)
{
    (void)handle;
    (void)buffer;
    // No-op: Core1 handles USB RX re-arming
    // Backpressure is applied via IPC StreamBuffer capacity.
}

bool Driver::vcom_send(void* handle, uint8_t* data, uint32_t length)
{
    (void)handle;

    if (!data || length == 0) {
        return false;
    }

    // Send ACM TX to Core1 via IPC
    // API: returns true if busy (caller should retry), false if done.
    nv::ipc::Queue::ConstItem item(data, length);
    auto                      status = nv::ipc::task::Task::handle_queue_data(
        item, nv::ipc::QueueId::UbridgeTx, false);

    // Returns true if busy (caller retries)
    return (status != nv::ipc::task::Status::Ok);
}

void* Driver::get_vcom_handle()
{
    // Return a non-null dummy handle so vruart can function.
    // The actual USB handle is on Core1.
    static uint8_t dummy_handle = 1;
    return &dummy_handle;
}

bool Driver::is_vcom_ready()
{
    // In dual-core mode, assume USB is always ready.
    // Core1 manages USB hardware and will apply backpressure via IPC if needed.
    return true;
}

#endif  // USB_CONFIG_UART_BRIDGE

}  // namespace sys::usb

#endif  // !CORE1_BARE_METAL && NCSI_ENABLE
