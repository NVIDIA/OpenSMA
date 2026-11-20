/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nv/usb/fs_task.h"

#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)

#include <algorithm>
#include <bit>
#include <chrono>

#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "sys/ipc/supervisor.h"
#include "sys/usb/usb_fs.h"

using namespace std::chrono_literals;

namespace nv::usb {

FsTask::FsTask()
: ipc::Task(ipc::TaskId::UsbFs, "USB-FS")
, _tx(ipc::Queue::make(ipc::QueueId::UsbFsTx))
{}

void FsTask::make()
{
    NV_TASK_DATA static FsTask task;
    // leave ~312 bytes of margin above the worst-case observed stack usage
    constexpr auto StackSize = std::max(480, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&task), sizeof(FsTask));
    task.setup(stack.span(), priv, ipc::Task::Priority::Usb, FsTask::entrypoint);
}

void FsTask::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    static_cast<FsTask*>(params)->main();
}

[[noreturn]] void FsTask::main()
{
    bool tx_in_progress = false;
    auto init_status    = sys::usb::FsDriver::init(_rx_buffer.data());
    if (init_status != 0U) {
        logger::error(logger::Event::UsbFsDriverInitFailed, {init_status});
    }

    // Check space and enqueue the packet and command without interruption.
    // Both queues must be on the same core as the USB FS task.
    static_assert(ipc::get_core_from_task(ipc::TaskId::UsbFs)
                  == ipc::get_core_from_task(ipc::TaskId::Mctp));
    // Use a one-tick retry timeout while RX is held. Do not
    // self-notify continuously: MCTP has lower priority and must be able to run.
    constexpr auto RxRetryDelay = std::chrono::microseconds(
        (1'000'000U + configTICK_RATE_HZ - 1U) / configTICK_RATE_HZ);

    while (true) {
        // Consume event bits atomically. A producer setting a bit after this
        // snapshot must wake the next iteration rather than be cleared by us.
        auto notification = ipc::TaskNotify::wait(_rx_active ? RxRetryDelay : 1s, true);
        auto active_bits  = notification.has_value() ? notification.value() : 0U;

        if ((active_bits & MctpTxBit) != 0U && !tx_in_progress
            && (active_bits & (MctpTxDoneBit | AttachBit | ResetBit)) == 0U) {
            if (transmit() == 0U) {
                tx_in_progress = true;
            }
        }
        if (((active_bits & MctpRxBit) != 0U || _rx_active)
            && (active_bits & (AttachBit | ResetBit)) == 0U) {
            receive();
        }
        if ((active_bits & MctpTxDoneBit) != 0U) {
            tx_in_progress = false;
            if (_tx.size() != 0U) {
                (void)ipc::TaskNotify::send(ipc::TaskId::UsbFs, MctpTxBit);
            }
        }
        if ((active_bits & ResetBit) != 0U) {
            reset_rx();
            tx_in_progress = false;
            clear_queue();
            logger::info(logger::Event::UsbFsConnectStatus, {0U});
        }
        if ((active_bits & AttachBit) != 0U && sys::usb::FsDriver::is_device_connected()) {
            reset_rx();
            // Only this task may arm OUT, after releasing the old RX state.
            const auto status = sys::usb::FsDriver::enable_mctp_rx();
            if (status != kStatus_USB_Success) {
                logger::error(logger::Event::UsbFsMctpRecvError,
                              {static_cast<uint8_t>(status)});
            }
            logger::info(logger::Event::UsbFsConnectStatus, {1U});
            if (_tx.size() != 0U) {
                (void)ipc::TaskNotify::send(ipc::TaskId::UsbFs, MctpTxBit);
            }
            // Defer a coalesced receive until the old session has been reset.
            if ((active_bits & MctpRxBit) != 0U) {
                (void)ipc::TaskNotify::send(ipc::TaskId::UsbFs, MctpRxBit);
            }
        }
        if ((active_bits & UpdateRoutingTableBit) != 0U) {
            update_routing_table();
        }
    }
}

FsTask::MctpPacket& FsTask::from(Buffer& buffer, uint32_t offset)
{
    return *std::bit_cast<MctpPacket*>(&buffer[offset]);
}

/**
 * @brief Validate and forward complete MCTP packets from the completed USB receive.
 *
 * Parsing is bounded by the received byte count. Partial packets persist across
 * full buffers, and forwarding is limited to four packets per iteration so TX
 * can drain. Queue-full retains the packet and leaves USB OUT unarmed for retry,
 * logging once if the packet is held for at least one second.
 * Short completions discard partial state; missing completions do not rearm.
 */
