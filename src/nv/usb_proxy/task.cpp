/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB Proxy Task Implementation for Core0
 *
 * Memory optimized: Uses C2C StreamBuffer directly, minimal internal buffers.
 */

#include NV_IPC_CONFIG_H

// Only enable when Core1 bare-metal USB is active
#if NCSI_ENABLE

#include "nv/usb_proxy/task.h"

#include <chrono>
#include <algorithm>
#include <cstring>

#include "nv/usb/usb_mctp_header.h"
#include "nv/usb/mctp_router.h"
#include "nv/usb/i2c_backend.h"
#include "nv/lstp/lstp_parser.h"

#include "nv/ipc/ipc_task.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/iox/task.h"
#include "nv/spi/task.h"
#include "nv/nv.h"
#include "nv/common/debug.h"
#include "nv/bootloader.h"
#include "nv/watchdog/runtime.h"
#include "sys/ipc/task.h"
#include "nv/ipc/queue.h"
#include "sys/usb/usb.h"
#if defined(USB_CONFIG_UART_BRIDGE)
#include "nv/vcom/vruart/bridge.h"
#endif

using namespace std::chrono_literals;

namespace nv::usb_proxy {

/*******************************************************************************
 * Static Instance and Shared Data
 ******************************************************************************/
namespace {
static constexpr size_t SharedHidBufferSize = 64;
}  // namespace

// Routing table static member definition
usb::RoutingTable Task::_routing_table{};

/*******************************************************************************
 * Static Helper Functions
 ******************************************************************************/
namespace {

using nv::usb::UsbDmtfId;
using nv::usb::UsbMctpHeader;

}  // namespace

bool Task::send_to_backend(ipchandler::Id     ipchandler_id,
                           uint8_t            physical_addr,
                           uint8_t            write_len,
                           uint16_t           read_len,
                           std::span<uint8_t> buffer)
{
    return usb::send_to_i2c_backend(ipchandler_id, physical_addr, write_len, read_len, buffer);
}

/*******************************************************************************
 * Task Implementation
 ******************************************************************************/

void Task::make()
{
    NV_TASK_DATA static Task task;
    constexpr auto           StackSize = std::max(704, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Usb, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.main();
}

Task::Task() noexcept
: ipc::Task(ipc::TaskId::Usb, "UsbVirtual")
, _event(ipc::Event::make(ipc::EventId::UsbTask))
{}

[[noreturn]] void Task::main()
{
    auto wait_bits = MctpRxBit | HidRxBit | HidI2cRespBit | WdtBit | UpdateRoutingTableBit;

    if constexpr (nv::lstp::EnableRouter) {
        wait_bits = wait_bits | LstpRxBit;
        _lstp_router.init();
    }

    // Mark USB task as booted for boot watchdog
    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Usb);

    // Queue receive buffers
    auto& mctp_queue     = ipc::Queue::make(ipc::QueueId::UsbTx);
    auto& hid_queue      = ipc::Queue::make(ipc::QueueId::UsbHid);
    auto& i2c_resp_queue = ipc::Queue::make(ipc::QueueId::UsbI2cResp);
    auto& lstp_queue     = ipc::Queue::make(ipc::QueueId::LstpRx);

    alignas(4) std::array<uint8_t, 512> mctp_buf{};
    ipc::Queue::Item                    mctp_item(mctp_buf.data(), mctp_buf.size());

    alignas(4) std::array<uint8_t, SharedHidBufferSize> hid_buf{};
    ipc::Queue::Item hid_item(hid_buf.data(), hid_buf.size());

    I2cResponseItem  i2c_resp{};
    ipc::Queue::Item i2c_resp_item(
        reinterpret_cast<uint8_t*>(&i2c_resp),  // NOLINT(*-reinterpret-cast)
        sizeof(i2c_resp));

    constexpr size_t LstpBufferSize = nv::lstp::EnableRouter ? nv::ipc::UsbLstpMsgSize : 1;
    alignas(4) std::array<uint8_t, LstpBufferSize> lstp_buf{};
    ipc::Queue::Item                               lstp_item(lstp_buf.data(), lstp_buf.size());

    while (true) {
        auto bits = _event.wait(wait_bits, false, false, 1s);
        if (!bits.has_value()) {
            continue;  // Timeout or error, retry wait
        }
        auto active_bits = bits.value() & wait_bits;

        // Handle HID data from C2C (enqueued by IPC task)
        if (active_bits & HidRxBit) {
            _event.clear(HidRxBit);
            if (hid_queue.recv(hid_item, 0ms) == ipc::Queue::Status::Ok) {
                process_hid(hid_buf.data(), hid_buf.size());
            }
        }

        // Handle I2C response (enqueued by I2C/I3C/IOX task)
        if (active_bits & HidI2cRespBit) {
            _event.clear(HidI2cRespBit);
            if (i2c_resp_queue.recv(i2c_resp_item, 0ms) == ipc::Queue::Status::Ok) {
                process_i2c_response(i2c_resp.read_length,
                                     i2c_resp.data.data(),
                                     i2c_resp.data_length,
                                     i2c_resp.result);
            }
        }

        // Handle MCTP data from C2C (enqueued by IPC task)
        if (active_bits & MctpRxBit) {
            _event.clear(MctpRxBit);
            if (mctp_queue.recv(mctp_item, 0ms) == ipc::Queue::Status::Ok) {
                process_mctp_from_c2c(mctp_buf.data(), mctp_item.size());
            }
        }

        if constexpr (nv::lstp::EnableRouter) {
            if (active_bits & LstpRxBit) {
                _event.clear(LstpRxBit);
                if (lstp_queue.recv(lstp_item, 0ms) == ipc::Queue::Status::Ok) {
                    // This is technically redundant because core1 already validated the request
                    auto status = nv::lstp::LstpParser::validate_request(lstp_item,
                                                                         lstp_item.size());

                    if (status == nv::lstp::LstpStatus::Success) {
                        status = _lstp_router.receive(lstp_item);
                    }

                    if (status != nv::lstp::LstpStatus::Success) {
                        nv::lstp::LstpRouter::send_error(lstp_item, status);
                    }
                }
            }
        }

        if (active_bits & WdtBit) {
            _event.clear(WdtBit);
            nv::watchdog::Runtime::mark_task_alive(nv::watchdog::TaskMonitorIndex::Usb);
        }

        if (active_bits & UpdateRoutingTableBit) {
            _event.clear(UpdateRoutingTableBit);
            update_routing_table();
        }
    }
}

void Task::process_mctp_from_c2c(const uint8_t* data, size_t length)
{
    if (!data || length < sizeof(UsbMctpHeader)) {
        return;
    }

    size_t offset = 0;

    // Loop through all bundled USB MCTP messages in the transfer buffer.
    // BMC (pldmd via Redfish) may bundle multiple MCTP messages per USB bulk transfer
    // (e.g., 7 messages × 72 bytes = 504 bytes in one 512-byte transfer).
    // Note: IPC queue always delivers a fixed 512-byte buffer, so trailing bytes
    // after the last valid message are padding — not an error.
    while (offset + sizeof(UsbMctpHeader) <= length) {
        // Use memcpy to avoid unaligned access on the packed header
        UsbMctpHeader usb_hdr{};
        std::memcpy(&usb_hdr, data + offset, sizeof(usb_hdr));

        // Non-matching DMTF ID means we've reached padding — stop parsing
        if (usb_hdr.dmtf_id != UsbDmtfId) {
            break;
        }

        const size_t msg_length = usb_hdr.length;
        if (msg_length < sizeof(UsbMctpHeader) || offset + msg_length > length) {
            logger::error(logger::Event::UsbBufferOverflow, {1});
            break;
        }

        // Extract MCTP packet (after USB header)
        const size_t mctp_data_offset = offset + sizeof(UsbMctpHeader);
        const size_t mctp_data_length = msg_length - sizeof(UsbMctpHeader);

        // Create MCTP packet
        mctp::Packet pkt{};
        pkt.priv.packet_length    = mctp_data_length;
        pkt.priv.packet_interface = static_cast<uint8_t>(mctp::Client::UsUsb);

        if (mctp_data_length < sizeof(pkt.hdr)) {
            logger::error(logger::Event::UsbBufferOverflow, {2});
            offset += msg_length;
            continue;
        }
        std::memcpy(&pkt.hdr, data + mctp_data_offset, sizeof(pkt.hdr));

        const size_t payload_offset = mctp_data_offset + sizeof(pkt.hdr);
        const size_t payload_length = mctp_data_length - sizeof(pkt.hdr);
        if (payload_length > pkt.msg.size()) {
            logger::error(logger::Event::UsbBufferOverflow, {3});
            offset += msg_length;
            continue;
        }
        if (payload_length > 0) {
            std::copy(
                data + payload_offset, data + payload_offset + payload_length, pkt.msg.begin());
        }

        // Route using shared MCTP routing logic
        auto route_result = usb::route_mctp_to_downstream(pkt, _routing_table);

        if (route_result == usb::RouteResult::NotRouted) {
            ipc::Queue::Item mctp_item(std::bit_cast<uint8_t*>(&pkt), sizeof(pkt));
            auto mctp_status = mctp::Driver::mctp_send(mctp_item, mctp::Client::UsUsb);
            if (mctp_status != mctp::Status::Ok) {
                logger::error(logger::Event::UsbCannotSendtoMctp,
                              {static_cast<uint8_t>(mctp_status)});
            }
        }

        offset += msg_length;
    }
}

std::pair<ipchandler::Id, uint8_t> Task::i2c_addr_mapping(uint8_t virtual_addr)
{
    return usb::i2c_addr_mapping(virtual_addr);
}

void Task::process_hid(const uint8_t* data, size_t length)
{
    if (!data || length < 1) {
        return;
    }

    ReportId report_id = static_cast<ReportId>(data[0]);
    _response_buffer   = {};

    switch (report_id) {
        case ReportId::DataReadReq: {
            if (length < 4) {
                // Invalid packet length - send error response
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::ArbitrationLost);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                return;
            }
            const uint8_t slave_addr            = data[1] >> 1U;
            auto [ipchandler_id, physical_addr] = i2c_addr_mapping(slave_addr);

            _hid_read_length = (static_cast<uint16_t>(data[2]) << 8) | data[3];

            if (ipchandler_id == ipchandler::Id::Unuse) {
                // Send NAK response
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::TimeoutNacked);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                _hid_state = HidState::Idle;
                return;
            }

            // For read request, write_length=0, so we use a small buffer (not actually used)
            i2c::I2cBuffer     buffer{};
            std::span<uint8_t> buffer_span(buffer.data(), buffer.size());

            const bool i2c_ok = send_to_backend(
                ipchandler_id, physical_addr, 0, _hid_read_length, buffer_span);
            if (!i2c_ok) {
                // I2C queue send failed - return error
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::ArbitrationLost);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                _hid_state = HidState::Idle;
                return;
            }
            _hid_state = HidState::Read;
            break;
        }

        case ReportId::DataWrite: {
            if (length < DataWriteHeaderSize) {
                // Invalid packet length - send error response
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::ArbitrationLost);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                return;
            }
            const uint8_t slave_addr            = data[1] >> 1U;
            const uint8_t write_len             = data[2];
            auto [ipchandler_id, physical_addr] = i2c_addr_mapping(slave_addr);

            if (ipchandler_id == ipchandler::Id::Unuse || write_len > MaxDataSize
                || length < DataWriteHeaderSize + write_len) {
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::TimeoutNacked);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                _hid_state = HidState::Idle;
                return;
            }

            i2c::I2cBuffer buffer{};
            std::copy(data + DataWriteHeaderSize,
                      data + DataWriteHeaderSize + write_len,
                      buffer.data());
            std::span<uint8_t> span_item(buffer.data(), i2c::I2cBufferSize);

            const bool i2c_ok = send_to_backend(
                ipchandler_id, physical_addr, write_len, 0, span_item);
            if (!i2c_ok) {
                // I2C queue send failed - return error
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::ArbitrationLost);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                _hid_state = HidState::Idle;
                return;
            }
            _hid_state = HidState::Write;
            break;
        }

        case ReportId::TransferStatusReq: {
            _response_buffer    = {};
            _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);

            if (_has_pending_response) {
                // Operation completed - report status but keep _has_pending_response
                // It will be cleared when DataReadForceSend is received
                _response_buffer[1] = static_cast<uint8_t>(_hid_status0);
                _response_buffer[2] = static_cast<uint8_t>(_hid_status1);
                _response_buffer[3] = 0x00;  // Retries high byte
                _response_buffer[4] = 0x00;  // Retries low byte
                _response_buffer[5] = (_hid_bytes_received >> 8) & 0xFF;
                _response_buffer[6] = _hid_bytes_received & 0xFF;
            }
            else if (_hid_state == HidState::Read) {
                _response_buffer[1] = static_cast<uint8_t>(Status0::Busy);
                _response_buffer[2] = static_cast<uint8_t>(Status1::ReadInProgress);
            }
            else if (_hid_state == HidState::Write) {
                _response_buffer[1] = static_cast<uint8_t>(Status0::Busy);
                _response_buffer[2] = static_cast<uint8_t>(Status1::WriteInProgress);
            }
            else {
                _response_buffer[1] = static_cast<uint8_t>(Status0::Idle);
            }

            send_response_to_core1(_response_buffer.data(), HidBufferSize);
            break;
        }

        case ReportId::CancelTransfer: {
            _hid_state            = HidState::Idle;
            _has_pending_response = false;
            break;
        }

        case ReportId::DataWriteReadReq: {
            // Write-then-read (combined transaction)
            if (length < DataWriteReadReqHeaderSize) {
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::ArbitrationLost);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                return;
            }
            const uint8_t slave_addr            = data[1] >> 1U;
            auto [ipchandler_id, physical_addr] = i2c_addr_mapping(slave_addr);
            _hid_read_length           = (static_cast<uint16_t>(data[2]) << 8) | data[3];
            const uint8_t tar_addr_len = data[4];

            if (ipchandler_id == ipchandler::Id::Unuse || tar_addr_len > MaxDataSize
                || (tar_addr_len > 0 && length < DataWriteReadReqHeaderSize + tar_addr_len)) {
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::TimeoutNacked);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                _hid_state = HidState::Idle;
                return;
            }

            i2c::I2cBuffer buffer{};
            if (tar_addr_len > 0) {
                std::copy(data + DataWriteReadReqHeaderSize,
                          data + DataWriteReadReqHeaderSize + tar_addr_len,
                          buffer.data());
            }
            std::span<uint8_t> span_item(buffer.data(), i2c::I2cBufferSize);

            const bool i2c_ok = send_to_backend(
                ipchandler_id, physical_addr, tar_addr_len, _hid_read_length, span_item);
            if (!i2c_ok) {
                _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
                _response_buffer[1] = static_cast<uint8_t>(Status0::CompleteWithError);
                _response_buffer[2] = static_cast<uint8_t>(Status1::ArbitrationLost);
                send_response_to_core1(_response_buffer.data(), HidBufferSize);
                _hid_state = HidState::Idle;
                return;
            }
            _hid_state = HidState::Read;
            break;
        }

        case ReportId::DataReadForceSend: {
            // Force send any pending read data
            _response_buffer    = {};
            _response_buffer[0] = static_cast<uint8_t>(ReportId::DataReadRes);

            if (_hid_status0 == Status0::Complete && _i2c_response_data_length > 0) {
                // Return success with data
                _response_buffer[1] = 0x00;  // Status: success
                _response_buffer[2] = static_cast<uint8_t>(_i2c_response_data_length);

                // Copy data starting at byte 3
                size_t copyLen = std::min(_i2c_response_data_length,
                                          static_cast<size_t>(HidBufferSize - 3));
                std::copy(_i2c_response_buffer.begin(),
                          _i2c_response_buffer.begin() + copyLen,
                          _response_buffer.begin() + 3);
            }
            else {
                // Return with 0 bytes (error or no data)
                _response_buffer[1] = 0x00;
                _response_buffer[2] = 0x00;
            }

            // Clear pending response after sending data
            _has_pending_response     = false;
            _i2c_response_data_length = 0;
            _hid_state                = HidState::Idle;

            send_response_to_core1(_response_buffer.data(), HidBufferSize);
            break;
        }

        default: break;
    }
}

