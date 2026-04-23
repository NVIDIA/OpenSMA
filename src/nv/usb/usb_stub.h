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
// Stub nv::usb::Task when SYS_USB_DISABLED (USB handled by Core1 bare-metal).
// Forwards active calls to usb_proxy on Core0.
#pragma once

#include "nv/ipc/queue.h"
#include "nv/ipc/event.h"
#include "nv/ipchandler/ipchandler.h"
#include "nv/i2c/common.h"
#include <span>

#if NCSI_ENABLE
#include "nv/ipc/ipc_task.h"
#include "nv/usb_proxy/task.h"
#include "nv/logger/log.h"
#endif

namespace nv::usb {

enum class Status
{
    Ok,
    QueueSendFail,
    EventSetFail,
    Unknown
};

class Task
{
public:
    // Stub: usb_proxy::Task::make() is called directly from main.cpp
    // when NCSI_ENABLE is defined. This make() is a no-op.
    static void make() {}

    // Forward MCTP TX to Core1 via IPC when NCSI_ENABLE is defined
    static Status usb_tx(nv::ipc::Queue::Item& item)
    {
        if constexpr (nv::ipc::EnableNcsi) {
            // Forward MCTP TX data to Core1 via C2C
            if (!item.data() || item.size() == 0) {
                return Status::Ok;
            }

            // Send via ipc::task to Core1
            nv::ipc::Queue::ConstItem const_item(item.data(), item.size());
            auto                      status = nv::ipc::task::Task::handle_queue_data(
                const_item, nv::ipc::QueueId::UsbTx, false);

            if (status != nv::ipc::task::Status::Ok) {
                return Status::QueueSendFail;
            }
            return Status::Ok;
        }
        else {
            (void)item;
            return Status::Ok;
        }
    }

    static Status set_update_routing_table_event()
    {
        if constexpr (nv::ipc::EnableNcsi) {
            usb_proxy::Task::set_update_routing_table_event();
        }
        return Status::Ok;
    }
    static Status set_mctp_rx0_event() { return Status::Ok; }
    static Status set_mctp_tx_done_event() { return Status::Ok; }
    static Status set_hid_rx_event() { return Status::Ok; }
    static Status set_device_attach_event() { return Status::Ok; }
    static Status reset_all_event_bits() { return Status::Ok; }
    static Status set_spi_rx_event() { return Status::Ok; }
    static Status set_spi_tx_done_event() { return Status::Ok; }
    static Status to_usbLstp(std::span<uint8_t>&) { return Status::Ok; }
    static bool   is_lstp_device_configured() { return false; }

    static bool to_usb(nv::ipchandler::Id    src_id,
                       uint16_t              read_length,
                       nv::ipc::Queue::Item& item,
                       nv::i2c::I2cStatus    result)
    {
        if constexpr (nv::ipc::EnableNcsi) {
            return usb_proxy::to_usb_proxy_impl(src_id, read_length, item, result);
        }
        else {
            (void)src_id;
            (void)read_length;
            (void)item;
            (void)result;
            return true;
        }
    }

    static void wdt_notify()
    {
        if constexpr (nv::ipc::EnableNcsi) {
            // Notify usb_proxy::Task to feed watchdog
            auto event = nv::ipc::Event::make(nv::ipc::EventId::UsbTask);
            (void)event.set(usb_proxy::Task::WdtBit);
        }
    }
};

}  // namespace nv::usb
