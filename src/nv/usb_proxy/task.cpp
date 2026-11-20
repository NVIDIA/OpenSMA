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
#include <limits>

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
#if defined(USB_CONFIG_UART_BRIDGE) && (USB_CONFIG_UART_BRIDGE > 0U)
#include "nv/vcom/vruart/bridge.h"
#endif

using namespace std::chrono_literals;

namespace nv::usb_proxy {

/*******************************************************************************
 * Static Instance and Shared Data
 ******************************************************************************/
namespace {
static constexpr size_t SharedHidBufferSize = 64;
#if defined(USB_CONFIG_CP2112) && (USB_CONFIG_CP2112 == 0U)
constexpr bool EnableHid = false;
#else
constexpr bool EnableHid = true;
#endif
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
    // leave ~264 bytes of margin above the worst-case observed stack usage
    constexpr auto StackSize = std::max(768, int(configMINIMAL_STACK_SIZE));
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

Task::Task() noexcept : ipc::Task(ipc::TaskId::Usb, "UsbVirtual") {}

[[noreturn]] void Task::main()
{
    if constexpr (nv::lstp::EnableRouter) {
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
        auto notification = ipc::TaskNotify::wait(1s);
        if (!notification.has_value()) {
            continue;  // Timeout or error, retry wait
        }
        auto active_bits = notification.value();

        // Handle HID data from C2C (enqueued by IPC task)
        if (EnableHid && (active_bits & HidRxBit)) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, HidRxBit);
            if (hid_queue.recv(hid_item, 0ms) == ipc::Queue::Status::Ok) {
                process_hid(hid_buf.data(), hid_buf.size());
            }
        }