void Task::process_i2c_response(uint16_t       read_length,
                                const uint8_t* data,
                                size_t         data_length,
                                i2c::I2cStatus result)
{
    // Update HID state based on I2C result
    if (result == i2c::I2cStatus::Ok) {
        _hid_status0        = Status0::Complete;
        _hid_status1        = Status1::Succeeded;
        _hid_bytes_received = read_length;

        if (data_length > 0 && data != nullptr) {
            const size_t copy_len = std::min(data_length, _i2c_response_buffer.size());
            std::copy(data, data + copy_len, _i2c_response_buffer.begin());
            _i2c_response_data_length = copy_len;
        }
        else {
            _i2c_response_data_length = 0;
        }
    }
    else if (result == i2c::I2cStatus::Nak) {
        _hid_status0              = Status0::CompleteWithError;
        _hid_status1              = Status1::TimeoutNacked;
        _i2c_response_data_length = 0;
    }
    else {
        _hid_status0              = Status0::CompleteWithError;
        _hid_status1              = Status1::ArbitrationLost;
        _i2c_response_data_length = 0;
    }

    _has_pending_response = true;

    // Auto-send TransferStatusRes to host (real CP2112 behavior).
    // A real CP2112 pushes a TransferStatusRes Input Report when the I2C
    // operation completes. The Linux cp2112 kernel driver polls with
    // TransferStatusReq (0x15) and waits for TransferStatusRes (0x16) with
    // Complete status before issuing DataReadForceSend (0x12) to retrieve data.
    _response_buffer = {};

    _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);
    _response_buffer[1] = static_cast<uint8_t>(_hid_status0);
    _response_buffer[2] = static_cast<uint8_t>(_hid_status1);
    _response_buffer[3] = 0x00;  // Retries high byte
    _response_buffer[4] = 0x00;  // Retries low byte
    _response_buffer[5] = (_hid_bytes_received >> 8) & 0xFF;
    _response_buffer[6] = _hid_bytes_received & 0xFF;

    if (_hid_state == HidState::Read && _hid_status0 == Status0::Complete
        && _i2c_response_data_length > 0) {
        // Read succeeded with data: keep _has_pending_response and
        // _i2c_response_data_length intact so that the subsequent
        // DataReadForceSend (0x12) can return the actual data.
    }
    else {
        // Write completed, read failed, or read with 0 bytes:
        // No data to retrieve via DataReadForceSend, clear pending state.
        _has_pending_response     = false;
        _i2c_response_data_length = 0;
    }

    send_response_to_core1(_response_buffer.data(), HidBufferSize);
    _hid_state = HidState::Idle;
}