void FsTask::receive()
{
    if (!_rx_active) {
        const auto received_length = sys::usb::FsDriver::take_mctp_rx_length();
        if (received_length == USB_CANCELLED_TRANSFER_LENGTH) {
            return;
        }
        if (received_length > BufferSize) {
            reset_rx();
        }
        _rx_remaining    = {_rx_buffer.data(),
                         received_length <= BufferSize ? received_length : 0U};
        _rx_transfer_end = received_length != BufferSize;
        _rx_active       = true;
    }

    // Return to the event loop between batches so TX completions get serviced.
    constexpr size_t PacketBudget = 4U;
    for (size_t i = 0U; i < PacketBudget; ++i) {
        mctp::Packet packet{};
        if (!decode_fs_mctp_stream(
                _rx_remaining, _rx_assembly, _rx_used, _rx_discard, packet)) {
            break;
        }
        packet.priv.packet_interface = static_cast<uint8_t>(mctp::Client::UsUsbFs);

        const auto downstream = std::find_if(
            _routing_table.begin(),
            _routing_table.begin() + ipc::DownStreamNum,
            [&packet](const auto& entry) {
                return entry.is_enumerated && entry.assigned_eid == packet.hdr.dst_eid;
            });
        if (downstream != _routing_table.begin() + ipc::DownStreamNum) {
            _rx_used        = 0U;
            _rx_held        = false;
            _rx_hold_logged = false;
            continue;
        }

        ipc::Queue::Item item(std::bit_cast<uint8_t*>(&packet), sizeof(packet));
        if (mctp::Driver::try_mctp_send(item, mctp::Client::UsUsbFs) != mctp::Status::Ok) {
            // Keep the assembled packet for retry while TX drains.
            const auto now = sys::ipc::get_os_ticks();
            if (!_rx_held) {
                _rx_hold_start = now;
                _rx_held       = true;
            }
            // Unsigned subtraction also handles a tick counter wrap during the hold.
            if (!_rx_hold_logged
                && static_cast<uint32_t>(now - _rx_hold_start) >= configTICK_RATE_HZ) {
                logger::error(logger::Event::UsbFsCannotSendToMctp);
                _rx_hold_logged = true;
            }
            return;
        }
        _rx_used        = 0U;
        _rx_held        = false;
        _rx_hold_logged = false;
    }
    if (!_rx_remaining.empty()) {
        return;
    }
    if (_rx_transfer_end) {
        _rx_used    = 0U;
        _rx_discard = false;
    }
    _rx_active = false;
    // Hold OUT unarmed until all packets have been forwarded. USB NAKs provide
    // host backpressure while this task drains the completion and TX queue.
    auto status = sys::usb::FsDriver::enable_mctp_rx();
    if (status != kStatus_USB_Success) {
        logger::error(logger::Event::UsbFsMctpRecvError, {static_cast<uint8_t>(status)});
    }
}

uint8_t FsTask::transmit()
{
    if (!sys::usb::FsDriver::is_device_connected()) {
        return 1U;
    }

    mctp::Packet     packet{};
    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&packet), sizeof(packet));
    auto             queue_status = _tx.recv(item, 0ms);
    if (queue_status != ipc::Queue::Status::Ok) {
        logger::error(logger::Event::UsbFsTxQueueRecvError,
                      {static_cast<uint8_t>(queue_status)});
        return static_cast<uint8_t>(queue_status);
    }

    auto&      usb_packet        = from(_tx_buffer, 0U);
    const auto usb_packet_length = static_cast<uint16_t>(packet.priv.packet_length
                                                         + sizeof(UsbMctpSpanningHeader));
    usb_packet.usb_hdr.set_packet_length(usb_packet_length);
    usb_packet.usb_hdr.dmtf_id = UsbDmtfId;
    usb_packet.mctp_hdr        = packet.hdr;
    std::copy(packet.msg.begin(), packet.msg.end(), usb_packet.msg.begin());

    auto status = sys::usb::FsDriver::write_mctp(_tx_buffer.data(), usb_packet_length);
    if (status != 0U) {
        logger::error(logger::Event::UsbFsMctpWriteError, {status});
    }
    return status;
}

void FsTask::clear_queue()
{
    mctp::Packet     packet{};
    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&packet), sizeof(packet));
    while (_tx.size() != 0U) {
        if (_tx.recv(item, 0ms) != ipc::Queue::Status::Ok) {
            break;
        }
    }
}

void FsTask::update_routing_table()
{
    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&_routing_table), sizeof(_routing_table));
    auto&            queue  = ipc::Queue::make(ipc::QueueId::UsbFsRoutingTable);
    auto             status = queue.recv(item, 100ms);
    if (status != ipc::Queue::Status::Ok) {
        logger::error(logger::Event::UsbFsUpdateRoutingTableFailed,
                      {static_cast<uint8_t>(status)});
    }
}

nv::usb::Status FsTask::usb_tx(ipc::Queue::Item& item)
{
    auto& queue = ipc::Queue::make(ipc::QueueId::UsbFsTx);
    if (queue.send(item, 100ms) != ipc::Queue::Status::Ok) {
        return nv::usb::Status::QueueSendFail;
    }
    if (ipc::TaskNotify::send(ipc::TaskId::UsbFs, MctpTxBit) != ipc::TaskNotify::Status::Ok) {
        return nv::usb::Status::EventSetFail;
    }
    return nv::usb::Status::Ok;
}

void FsTask::reset_rx()
{
    _rx_used        = 0U;
    _rx_discard     = false;
    _rx_remaining   = {};
    _rx_active      = false;
    _rx_hold_start  = 0U;
    _rx_held        = false;
    _rx_hold_logged = false;
}

#define USB_FS_EVENT_METHOD(method, bit)                                                       \
    nv::usb::Status FsTask::method()                                                           \
    {                                                                                          \
        return ipc::TaskNotify::send(ipc::TaskId::UsbFs, bit) == ipc::TaskNotify::Status::Ok   \
                 ? nv::usb::Status::Ok                                                         \
                 : nv::usb::Status::EventSetFail;                                              \
    }

USB_FS_EVENT_METHOD(set_mctp_rx_event, MctpRxBit)
USB_FS_EVENT_METHOD(set_mctp_tx_done_event, MctpTxDoneBit)
USB_FS_EVENT_METHOD(set_device_attach_event, AttachBit)
USB_FS_EVENT_METHOD(set_update_routing_table_event, UpdateRoutingTableBit)
USB_FS_EVENT_METHOD(reset_events, ResetBit)

#undef USB_FS_EVENT_METHOD

}  // namespace nv::usb

#endif
