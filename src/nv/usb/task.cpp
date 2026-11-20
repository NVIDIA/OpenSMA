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

#include "nv/usb/task.h"

// Only compile full implementation when USB is not disabled
// When SYS_USB_DISABLED == 1, task.h provides stub implementation
#if !SYS_USB_DISABLED

#include <algorithm>
#include <atomic>
#include <chrono>

#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"
#include "nv/perf_mon/perf_mon.h"
#include "sys/usb/usb.h"
#include "nv/spi/task.h"
#include "nv/usb/mctp_router.h"
#include "nv/lstp/lstp_parser.h"

#include "nv/watchdog/runtime.h"
#include "nv/spi/flashrom_task.h"

using namespace std::chrono_literals;
using namespace nv::ipc;
using namespace nv;

namespace nv::usb {

void Task::make()
{
    NV_TASK_DATA static Task task;
    // leave ~312 bytes of margin above the worst-case observed stack usage
    constexpr auto StackSize = std::max(640, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Usb, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    using namespace std::chrono_literals;
    auto& task = *static_cast<Task*>(params);
    task.main();
}

Task::Task() : ipc::Task(ipc::TaskId::Usb, "USB"), _tx(ipc::Queue::make(ipc::QueueId::UsbTx)) {}

[[noreturn]] void Task::main()
{
    // A pending TX bit must be re-notified because waiting consumes its wake state.
    bool mctp_tx_in_progress = false;
    bool lstp_tx_in_progress = false;

    uint32_t init_status = 0;
    if constexpr (nv::ipc::EnableLstp) {
        init_status = _driver.init(mctp_rx_buffer.data(),
                                   mctp_rx_buffer.data(),
                                   hid_rx_buffer.data(),
                                   hid_rx_buffer.data(),
                                   hid_ep0_buffer.data(),
                                   lstp_rx_buffer.data(),
                                   lstp_tx_buffer.data(),
                                   &lstp_rx_len);
    }
    else {
        init_status = _driver.init(mctp_rx_buffer.data(),
                                   mctp_rx_buffer.data(),
                                   hid_rx_buffer.data(),
                                   hid_rx_buffer.data(),
                                   hid_ep0_buffer.data());
    }

    if (init_status != 0) {
        logger::error(logger::Event::UsbDriverInitFailed, {static_cast<uint8_t>(init_status)});
    }

    was_vbus_on = _driver.check_vbus();

    if constexpr (nv::ipc::EnableLstp) {
        _lstp_router.init();
    }

    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Usb);

    while (true) {
        auto    notification = ipc::TaskNotify::wait(1s);
        auto    active_bits  = notification.has_value() ? notification.value() : 0U;
        uint8_t tx_result    = 0;

        if (active_bits & WdtEventBit) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, WdtEventBit);
            nv::watchdog::Runtime::mark_task_alive(nv::watchdog::TaskMonitorIndex::Usb);
        }
        /* MctpTxBit */
        if ((active_bits & MctpTxBit) && !mctp_tx_in_progress) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, MctpTxBit);
            tx_result = transmit();

            if (tx_result == 0) {
                mctp_tx_in_progress = true;
            }
        }

        /* MctpRxBit */
        if (active_bits & MctpRxBit) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, MctpRxBit);
            mctp_receive();
        }

#if (SYS_USB_HID_CP2112 > 0U)
        /* HidRxBit */
        if (active_bits & HidRxBit) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, HidRxBit);
            hid_receive();
        }