bool Task::to_usb_proxy(ipchandler::Id    src_id,
                        uint16_t          read_length,
                        ipc::Queue::Item& item,
                        i2c::I2cStatus    result)
{
    // This function runs in I2C task context - pack response into queue item
    I2cResponseItem resp{};
    resp.read_length = read_length;
    resp.result      = result;

    if (result == i2c::I2cStatus::Ok && read_length > 0) {
        resp.data_length = static_cast<uint8_t>(std::min(
            static_cast<size_t>(read_length), std::min(item.size(), resp.data.size())));
        if (resp.data_length > 0) {
            std::copy(item.begin(), item.begin() + resp.data_length, resp.data.begin());
        }
    }

    const auto q_item = ipc::Queue::ConstItem(
        reinterpret_cast<const uint8_t*>(&resp),  // NOLINT(*-reinterpret-cast)
        sizeof(resp));
    auto& queue  = ipc::Queue::make(ipc::QueueId::UsbI2cResp);
    auto  status = queue.send(q_item, 100ms);

    // Set event to notify usb_proxy::Task only on successful enqueue
    if (status == ipc::Queue::Status::Ok) {
        auto& event = ipc::Event::make(ipc::EventId::UsbTask);
        event.set(HidI2cRespBit);
    }

    (void)src_id;
    return status == ipc::Queue::Status::Ok;
}