        // Handle I2C response (enqueued by I2C/I3C/IOX task)
        if (EnableHid && (active_bits & HidI2cRespBit)) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, HidI2cRespBit);
            if (i2c_resp_queue.recv(i2c_resp_item, 0ms) == ipc::Queue::Status::Ok) {
                process_i2c_response(i2c_resp.read_length,
                                     i2c_resp.data.data(),
                                     i2c_resp.data_length,
                                     i2c_resp.result);
            }
        }

        // Handle MCTP data from C2C (enqueued by IPC task)
        if (active_bits & MctpRxBit) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, MctpRxBit);
            if (mctp_queue.recv(mctp_item, 0ms) == ipc::Queue::Status::Ok) {
                process_mctp_from_c2c(mctp_buf.data(), mctp_item.size());
            }
        }

        if constexpr (nv::lstp::EnableRouter) {
            if (active_bits & LstpRxBit) {
                (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, LstpRxBit);
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
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, WdtBit);
            nv::watchdog::Runtime::mark_task_alive(nv::watchdog::TaskMonitorIndex::Usb);
        }

        if (active_bits & UpdateRoutingTableBit) {
            (void)ipc::TaskNotify::clear(ipc::TaskId::Usb, UpdateRoutingTableBit);
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

void Task::complete_hid_error(Status1 status)
{
    _hid_status0              = Status0::CompleteWithError;
    _hid_status1              = status;
    _hid_bytes_received       = 0;
    _hid_bytes_sent           = 0;
    _i2c_response_data_length = 0;
    _has_pending_response     = true;
    _hid_state                = HidState::Idle;
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
                complete_hid_error(Status1::ArbitrationLost);
                return;
            }
            const uint8_t slave_addr            = data[1] >> 1U;
            auto [ipchandler_id, physical_addr] = i2c_addr_mapping(slave_addr);

            _hid_read_length = (static_cast<uint16_t>(data[2]) << 8) | data[3];

            if (_hid_read_length > MaxReadSize) {
                logger::error(logger::Event::UsbBufferOverflow,
                              {4,
                               static_cast<uint8_t>(_hid_read_length & UINT8_MAX),
                               static_cast<uint8_t>(_hid_read_length >> 8)});
                complete_hid_error(Status1::TimeoutNacked);
                return;
            }

            if (ipchandler_id == ipchandler::Id::Unuse) {
                complete_hid_error(Status1::TimeoutNacked);
                return;
            }

            // For read request, write_length=0, so we use a small buffer (not actually used)
            i2c::I2cBuffer     buffer{};
            std::span<uint8_t> buffer_span(buffer.data(), buffer.size());

            const bool i2c_ok = send_to_backend(
                ipchandler_id, physical_addr, 0, _hid_read_length, buffer_span);
            if (!i2c_ok) {
                complete_hid_error(Status1::ArbitrationLost);
                return;
            }
            _hid_state                = HidState::Read;
            _hid_bytes_received       = 0;
            _hid_bytes_sent           = 0;
            _i2c_response_data_length = 0;
            _has_pending_response     = false;
            break;
        }

        case ReportId::DataWrite: {
            if (length < DataWriteHeaderSize) {
                complete_hid_error(Status1::ArbitrationLost);
                return;
            }
            const uint8_t slave_addr            = data[1] >> 1U;
            const uint8_t write_len             = data[2];
            auto [ipchandler_id, physical_addr] = i2c_addr_mapping(slave_addr);

            if (ipchandler_id == ipchandler::Id::Unuse || write_len > MaxDataSize
                || length < DataWriteHeaderSize + write_len) {
                complete_hid_error(Status1::TimeoutNacked);
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
                complete_hid_error(Status1::ArbitrationLost);
                return;
            }
            _hid_state                = HidState::Write;
            _hid_read_length          = 0;
            _hid_bytes_received       = 0;
            _hid_bytes_sent           = 0;
            _hid_status0              = Status0::Busy;
            _hid_status1              = Status1::WriteInProgress;
            _has_pending_response     = false;
            _i2c_response_buffer      = {};
            _i2c_response_data_length = 0;
            break;
        }

        case ReportId::TransferStatusReq: {
            _response_buffer    = {};
            _response_buffer[0] = static_cast<uint8_t>(ReportId::TransferStatusRes);

            if (_has_pending_response) {
                // Operation completed - report held status. For reads with data, keep
                // pending until DataReadForceSend. For writes/errors, clear after this
                // poll so a late TransferStatusReq still sees Complete instead of Idle.
                _response_buffer[1] = static_cast<uint8_t>(_hid_status0);
                _response_buffer[2] = static_cast<uint8_t>(_hid_status1);
                _response_buffer[3] = 0x00;  // Retries high byte
                _response_buffer[4] = 0x00;  // Retries low byte
                _response_buffer[5] = (_hid_bytes_received >> 8) & 0xFF;
                _response_buffer[6] = _hid_bytes_received & 0xFF;
                if (_i2c_response_data_length == 0) {
                    _has_pending_response = false;
                }
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
            // The backend has no cancellation API. If a request is still in
            // flight, discard its eventual response so it cannot revive this
            // canceled transaction.
            const bool backend_response_pending = (_hid_state == HidState::Read
                                                   || _hid_state == HidState::Write)
                                               && !_has_pending_response;
            // Track every canceled in-flight request and saturate to avoid wrapping.
            if (backend_response_pending
                && _discard_i2c_response_count < std::numeric_limits<uint8_t>::max()) {
                ++_discard_i2c_response_count;
            }

            _hid_state                = HidState::Idle;
            _hid_read_length          = 0;
            _hid_bytes_received       = 0;
            _hid_bytes_sent           = 0;
            _hid_status0              = Status0::Idle;
            _hid_status1              = Status1::Acked;
            _has_pending_response     = false;
            _i2c_response_buffer      = {};
            _i2c_response_data_length = 0;
            break;
        }

        case ReportId::DataWriteReadReq: {
            // Write-then-read (combined transaction)
            if (length < DataWriteReadReqHeaderSize) {
                complete_hid_error(Status1::ArbitrationLost);
                return;
            }
            const uint8_t slave_addr            = data[1] >> 1U;
            auto [ipchandler_id, physical_addr] = i2c_addr_mapping(slave_addr);
            _hid_read_length           = (static_cast<uint16_t>(data[2]) << 8) | data[3];
            const uint8_t tar_addr_len = data[4];

            if (_hid_read_length > MaxReadSize) {
                logger::error(logger::Event::UsbBufferOverflow,
                              {4,
                               static_cast<uint8_t>(_hid_read_length & UINT8_MAX),
                               static_cast<uint8_t>(_hid_read_length >> 8)});
                complete_hid_error(Status1::TimeoutNacked);
                return;
            }

            if (ipchandler_id == ipchandler::Id::Unuse || tar_addr_len > MaxAddrSize
                || (tar_addr_len > 0 && length < DataWriteReadReqHeaderSize + tar_addr_len)) {
                complete_hid_error(Status1::TimeoutNacked);
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
                complete_hid_error(Status1::ArbitrationLost);
                return;
            }
            _hid_state                = HidState::Read;
            _hid_bytes_received       = 0;
            _hid_bytes_sent           = 0;
            _i2c_response_data_length = 0;
            _has_pending_response     = false;
            break;
        }

        case ReportId::DataReadForceSend: {
            if (length < 3) {
                complete_hid_error(Status1::ArbitrationLost);
                return;
            }

            // Return at most one HID payload and retain any remaining I2C data
            // for the next force-send request.
            _response_buffer    = {};
            _response_buffer[0] = static_cast<uint8_t>(ReportId::DataReadRes);

            if (_hid_status0 == Status0::Complete
                && _hid_bytes_sent < _i2c_response_data_length) {
                // DataReadForceSend.length is big-endian, as sent by the Linux driver.
                const uint16_t requested_length = (static_cast<uint16_t>(data[1]) << 8)
                                                | static_cast<uint16_t>(data[2]);
                const size_t remaining_length = _i2c_response_data_length - _hid_bytes_sent;
                const size_t copy_length      = std::min({remaining_length,
                                                          static_cast<size_t>(requested_length),
                                                          static_cast<size_t>(MaxDataSize)});

                _response_buffer[1] = 0x00;  // Status: success
                _response_buffer[2] = static_cast<uint8_t>(copy_length);
                std::copy(_i2c_response_buffer.begin() + _hid_bytes_sent,
                          _i2c_response_buffer.begin() + _hid_bytes_sent + copy_length,
                          _response_buffer.begin() + 3);
                _hid_bytes_sent += copy_length;
            }
            else {
                // Return with 0 bytes (error or no data)
                _response_buffer[1] = 0x00;
                _response_buffer[2] = 0x00;
            }

            if (_hid_bytes_sent >= _i2c_response_data_length) {
                _has_pending_response     = false;
                _hid_bytes_sent           = 0;
                _i2c_response_data_length = 0;
                _hid_state                = HidState::Idle;
            }
            else {
                _hid_state = HidState::Read;
            }

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
    if (_discard_i2c_response_count > 0) {
        --_discard_i2c_response_count;
        return;
    }

    // Update HID state based on I2C result
    if (result == i2c::I2cStatus::Ok) {
        _hid_status0        = Status0::Complete;
        _hid_status1        = Status1::Succeeded;
        _hid_bytes_received = 0;
        _hid_bytes_sent     = 0;

        if (data_length > 0 && data != nullptr) {
            const size_t copy_len = std::min(
                {static_cast<size_t>(read_length), data_length, _i2c_response_buffer.size()});
            std::copy(data, data + copy_len, _i2c_response_buffer.begin());
            _hid_bytes_received       = static_cast<uint16_t>(copy_len);
            _i2c_response_data_length = copy_len;
        }
        else {
            _i2c_response_data_length = 0;
        }
    }
    else {
        _hid_status0 = Status0::CompleteWithError;
        switch (result) {
            case i2c::I2cStatus::Busy   : _hid_status1 = Status1::Scllowtimeout; break;
            case i2c::I2cStatus::ArbLost: _hid_status1 = Status1::ArbitrationLost; break;
            case i2c::I2cStatus::Nak    :
            case i2c::I2cStatus::Error  :
            default                     : _hid_status1 = Status1::TimeoutNacked; break;
        }
        _hid_bytes_received       = 0;
        _hid_bytes_sent           = 0;
        _i2c_response_data_length = 0;
    }

    // Hold the result instead of pushing it. 0x16 is defined as the reply to a
    // TransferStatusReq (0x15), and the host discards input reports that arrive
    // outside its own request window, so an unsolicited report would be dropped
    // and would also consume Core1's single HID IN buffer.
    _has_pending_response = true;

    // Reads with data stay pending until DataReadForceSend; writes and errors are
    // cleared once TransferStatusReq has reported them.
    if (!(_hid_state == HidState::Read && _hid_status0 == Status0::Complete
          && _i2c_response_data_length > 0)) {
        _i2c_response_data_length = 0;
    }

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
    auto& queue = ipc::Queue::make(ipc::QueueId::UsbI2cResp);

    // Give the USB proxy task a bounded opportunity to drain its single-entry queue.
    // Do not wait indefinitely here because this runs in the backend task context.
    constexpr uint8_t MaxSendAttempts = 3;
    uint8_t           send_attempts   = 0;
    auto              status          = ipc::Queue::Status::Full;
    do {
        ++send_attempts;
        status = queue.send(q_item, 100ms);
    } while ((status == ipc::Queue::Status::Full || status == ipc::Queue::Status::Timeout)
             && send_attempts < MaxSendAttempts);

    // Notify usb_proxy::Task only on successful enqueue
    if (status == ipc::Queue::Status::Ok) {
        (void)ipc::TaskNotify::send(ipc::TaskId::Usb, HidI2cRespBit);
    }
    else {
        logger::error(
            logger::Event::UsbRequestQueueError,
            {static_cast<uint8_t>(status), static_cast<uint8_t>(src_id), send_attempts});
    }

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

    if (queue_id == ipc::QueueId::UsbTx) {
        // MCTP: enqueue to UsbTx FreeRTOS queue (item_size = 512)
        constexpr size_t MctpQueueItemSize = 512;
        const auto       item              = ipc::Queue::ConstItem(data, MctpQueueItemSize);
        if (ipc::Queue::make(ipc::QueueId::UsbTx).send(item, 100ms) == ipc::Queue::Status::Ok) {
            (void)ipc::TaskNotify::send(ipc::TaskId::Usb, Task::MctpRxBit);
        }
        return true;
    }

    if (EnableHid && queue_id == ipc::QueueId::UsbHid) {
        // HID: enqueue to UsbHid FreeRTOS queue (item_size = 64)
        if (data == nullptr || length == 0 || length > SharedHidBufferSize) {
            logger::error(logger::Event::UsbBufferOverflow, {4});
            return true;
        }

        // Core1 forwards the actual USB transfer length, which may be shorter
        // than the fixed 64-byte queue item. Zero-pad locally so the queue copy
        // never reads beyond the valid C2C payload.
        std::array<uint8_t, SharedHidBufferSize> hid_report{};
        std::copy(data, data + length, hid_report.begin());

        const auto item = ipc::Queue::ConstItem(hid_report.data(), hid_report.size());
        if (ipc::Queue::make(ipc::QueueId::UsbHid).send(item, 100ms)
            == ipc::Queue::Status::Ok) {
            (void)ipc::TaskNotify::send(ipc::TaskId::Usb, Task::HidRxBit);
        }
        return true;
    }

#if defined(USB_CONFIG_UART_BRIDGE) && (USB_CONFIG_UART_BRIDGE > 0U)
    if (queue_id == ipc::QueueId::UsbAcm) {
        // ACM: wrap with 2-byte length prefix, enqueue to Ubridge0Rx for bridge task
        constexpr size_t                AcmBufSize = 514;  // 2-byte length + 512-byte max data
        std::array<uint8_t, AcmBufSize> buf{};
        std::memcpy(buf.data(), &length, sizeof(length));
        std::memcpy(buf.data() + 2, data, length);
        const auto item = ipc::Queue::ConstItem(buf.data(), buf.size());
        (void)ipc::Queue::make(ipc::QueueId::Ubridge0Rx).send(item, 100ms);
        nv::vruart::CdcBridge::set_usb_rx_done_event();
        return true;
    }
#endif

    if constexpr (nv::lstp::EnableRouter) {
        if (queue_id == ipc::QueueId::LstpRx) {
            const auto item = ipc::Queue::ConstItem(data, nv::ipc::UsbLstpMsgSize);
            if (ipc::Queue::make(ipc::QueueId::LstpRx).send(item, 100ms)
                == ipc::Queue::Status::Ok) {
                (void)ipc::TaskNotify::send(ipc::TaskId::Usb, Task::LstpRxBit);
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
    (void)ipc::TaskNotify::send(ipc::TaskId::Usb, UpdateRoutingTableBit);
}

void Task::update_routing_table()
{
    usb::update_routing_table(_routing_table);
}

}  // namespace nv::usb_proxy

#endif  // NCSI_ENABLE