#endif

        /* LSTP event handling */
        if constexpr (nv::ipc::EnableLstp) {
            /* LstpRxBit */
            if (active_bits & LstpRxBit) {
                (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, LstpRxBit);
                lstp_receive();
            }

            /* LstpTxBit */
            if ((active_bits & LstpTxBit) && !lstp_tx_in_progress) {
                (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, LstpTxBit);
                tx_result = lstp_transmit();

                if (tx_result == 0) {
                    lstp_tx_in_progress = true;
                }
                else {
                    // Transmission failed. Re-set
                    // LstpTxBit if more items are pending so the
                    // queue eventually drains once USB attaches.
                    auto& queue = ipc::Queue::make(ipc::QueueId::LstpTx);
                    if (queue.size() != 0) {
                        (void)ipc::TaskNotify::send(ipc::TaskId::Usb, EventBits::LstpTxBit);
                    }
                }
            }

            /* LstpTxDoneBit */
            if (active_bits & LstpTxDoneBit) {
                (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, LstpTxDoneBit);
                lstp_tx_in_progress = false;

                auto& queue = ipc::Queue::make(ipc::QueueId::LstpTx);
                if (queue.size() != 0) {
                    (void)ipc::TaskNotify::send(ipc::TaskId::Usb, EventBits::LstpTxBit);
                }
            }
        }

        /* MctpTxDoneBit */
        if (active_bits & MctpTxDoneBit) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, MctpTxDoneBit);
            mctp_tx_in_progress = false;
            loop_count          = 0;

            if (_tx.size() != 0) {
                (void)ipc::TaskNotify::send(ipc::TaskId::Usb, MctpTxBit);
            }
        }

        /* AttachBit */
        if (active_bits & AttachBit) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, AttachBit);
            logger::info(logger::Event::UsbConnectStatus, {1});
        }

        /* ResetBit */
        if (active_bits & ResetBit) {
            (void)ipc::TaskNotify::reset(ipc::TaskId::Usb);
            // Queued and in-flight TX data belongs to the previous host session. The reset
            // above also dropped any pending TX bit, so a full queue would never drain.
            mctp_tx_in_progress = false;
            clear_queue();
            if constexpr (nv::ipc::EnableLstp) {
                lstp_tx_in_progress = false;
                clear_usb_lstp_queue();
            }
        }

        /* UpdateRoutingTableBit */
        if (active_bits & UpdateRoutingTableBit) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, UpdateRoutingTableBit);
            update_routing_table();
        }

        /* vbus detach check */
        check_vbus_status();

        /* clear MctpTxDoneBit if it still has been set after one loop */
        check_tx_done_status(active_bits);
    }
}

bool Task::check_vbus_status()
{
    was_vbus_on = is_vbus_on;
    is_vbus_on  = _driver.check_vbus();

    if (was_vbus_on && !is_vbus_on) {
        logger::info(logger::Event::UsbConnectStatus, {0});
    }

    return is_vbus_on;
}

void Task::check_tx_done_status(uint32_t active_bits)
{
    if (active_bits & MctpTxDoneBit) {
        if (loop_count >= 1) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, MctpTxDoneBit);
            loop_count = 0;
        }
        else {
            loop_count++;
        }
    }
}

void Task::recover_lstp_endpoint()
{
    if constexpr (nv::ipc::EnableLstp) {
        // Clear the stuck endpoint
        _driver.recover_spi_endpoint();
    }
}
void Task::mctp_receive()
{
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::UsbHandleRxIsr);
    auto offset = 0U;

    // handle multi packet in single usb packet
    while (offset + sizeof(MctpPacket::usb_hdr) <= BufferSize) {
        auto& usb_pkt = from(mctp_rx_buffer, offset);
        if (usb_pkt.usb_hdr.dmtf_id != UsbDmtfId
            || usb_pkt.usb_hdr.length < sizeof(MctpPacket::usb_hdr)
            || usb_pkt.usb_hdr.length > sizeof(MctpPacket)) {
            break;
        }
        else {
            auto is_drop = false;

            mctp::Packet pkt{};
            // update offset in usb buffer
            offset += usb_pkt.usb_hdr.length;

            // Currently packets exceeding buffer size are dropped
            // Future implementation should handle mutli-packet for large data
            if (offset > BufferSize) {
                auto overflow_size = offset - BufferSize;
                logger::error(logger::Event::UsbBufferOverflow,
                              {static_cast<uint8_t>(overflow_size),
                               static_cast<uint8_t>(usb_pkt.usb_hdr.length)});
                // Set offset to buffer size to clear the entire buffer
                offset = BufferSize;
                break;
            }

            // prepare private header
            pkt.priv.packet_length = usb_pkt.usb_hdr.length > sizeof(MctpHeader)
                                       ? usb_pkt.usb_hdr.length - sizeof(MctpHeader)
                                       : sizeof(MctpHeader);

            pkt.priv.packet_interface = static_cast<uint8_t>(mctp::Client::UsUsb);

            // copy mctp header
            pkt.hdr = usb_pkt.mctp_hdr;

            // copy mctp payload
            std::copy(usb_pkt.msg.begin(), usb_pkt.msg.end(), pkt.msg.begin());

            ipc::Queue::Item item(std::bit_cast<uint8_t*>(&pkt), sizeof(pkt));

            auto route_result = route_mctp_to_downstream(pkt, _routing_table);

            if (route_result == RouteResult::Dropped) {
                is_drop = true;
            }
            else if (route_result == RouteResult::NotRouted) {
                auto status = mctp::Driver::mctp_send(item, mctp::Client::UsUsb);
                if (status != mctp::Status::Ok) {
                    logger::error(logger::Event::UsbCannotSendtoMctp);
                    is_drop = true;
                }
            }

            perf_mon::Driver::log_pkt_meas(0, usb_pkt.usb_hdr.length, true, is_drop);
        }
    }

    // Since we've already ensured offset <= BufferSize in the loop, we can use offset directly
    std::fill(mctp_rx_buffer.begin(), mctp_rx_buffer.begin() + offset, 0);

    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::UsbEnableRx);
    /// WAR driver call should not be interrupted
    auto status = _driver.enable_mctp_rx();

    if (status != 0) {
        logger::error(logger::Event::UsbMctpRecvError, {static_cast<uint8_t>(status)});
    }
}

