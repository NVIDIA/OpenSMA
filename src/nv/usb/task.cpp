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

#include <algorithm>
#include <chrono>

#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"
#include "nv/perf_mon/perf_mon.h"
#include "sys/usb/usb.h"
#include "nv/spi/task.h"

#include "nv/watchdog/runtime.h"
#include "nv/spi/flashrom_task.h"

using namespace std::chrono_literals;
using namespace nv::ipc;
using namespace nv;

namespace nv::usb {

void Task::make()
{
    NV_TASK_DATA static Task task;
    constexpr auto           StackSize = std::max(480, int(configMINIMAL_STACK_SIZE));
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

Task::Task()
: ipc::Task(ipc::TaskId::Usb, "USB")
, _event(ipc::Event::make(ipc::EventId::UsbTask))
, _tx(ipc::Queue::make(ipc::QueueId::UsbTx))
{}

[[noreturn]] void Task::main()
{
    auto wait_bits = MctpTxBit | MctpRxBit | MctpTxDoneBit | AttachBit | ResetBit
                   | UpdateRoutingTableBit | HidRxBit | WdtEventBit;

    // Add flashrom-related event bits if enabled
    if constexpr (nv::ipc::EnableFlashrom) {
        wait_bits = wait_bits | SpiRxBit | SpiTxBit | SpiTxDoneBit;
    }

    uint32_t init_status = 0;
    if constexpr (nv::ipc::EnableFlashrom) {
        init_status = _driver.init(mctp_rx_buffer.data(),
                                   mctp_rx_buffer.data(),
                                   hid_rx_buffer.data(),
                                   hid_rx_buffer.data(),
                                   hid_ep0_buffer.data(),
                                   spi_rx_buffer.data(),
                                   spi_rx_buffer.data(),
                                   &spi_rx_len);
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

    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Usb);

    while (true) {
        auto    bits        = _event.wait(wait_bits, false, false, 1s);
        auto    event       = bits.value();
        auto    active_bits = event & wait_bits;
        uint8_t tx_result   = 0;

        if (active_bits & WdtEventBit) {
            _event.clear(WdtEventBit);
            nv::watchdog::Runtime::mark_task_alive(nv::watchdog::TaskMonitorIndex::Usb);
        }
        /* MctpTxBit */
        if (active_bits & MctpTxBit) {
            _event.clear(MctpTxBit);
            tx_result = transmit();

            if (tx_result == 0) {
                // if transmission is successful, disable MctpTxBit in wait_bits.
                wait_bits = disable_bit(wait_bits, MctpTxBit);
            }
        }

        /* MctpRxBit */
        if (active_bits & MctpRxBit) {
            _event.clear(MctpRxBit);
            mctp_receive();
        }

        /* HidRxBit */
        if (active_bits & HidRxBit) {
            _event.clear(HidRxBit);
            hid_receive();
        }

        /* Flashrom/SPI event handling */
        if constexpr (nv::ipc::EnableFlashrom) {
            /* SpiRxBit */
            if (active_bits & SpiRxBit) {
                _event.clear(SpiRxBit);
                spi_receive();
            }

            /* SpiTxBit */
            if (active_bits & SpiTxBit) {
                _event.clear(SpiTxBit);
                tx_result = spi_transmit();

                if (tx_result == 0) {
                    // if transmission is successful, disable SpiTxBit in wait_bits.
                    wait_bits = disable_bit(wait_bits, SpiTxBit);
                }
            }

            /* SpiTxDoneBit */
            if (active_bits & SpiTxDoneBit) {
                _event.clear(SpiTxDoneBit);
                // enable tx
                wait_bits = enable_bit(wait_bits, SpiTxBit);

                // Set SpiTxBit if tx queue not empty
                auto& queue = ipc::Queue::make(ipc::QueueId::SpiToUsb);
                if (queue.size() != 0) {
                    (void)_event.set(EventBits::SpiTxBit);
                }
            }
        }

        /* MctpTxDoneBit */
        if (active_bits & MctpTxDoneBit) {
            _event.clear(MctpTxDoneBit);
            // enable tx
            wait_bits  = enable_bit(wait_bits, MctpTxBit);
            loop_count = 0;
        }

        /* AttachBit */
        if (active_bits & AttachBit) {
            _event.clear(AttachBit);
            logger::info(logger::Event::UsbConnectStatus, {1});
        }

        /* ResetBit */
        if (active_bits & ResetBit) {
            _event.clear(wait_bits);
            clear_queue();
        }

        /* UpdateRoutingTableBit */
        if (active_bits & UpdateRoutingTableBit) {
            _event.clear(UpdateRoutingTableBit);
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
            _event.clear(MctpTxDoneBit);
            loop_count = 0;
        }
        else {
            loop_count++;
        }
    }
}

void Task::recover_spi_endpoint()
{
    if constexpr (nv::ipc::EnableFlashrom) {
        // Clear the stuck endpoint
        _driver.recover_spi_endpoint();
    }
}
void Task::mctp_receive()
{
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::UsbHandleRxIsr);
    auto offset = 0U;

    // handle multi packet in single usb packet
    while (offset < BufferSize) {
        auto& usb_pkt = from(mctp_rx_buffer, offset);
        if (usb_pkt.usb_hdr.dmtf_id != UsbDmtfId
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
            pkt.priv.packet_length = usb_pkt.usb_hdr.length > sizeof(Header)
                                       ? usb_pkt.usb_hdr.length - sizeof(Header)
                                       : sizeof(Header);

            pkt.priv.packet_interface = static_cast<uint8_t>(mctp::Client::UsUsb);

            // copy mctp header
            pkt.hdr = usb_pkt.mctp_hdr;

            // copy mctp payload
            std::copy(usb_pkt.msg.begin(), usb_pkt.msg.end(), pkt.msg.begin());

            ipc::Queue::Item item(std::bit_cast<uint8_t*>(&pkt), sizeof(pkt));

            bool is_routed = false;
            for (auto& entry : _routing_table) {
                if (entry.is_enumerated == true) {
                    // The packet is directly from upstream to downstream
                    if (entry.assigned_eid == pkt.hdr.dst_eid) {
                        // coverity[cert_int31_c_violation] number of client < 256
                        pkt.priv.packet_interface = static_cast<uint8_t>(entry.client);

                        // Check if it is from  is SetEID/Routing table updates
                        if (!mctp::Driver::is_allow_bridge(pkt)) {
                            // Drop the packet since it is directly from upstream to downstream
                            // (MCU is only bridge)
                            logger::info(
                                logger::Event::MctpMcuActAsBridgePacketDrop,
                                {
                                    static_cast<uint8_t>(
                                        static_cast<uint16_t>(mctp::Client::UsUsb) & UINT8_MAX),
                                    static_cast<uint8_t>(
                                        static_cast<uint16_t>(mctp::Client::UsUsb) >> 8U),
                                    pkt.priv.packet_interface,
                                    pkt.msg.at(2),  // command_code
                                    pkt.msg.at(4)   // eid to set
                                });
                            is_drop = true;
                        }
                        else if (entry.client == mctp::Client::DsI2c0
                                 || entry.client == mctp::Client::DsI2c1
                                 || entry.client == mctp::Client::DsI2c2
                                 || entry.client == mctp::Client::DsI2c3) {
                            auto status = i2c::Task::tx(pkt);
                            if (status != i2c::Task::Status::Ok) {
                                logger::error(logger::Event::UsbCannotSend,
                                              {static_cast<uint8_t>(entry.client)});
                                is_drop = true;
                            }
                        }
                        else if (entry.client == mctp::Client::DsI3c0
                                 || entry.client == mctp::Client::DsI3c1) {
                            perf_mon::Driver::log_pkt_latency(
                                perf_mon::Driver::LatencyEvent::UsbSendTxPktToTask);
                            auto status = i3c::Task::tx(pkt);
                            if (status != i3c::Task::Status::Ok) {
                                logger::error(logger::Event::UsbCannotSend,
                                              {static_cast<uint8_t>(entry.client)});
                                is_drop = true;
                            }
                        }
                        else if (entry.client == mctp::Client::Spi0
                                 || entry.client == mctp::Client::Spi1
                                 || entry.client == mctp::Client::Spi2) {
                            if constexpr (nv::ipc::Spi_Available) {
                                auto status = spi::Task::tx(pkt);
                                if (status != spi::Task::Status::Ok) {
                                    logger::error(logger::Event::UsbCannotSend,
                                                  {static_cast<uint8_t>(entry.client)});
                                    is_drop = true;
                                }
                            }
                        }
                        is_routed = true;
                    }
                }
            }

            // send to mctp task if not found
            if (is_routed == false) {
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

uint8_t Task::spi_transmit()
{
    if constexpr (nv::ipc::EnableFlashrom) {
        auto&              queue = ipc::Queue::make(ipc::QueueId::SpiToUsb);
        std::span<uint8_t> item(spi_tx_buffer.data(), nv::ipc::UsbSpiMsgSize);

        auto status = queue.recv(item, 100ms);
        if (status != ipc::Queue::Status::Ok) {
            logger::error(logger::Event::UsbSpiQueueRecvError, {static_cast<uint8_t>(status)});
            return static_cast<uint8_t>(status);
        }

        auto           spiTxHdr = nv::spi::FlashromMsgHdr_from(spi_tx_buffer);
        const uint16_t tx_len   = ((spiTxHdr->len_msb << 8) | spiTxHdr->len_lsb)
                              + nv::spi::Flashrom::SPI_HEADER_LEN;
        if (tx_len
            > (nv::spi::Flashrom::SPI_MAX_DATA_LEN + nv::spi::Flashrom::SPI_HEADER_LEN)) {
            logger::error(logger::Event::UsbSpiWriteError, {static_cast<uint8_t>(0)});
            return 1;  // Abort transmission on invalid length
        }

        auto result = _driver.write_spi(spi_tx_buffer.begin(), tx_len);
        if (result != 0) {
            logger::error(logger::Event::UsbSpiWriteError, {static_cast<uint8_t>(result)});
        }
    }
    return 0;
}

void Task::spi_receive()
{
    if constexpr (nv::ipc::EnableFlashrom) {
        auto spiRxHdr = nv::spi::FlashromMsgHdr_from(spi_rx_buffer);
        if ((spiRxHdr->cmdCode & nv::spi::Flashrom::CMD_CODE_MASK)
            == nv::spi::FlashromCmdCode::SPI_CMD_CONFIG) {
            recover_spi_endpoint();
            clear_usb_spi_queue();
        }
        std::span<uint8_t> item(spi_rx_buffer.data(), nv::ipc::UsbSpiMsgSize);
        nv::spi::FlashromTask::to_spi(item);
        auto error = _driver.enable_spi_rx();
        if (error) {
            logger::error(logger::Event::UsbSpiRecvError, {static_cast<uint8_t>(error)});
        }
    }
}

uint8_t Task::transmit()
{
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

void Task::clear_usb_spi_queue()
{
    if constexpr (nv::ipc::EnableFlashrom) {
        auto& queue = ipc::Queue::make(ipc::QueueId::SpiToUsb);
        // Use a local buffer instead of instance member
        std::array<uint8_t, nv::ipc::UsbSpiMsgSize> temp_buffer{};
        std::span<uint8_t> item(temp_buffer.data(), nv::ipc::UsbSpiMsgSize);

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
    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&_routing_table), sizeof(_routing_table));
    auto             router_queue = ipc::Queue::make(ipc::QueueId::RoutingTable);
    auto             queue_status = router_queue.recv(item, 100ms);

    if (queue_status != Queue::Status::Ok) {
        logger::error(logger::Event::UsbUpdateRoutingTableFailed,
                      {static_cast<uint8_t>(queue_status)});
    }
}

usb::Status Task::usb_tx(ipc::Queue::Item& item)
{
    auto& queue = Queue::make(QueueId::UsbTx);
    auto& event = Event::make(EventId::UsbTask);

    auto queue_status = queue.send(item, 100ms);
    if (queue_status != Queue::Status::Ok) {
        return usb::Status::QueueSendFail;
    }

    auto event_status = event.set(MctpTxBit);
    if (event_status != Event::Status::Ok) {
        return usb::Status::EventSetFail;
    }

    return usb::Status::Ok;
}

usb::Status Task::set_mctp_rx0_event()
{
    perf_mon::Driver::log_pkt_latency(perf_mon::Driver::LatencyEvent::UsbRecvRxIsr);
    auto& event        = Event::make(EventId::UsbTask);
    auto  event_status = event.set(MctpRxBit);

    if (event_status != Event::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

usb::Status Task::set_hid_rx_event()
{
    auto& event        = Event::make(EventId::UsbTask);
    auto  event_status = event.set(HidRxBit);

    if (event_status != Event::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

usb::Status Task::set_spi_rx_event()
{
    if constexpr (nv::ipc::EnableFlashrom) {
        auto& event        = Event::make(EventId::UsbTask);
        auto  event_status = event.set(SpiRxBit);

        if (event_status != Event::Status::Ok) {
            return usb::Status::EventSetFail;
        }
    }
    return usb::Status::Ok;
}

usb::Status Task::set_spi_tx_done_event()
{
    if constexpr (nv::ipc::EnableFlashrom) {
        auto& event        = Event::make(EventId::UsbTask);
        auto  event_status = event.set(SpiTxDoneBit);

        if (event_status != Event::Status::Ok) {
            return usb::Status::EventSetFail;
        }
    }
    return usb::Status::Ok;
}

usb::Status Task::set_mctp_tx_done_event()
{
    auto& event        = Event::make(EventId::UsbTask);
    auto  event_status = event.set(MctpTxDoneBit);

    if (event_status != Event::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

usb::Status Task::set_device_attach_event()
{
    auto& event        = Event::make(EventId::UsbTask);
    auto  event_status = event.set(AttachBit);

    if (event_status != Event::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

usb::Status Task::reset_all_event_bits()
{
    auto& event        = Event::make(EventId::UsbTask);
    auto  event_status = event.set(ResetBit);

    if (event_status != Event::Status::Ok) {
        return usb::Status::EventSetFail;
    }
    return usb::Status::Ok;
}

usb::Status Task::set_update_routing_table_event()
{
    auto& event        = Event::make(EventId::UsbTask);
    auto  event_status = event.set(UpdateRoutingTableBit);

    if (event_status != Event::Status::Ok) {
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
    auto event = nv::ipc::Event::make(nv::ipc::EventId::UsbTask);
    auto sts   = event.set(EventBits::WdtEventBit);
    if (sts != nv::ipc::Event::Status::Ok) {
        // TBD
    }
}

usb::Status Task::to_usbSpi(std::span<uint8_t>& item)
{
    if constexpr (nv::ipc::EnableFlashrom) {
        auto& queue = Queue::make(QueueId::SpiToUsb);
        auto& event = Event::make(EventId::UsbTask);

        auto queue_status = queue.send(item, 100ms);
        if (queue_status != Queue::Status::Ok) {
            logger::error(logger::Event::UsbSpiQueueSendError,
                          {static_cast<uint8_t>(queue_status)});
            return usb::Status::QueueSendFail;
        }

        event.set(SpiTxBit);
    }
    return usb::Status::Ok;
}

}  // namespace nv::usb