bool Task::to_usbLstp_proxy(std::span<uint8_t>& data)
{
    if (data.empty()) {
        return true;
    }

    ipc::Queue::ConstItem item(data.data(), data.size());
    auto status = ipc::task::Task::handle_queue_data(item, ipc::QueueId::LstpTx, false);
    return status == ipc::task::Status::Ok;
}

bool Task::send_response_to_core1(const uint8_t* data, size_t length)
{
    if (!data || length == 0) {
        return false;
    }

    // Send HID response via C2C StreamBuffer (handled by ipc::task)
    ipc::Queue::ConstItem item(data, length);
    auto status = ipc::task::Task::handle_queue_data(item, ipc::QueueId::UsbHid, false);

    return status == ipc::task::Status::Ok;
}

/*******************************************************************************
 * C2C Dispatch — called from IPC task to forward Core1 USB data
 ******************************************************************************/

bool dispatch_c2c_data(ipc::QueueId queue_id, const uint8_t* data, uint16_t length)
{
    using namespace std::chrono_literals;
    auto& event = ipc::Event::make(ipc::EventId::UsbTask);

    if (queue_id == ipc::QueueId::UsbTx) {
        // MCTP: enqueue to UsbTx FreeRTOS queue (item_size = 512)
        constexpr size_t MctpQueueItemSize = 512;
        const auto       item              = ipc::Queue::ConstItem(data, MctpQueueItemSize);
        if (ipc::Queue::make(ipc::QueueId::UsbTx).send(item, 100ms) == ipc::Queue::Status::Ok) {
            event.set(Task::MctpRxBit);
        }
        return true;
    }

    if (queue_id == ipc::QueueId::UsbHid) {
        // HID: enqueue to UsbHid FreeRTOS queue (item_size = 64)
        constexpr size_t HidReportSize = 64;
        const auto       item          = ipc::Queue::ConstItem(data, HidReportSize);
        if (ipc::Queue::make(ipc::QueueId::UsbHid).send(item, 100ms)
            == ipc::Queue::Status::Ok) {
            event.set(Task::HidRxBit);
        }
        return true;
    }

#if defined(USB_CONFIG_UART_BRIDGE)
    if (queue_id == ipc::QueueId::UsbAcm) {
        // ACM: wrap with 2-byte length prefix, enqueue to UbridgeRx for bridge task
        constexpr size_t                AcmBufSize = 514;  // 2-byte length + 512-byte max data
        std::array<uint8_t, AcmBufSize> buf{};
        std::memcpy(buf.data(), &length, sizeof(length));
        std::memcpy(buf.data() + 2, data, length);
        const auto item = ipc::Queue::ConstItem(buf.data(), buf.size());
        (void)ipc::Queue::make(ipc::QueueId::UbridgeRx).send(item, 100ms);
        nv::vruart::CdcBridge::set_usb_rx_done_event();
        return true;
    }
#endif

    if constexpr (nv::lstp::EnableRouter) {
        if (queue_id == ipc::QueueId::LstpRx) {
            const auto item = ipc::Queue::ConstItem(data, nv::ipc::UsbLstpMsgSize);
            if (ipc::Queue::make(ipc::QueueId::LstpRx).send(item, 100ms)
                == ipc::Queue::Status::Ok) {
                event.set(Task::LstpRxBit);
            }
            return true;
        }
    }

    return false;  // Not a USB queue — caller should use generic path
}

bool to_usb_proxy_impl(ipchandler::Id    src_id,
                       uint16_t          read_length,
                       ipc::Queue::Item& item,
                       i2c::I2cStatus    result)
{
    return Task::to_usb_proxy(src_id, read_length, item, result);
}

bool to_usbLstp_proxy_impl(std::span<uint8_t>& data)
{
    return Task::to_usbLstp_proxy(data);
}

void Task::set_update_routing_table_event()
{
    auto& event = ipc::Event::make(ipc::EventId::UsbTask);
    event.set(UpdateRoutingTableBit);
}

void Task::update_routing_table()
{
    usb::update_routing_table(_routing_table);
}

}  // namespace nv::usb_proxy

#endif  // NCSI_ENABLE