#if (SYS_USB_HID_CP2112 > 0U)
void Task::hid_receive()
{
    bool is_send = false;
    _hid_smb.receive(hid_tx_buffer, hid_rx_buffer, is_send);

    if (is_send == true) {
        /// WAR driver call should not be interrupted
        auto result = _driver.write_hid(hid_tx_buffer.begin(), hid_tx_buffer.size());

        if (result != 0) {
            logger::error(logger::Event::UsbHidWriteError, {static_cast<uint8_t>(result)});
        }
    }

    /// WAR driver call should not be interrupted
    auto error = _driver.enable_hid_rx();

    if (error) {
        logger::error(logger::Event::UsbHidRecvError, {static_cast<uint8_t>(error)});
    }
}
#endif  // SYS_USB_HID_CP2112

uint8_t Task::lstp_transmit()
{
    if constexpr (nv::ipc::EnableLstp) {
        auto& queue = ipc::Queue::make(ipc::QueueId::LstpTx);

        // Tolerate spurious LstpTxBit wakeups on an empty queue.
        // Return non-zero so the caller does NOT mask LstpTxBit out of
        // wait_bits (any real producer set that happens after this
        // point will still wake us). Skip the log -- this is benign.
        if (queue.size() == 0) {
            return 1;
        }

        std::span<uint8_t> item(lstp_tx_buffer.data(), nv::ipc::UsbLstpMsgSize);

        auto status = queue.recv(item, 100ms);
        if (status != ipc::Queue::Status::Ok) {
            logger::error(logger::Event::UsbLstpQueueRecvError, {static_cast<uint8_t>(status)});
            return static_cast<uint8_t>(status);
        }

        auto           spiTxHdr = nv::spi::FlashromMsgHdr_from(lstp_tx_buffer);
        const uint16_t tx_len   = ((spiTxHdr->len_msb << 8) | spiTxHdr->len_lsb)
                              + nv::spi::Flashrom::SPI_HEADER_LEN;
        if (tx_len
            > (nv::spi::Flashrom::SPI_MAX_DATA_LEN + nv::spi::Flashrom::SPI_HEADER_LEN)) {
            logger::error(logger::Event::UsbLstpWriteError, {static_cast<uint8_t>(0)});
            return 1;  // Abort transmission on invalid length
        }

        auto result = _driver.write_spi(lstp_tx_buffer.begin(), tx_len);
        if (result != 0) {
            logger::error(logger::Event::UsbLstpWriteError, {static_cast<uint8_t>(result)});
            // Propagate the failure so the caller does NOT mask LstpTxBit out of
            // wait_bits.
            return result;
        }
    }
    return 0;
}

void Task::lstp_receive()
{
    if constexpr (nv::ipc::EnableLstp) {
        if constexpr (nv::lstp::EnableSpi) {
            // For backward compatibility with Flashrom NV_SMA_SPI
            // Flashrom always uses channel ID = 0 for CONFIG command
            auto spiRxHdr = nv::spi::FlashromMsgHdr_from(lstp_rx_buffer);
            if ((spiRxHdr->channelId == 0)
                && (spiRxHdr->cmdCode & nv::spi::Flashrom::CMD_CODE_MASK)
                       == nv::spi::FlashromCmdCode::SPI_CMD_CONFIG) {
                recover_lstp_endpoint();
                clear_usb_lstp_queue();
            }
        }

        std::span<uint8_t> item(lstp_rx_buffer.data(), lstp_rx_buffer.size());
        auto               status = nv::lstp::LstpParser::validate_request(item, lstp_rx_len);

        if (status == nv::lstp::LstpStatus::Success) {
            status = _lstp_router.receive(item);
        }

        if (status != nv::lstp::LstpStatus::Success) {
            nv::lstp::LstpRouter::send_error(item, status);
        }

        auto error = _driver.enable_spi_rx();
        if (error) {
            logger::error(logger::Event::UsbLstpRecvError, {static_cast<uint8_t>(error)});
        }
    }
}

