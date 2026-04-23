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

#include "nv/usb/hid_smb.h"

#include <chrono>

#include "nv/i2c/common.h"
#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "sys/usb/usb.h"
#include "nv/usb/task.h"
#include "nv/usb/i2c_backend.h"
#include "nv/iox/task.h"

// WAR
#include "nv/i3c/driver.h"

using namespace std::chrono_literals;
using namespace nv::ipc;
using namespace nv;

namespace nv::usb {

namespace {

void i2c_manual_nack(ipchandler::Id src_i2c_ipchandler_id, std::span<uint8_t> response_span)
{
    using namespace nv::ipc;
    usb::Task::to_usb(src_i2c_ipchandler_id, 0, response_span, nv::i2c::I2cStatus::Nak);
    return;
}

}  // namespace

HidSmb::HidSmb() : _queue(ipc::Queue::make(ipc::QueueId::UsbHid)) {}

void HidSmb::receive(Buffer& tx_buffer, Buffer& rx_buffer, bool& is_tx_send)
{
    is_tx_send = false;

    auto& hid_report = hid_from(rx_buffer);

    if (hid_report.report_id == ReportId::DataReadReq) {
        _queue.reset();

        auto& data_read_pkt = hid_10_from(rx_buffer);

        const uint8_t SlaveAddr                  = data_read_pkt.slave_addr >> 1u;
        auto [ipchandler_id, mapping_slave_addr] = usb::i2c_addr_mapping(SlaveAddr);
        auto ocp_device                          = usb::is_i2c_ocp_device(SlaveAddr);

        read_length = (data_read_pkt.length_h << 8) | data_read_pkt.length_l;

        // prevent uninitialized data in buffer
        i2c::I2cHidBuffer        buffer = {};
        const std::span<uint8_t> buffer_span(buffer.data(), i2c::I2cHidSmbBufferSize);

        // Nack when write out of bounds on buffer.data()
        if (static_cast<size_t>(read_length) > buffer.size()
            || ipchandler_id == ipchandler::Id::Unuse) {
            _read_context.reset();
            i2c_manual_nack(ipchandler_id, _read_context.buffer);
            _state = State::Read;
            return;
        }

        // nack if its a OCP device, OCP spec only allow block read/write
        if (ocp_device) {
            _read_context.reset();
            i2c_manual_nack(ipchandler_id, _read_context.buffer);
            _state = State::Read;
            return;
        }

        usb::send_to_i2c_backend(
            ipchandler_id, mapping_slave_addr, 0, read_length, buffer_span);
        _state = State::Read;
    }

    if (hid_report.report_id == ReportId::DataWriteReadReq) {
        _queue.reset();

        auto& data_read_write_pkt = hid_11_from(rx_buffer);

        const uint8_t TarAddrLen                 = data_read_write_pkt.tar_addr_len;
        const uint8_t SlaveAddr                  = data_read_write_pkt.slave_addr >> 1u;
        auto [ipchandler_id, mapping_slave_addr] = usb::i2c_addr_mapping(SlaveAddr);
        // prevent uninitialized data in buffer
        i2c::I2cBuffer buffer = {};

        // Prevent write out of bounds on buffer.data()
        // Prevent read out of bounds on data_read_write_pkt.tar_addr
        if (static_cast<size_t>(TarAddrLen) > buffer.size()
            || static_cast<size_t>(TarAddrLen) > data_read_write_pkt.tar_addr.size()
            || ipchandler_id == ipchandler::Id::Unuse) {
            _read_context.reset();
            i2c_manual_nack(ipchandler_id, _read_context.buffer);
            _state = State::Read;
            return;
        }

        // copy data to buffer
        std::copy(data_read_write_pkt.tar_addr.data(),
                  data_read_write_pkt.tar_addr.data() + TarAddrLen,
                  buffer.data());

        const std::span<uint8_t> span_item(buffer.data(), i2c::I2cBufferSize);
        read_length = data_read_write_pkt.length_l | (data_read_write_pkt.length_h << 8);

        usb::send_to_i2c_backend(
            ipchandler_id, mapping_slave_addr, TarAddrLen, read_length, span_item);
        _state = State::Read;
    }

    if (hid_report.report_id == ReportId::CancelTransfer) {
        // TODO: need to figure out how to do that
        _state = State::Idle;
    }

    if (hid_report.report_id == ReportId::DataWrite) {
        _queue.reset();

        auto& data_write_pkt = hid_14_from(rx_buffer);

        const uint8_t SlaveAddr                  = data_write_pkt.slave_addr >> 1u;
        auto [ipchandler_id, mapping_slave_addr] = usb::i2c_addr_mapping(SlaveAddr);
        const uint8_t Length                     = data_write_pkt.length;
        // prevent uninitialized data in buffer
        i2c::I2cBuffer buffer = {};
        if (static_cast<size_t>(Length) > data_write_pkt.data.size()
            || ipchandler_id == ipchandler::Id::Unuse) {
            _read_context.reset();
            i2c_manual_nack(ipchandler_id, _read_context.buffer);
            _state = State::Read;
            return;
        }

        std::copy(
            data_write_pkt.data.data(), data_write_pkt.data.data() + Length, buffer.data());

        const std::span<uint8_t> span_item(buffer.data(), i2c::I2cBufferSize);

        usb::send_to_i2c_backend(ipchandler_id, mapping_slave_addr, Length, 0, span_item);

        _state = State::Write;
    }

    if (hid_report.report_id == ReportId::DataReadForceSend) {
        auto& force_read_pkt = hid_12_from(rx_buffer);

        const uint16_t requested_length = force_read_pkt.length;

        // prevent uninitialized data in buffer
        tx_buffer            = {};
        auto& data_send_pkt  = hid_13_from(tx_buffer);
        data_send_pkt.hid_id = ReportId::DataReadRes;
        data_send_pkt.status = 0;

        const size_t already_sent = _read_context.bytes_sent;
        size_t remaining_bytes    = (read_length > already_sent) ? (read_length - already_sent)
                                                                 : 0;

        remaining_bytes = std::min(remaining_bytes, static_cast<size_t>(requested_length));

        const size_t amount_to_copy = std::min(remaining_bytes, data_send_pkt.data.size());

        data_send_pkt.length = (amount_to_copy & UINT8_MAX);

        std::copy(_read_context.buffer.data() + already_sent,
                  _read_context.buffer.data() + already_sent + amount_to_copy,
                  data_send_pkt.data.data());

        _read_context.bytes_sent += amount_to_copy;

        if (_read_context.bytes_sent >= read_length) {
            _state = State::Idle;
            _read_context.reset();
        }
        else {
            _state = State::Read;
        }

        is_tx_send = true;
    }

    if (hid_report.report_id == ReportId::TransferStatusReq) {
        // polling transfer status
        auto& status_pkt = hid_16_from(tx_buffer);

        status_pkt        = {};
        status_pkt.hid_id = ReportId::TransferStatusRes;

        Request request{};

        // Get the configured queue item size
        [[maybe_unused]] constexpr size_t queue_item_size = []() constexpr -> size_t {
            for (const auto& info : nv::ipc::QueueInfos) {
                if (std::get<0>(info) == nv::ipc::QueueId::UsbHid) {
                    return static_cast<size_t>(std::get<2>(info));
                }
            }
            return 0;
        }();

        // Only verify queue size in single-core COMPOSITE mode.
        // In NCSI mode, UsbHid queue carries 64-byte HID reports (not HidSmb::Request).
#if defined(USB_CONFIG_COMPOSITE) && !NCSI_ENABLE
        static_assert(
            queue_item_size == sizeof(HidSmb::Request),
            "COMPOSITE mode: UsbHid queue item size must equal sizeof(HidSmb::Request)");
#endif

        ipc::Queue::Item item(std::bit_cast<uint8_t*>(&request), sizeof(request));

        if (_state == State::Read) {
            if (_queue.size() != 0) {
                auto status = _queue.recv(item, 100ms);
                if (status != ipc::Queue::Status::Ok) {
                    info("usb recv failed: %d\n", status);
                }

                if (request.result == i2c::I2cStatus::Ok) {
                    // prevent read out of bounds on request.read_buffer
                    size_t amount_to_copy = std::min(static_cast<size_t>(request.read_length),
                                                     request.read_buffer.size());
                    // prevent write out of bounds on read_buffer
                    amount_to_copy = std::min(amount_to_copy, _read_context.buffer.size());

                    std::copy(request.read_buffer.data(),
                              request.read_buffer.data() + amount_to_copy,
                              _read_context.buffer.data());

                    status_pkt.status0    = Status0::Complete;
                    status_pkt.status1    = Status1::Succeeded;
                    status_pkt.bytes_rx_l = amount_to_copy & UINT8_MAX;
                    status_pkt.bytes_rx_h = (amount_to_copy >> 8) & UINT8_MAX;
                }
                else if (request.result == i2c::I2cStatus::Nak) {
                    status_pkt.status0 = Status0::CompleteWithError;
                    status_pkt.status1 = Status1::TimeoutNacked;
                    _state             = State::Idle;
                }
                else if (request.result == i2c::I2cStatus::Busy) {
                    status_pkt.status0 = Status0::CompleteWithError;
                    status_pkt.status1 = Status1::Scllowtimeout;
                }
                else if (request.result == i2c::I2cStatus::ArbLost) {
                    status_pkt.status0 = Status0::CompleteWithError;
                    status_pkt.status1 = Status1::ArbitrationLost;
                }
                else if (request.result == i2c::I2cStatus::Error) {
                    // TODO: no general error code
                    status_pkt.status0 = Status0::CompleteWithError;
                    status_pkt.status1 = Status1::TimeoutNacked;
                }
            }
            else {
                status_pkt.status0 = Status0::Busy;
                status_pkt.status1 = Status1::ReadIncomplete;
            }
        }
        else if (_state == State::Write) {
            status_pkt.status0 = Status0::Busy;
            status_pkt.status1 = Status1::WriteIncomplete;

            if (_queue.size() != 0) {
                status_pkt.status0 = Status0::Complete;
                status_pkt.status1 = Status1::Succeeded;

                auto status = _queue.recv(item, 100ms);
                if (status != ipc::Queue::Status::Ok) {
                    info("usb recv failed: %d\n", status);
                }

                if (request.result == i2c::I2cStatus::Ok) {
                    status_pkt.status0 = Status0::Complete;
                    status_pkt.status1 = Status1::Succeeded;
                }
                else if (request.result == i2c::I2cStatus::Nak) {
                    status_pkt.status0 = Status0::CompleteWithError;
                    status_pkt.status1 = Status1::TimeoutNacked;
                    _state             = State::Idle;
                }
                else if (request.result == i2c::I2cStatus::Busy) {
                    status_pkt.status0 = Status0::CompleteWithError;
                    status_pkt.status1 = Status1::Scllowtimeout;
                }
                else if (request.result == i2c::I2cStatus::ArbLost) {
                    status_pkt.status0 = Status0::CompleteWithError;
                    status_pkt.status1 = Status1::ArbitrationLost;
                }
                else if (request.result == i2c::I2cStatus::Error) {
                    // TODO: no general error code
                    status_pkt.status0 = Status0::CompleteWithError;
                    status_pkt.status1 = Status1::TimeoutNacked;
                }
            }
            else {
                status_pkt.status0 = Status0::Busy;
                status_pkt.status1 = Status1::ReadIncomplete;
            }
        }
        else if (_state == State::Idle) {
            status_pkt.status0 = Status0::Idle;
        }

        is_tx_send = true;
    }

}  // namespace nv::usb
}  // namespace nv::usb