uint8_t Task::transmit()
{
    // The packet may already be sent when its TX notification arrives.
    if (_tx.size() == 0) {
        // Nothing to send; keep TX idle.
        return 1;
    }

    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::UsbHandleRxPkt);
    mctp::Packet pkt{};
    auto         is_drop = false;

    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&pkt), sizeof(pkt));
    auto             queue_status = _tx.recv(item, 100ms);

    if (queue_status != ipc::Queue::Status::Ok) {
        logger::error(logger::Event::UsbMctpTxQueueRecvError,
                      {static_cast<uint8_t>(queue_status)});
        return static_cast<uint8_t>(queue_status);
    }

    // TODO: compose multiple mctp pkt into one single usb pkt
    auto& usb_pkt = from(mctp_tx_buffer, 0);

    // prepare private header
    usb_pkt.usb_hdr.length  = pkt.priv.packet_length + 4;
    usb_pkt.usb_hdr.rsvb    = 0;
    usb_pkt.usb_hdr.dmtf_id = UsbDmtfId;

    // copy mctp header
    usb_pkt.mctp_hdr = pkt.hdr;

    // copy mctp payload
    std::copy(pkt.msg.begin(), pkt.msg.end(), usb_pkt.msg.begin());

    uint8_t result = 0;

    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::UsbCallDriverTx,
                                      static_cast<uint32_t>(pkt.priv.packet_interface));

    /// WAR driver call should not be interrupted
    result = _driver.write_mctp(mctp_tx_buffer.begin(), usb_pkt.usb_hdr.length);

    if (result != 0) {
        logger::error(logger::Event::UsbMctpWriteError, {static_cast<uint8_t>(result)});
        is_drop = true;
    }

    perf_mon::Driver::log_pkt_meas(0, usb_pkt.usb_hdr.length, false, is_drop);
    return result;
}

void Task::clear_queue()
{
    const mctp::Packet Pkt{};

    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&Pkt), sizeof(Pkt));

    auto max_items       = _tx.max_items();
    auto available_items = _tx.available();
    auto items_to_remove = max_items - available_items;

    while (items_to_remove > 0) {
        auto queue_status = _tx.recv(item, 100ms);
        if (queue_status != Queue::Status::Ok) {
            logger::error(logger::Event::UsbClearQueueFailed,
                          {static_cast<uint8_t>(queue_status)});
            break;
        }
        items_to_remove--;
    }
}

void Task::clear_usb_lstp_queue()
{
    if constexpr (nv::ipc::EnableLstp) {
        auto& queue = ipc::Queue::make(ipc::QueueId::LstpTx);
        // Use a local buffer instead of instance member
        std::array<uint8_t, nv::ipc::UsbLstpMsgSize> temp_buffer{};
        std::span<uint8_t> item(temp_buffer.data(), nv::ipc::UsbLstpMsgSize);

        auto items_to_remove = queue.size();

        while (items_to_remove > 0) {
            auto queue_status = queue.recv(item, 0ms);
            if (queue_status != Queue::Status::Ok) {
                break;
            }
            items_to_remove--;
        }
    }
}

void Task::update_routing_table()
{
    usb::update_routing_table(_routing_table);
}

usb::Status Task::usb_tx(ipc::Queue::Item& item)
{
    auto& queue = Queue::make(QueueId::UsbTx);

    auto queue_status = queue.send(item, 100ms);
    if (queue_status != Queue::Status::Ok) {
        return usb::Status::QueueSendFail;
    }

    auto notification_status = TaskNotify::send(TaskId::Usb, MctpTxBit);
    if (notification_status != TaskNotify::Status::Ok) {
        return usb::Status::EventSetFail;
    }

    return usb::Status::Ok;
}

usb::Status Task::set_mctp_rx0_event()
{
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::UsbRecvRxIsr);
    auto notification_status = TaskNotify::send(TaskId::Usb, MctpRxBit);

    if (notification_status != TaskNotify::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

#if (SYS_USB_HID_CP2112 > 0U)
usb::Status Task::set_hid_rx_event()
{
    auto notification_status = TaskNotify::send(TaskId::Usb, HidRxBit);

    if (notification_status != TaskNotify::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}
#endif  // SYS_USB_HID_CP2112

usb::Status Task::set_lstp_rx_event()
{
    if constexpr (nv::ipc::EnableLstp) {
        auto notification_status = TaskNotify::send(TaskId::Usb, LstpRxBit);

        if (notification_status != TaskNotify::Status::Ok) {
            return usb::Status::EventSetFail;
        }
    }
    return usb::Status::Ok;
}

bool Task::is_lstp_device_connected()
{
    if constexpr (nv::ipc::EnableLstp) {
        return sys::usb::Driver::is_device_connected();
    }
    return false;
}

usb::Status Task::set_lstp_tx_done_event()
{
    if constexpr (nv::ipc::EnableLstp) {
        auto notification_status = TaskNotify::send(TaskId::Usb, LstpTxDoneBit);

        if (notification_status != TaskNotify::Status::Ok) {
            return usb::Status::EventSetFail;
        }
    }
    return usb::Status::Ok;
}

usb::Status Task::set_mctp_tx_done_event()
{
    auto notification_status = TaskNotify::send(TaskId::Usb, MctpTxDoneBit);

    if (notification_status != TaskNotify::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

usb::Status Task::set_device_attach_event()
{
    auto notification_status = TaskNotify::send(TaskId::Usb, AttachBit);

    if (notification_status != TaskNotify::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

usb::Status Task::reset_all_event_bits()
{
    auto notification_status = TaskNotify::send(TaskId::Usb, ResetBit);

    if (notification_status != TaskNotify::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

usb::Status Task::set_update_routing_table_event()
{
    auto notification_status = TaskNotify::send(TaskId::Usb, UpdateRoutingTableBit);

    if (notification_status != TaskNotify::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

bool Task::to_usb(ipchandler::Id    src_id,
                  uint16_t          read_length,
                  ipc::Queue::Item& item,
                  i2c::I2cStatus    result)
{
    using namespace nv::ipc;
    UsbRequest request = {};

    request.read_length = read_length;
    request.result      = result;
    request.src_id      = static_cast<uint8_t>(src_id);

    // Copy only the required size
    auto item_size   = item.size();
    auto buffer_size = request.read_buffer.size();

    if (item_size != buffer_size) {
        logger::error(logger::Event::UsbItemSizeMismatchBufferSize,
                      {static_cast<uint8_t>(item_size), static_cast<uint8_t>(buffer_size)});
        return false;
    }

    // Copy data from span to array
    std::copy_n(item.begin(), item.size(), request.read_buffer.begin());

    auto request_item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto status       = ipchandler::Driver::send(
        src_id, ipchandler::Id::Usb, request_item, sizeof(request), true);
    if (status != ipchandler::Status::Success) {
        logger::error(logger::Event::UsbRequestQueueError, {static_cast<uint8_t>(status)});
        return false;
    }

    return true;
}

void Task::wdt_notify()
{
    auto notification_status = nv::ipc::TaskNotify::send(nv::ipc::TaskId::Usb,
                                                         EventBits::WdtEventBit);
    if (notification_status != nv::ipc::TaskNotify::Status::Ok) {
        // TBD
    }
}

usb::Status Task::to_usbLstp(std::span<uint8_t>& item)
{
    if constexpr (nv::ipc::EnableLstp) {
        // Producers retry continuously while the IN pipe is blocked; logging every failure
        // evicts the rest of the event log, so only the first failure of an episode is logged.
        NV_SHARED_BSS static std::atomic<bool> send_error_logged{false};

        auto& queue = Queue::make(QueueId::LstpTx);

        auto queue_status = queue.send(item, 100ms);
        if (queue_status != Queue::Status::Ok) {
            if (!send_error_logged.exchange(true)) {
                logger::error(logger::Event::UsbLstpQueueSendError,
                              {static_cast<uint8_t>(queue_status)});
            }
            return usb::Status::QueueSendFail;
        }
        send_error_logged.store(false);

        (void)TaskNotify::send(TaskId::Usb, LstpTxBit);
    }
    return usb::Status::Ok;
}

}  // namespace nv::usb

#endif  // !SYS_USB_DISABLED (value check)
