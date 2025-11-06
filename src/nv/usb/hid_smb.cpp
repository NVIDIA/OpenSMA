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
#include "nv/iox/task.h"

// WAR
#include "nv/i3c/driver.h"

using namespace std::chrono_literals;
using namespace nv::ipc;
using namespace nv;

namespace nv::usb {

namespace {
// declare as anonymous function
// this is uesd to find virtual mapping to downstream i2c | i3c task id and address
std::pair<ipchandler::Id, uint8_t> i2c_addr_mapping(const uint8_t virtual_i2c_address)
{
    for (const ipc::I2cVirtualAddressMappingTableItem mapping_item :
         ipc::I2cVirtualAddressMappingTable) {
        // now only support two mapping for each task (gpu addr and gpu recovery addr)
        if (mapping_item.virtual_address == virtual_i2c_address) {
            if (mapping_item.dynamic_address_type
                == ipc::I2cDynamicAddressType::NotDynamicType) {  // static address
                return std::make_pair(mapping_item.ipchandler_id,
                                      mapping_item.physical_address);
            }
            if constexpr (std::to_underlying(ipc::I2cDynamicAddressType::End)
                              - std::to_underlying(ipc::I2cDynamicAddressType::Begin)
                          != 1) {  // dynamic address
                auto dynamic_address = ipc::find_i2c_dynamic_virtual_address(
                    mapping_item.dynamic_address_type);

                return std::make_pair(mapping_item.ipchandler_id,
                                      dynamic_address.value_or(mapping_item.physical_address));
            }
        }
    }

    // receive not support i2c addr
    if constexpr (ipc::I2cManualNackMode == true) {  // need manual Nack
        return std::make_pair(ipchandler::Id::Unuse, virtual_i2c_address);
    }
    else {
        return std::make_pair(ipc::I2cDefaultInhandlerId, virtual_i2c_address);
    }
}

bool is_i2c_ocp_device(const uint8_t virtual_i2c_address)
{
    for (const ipc::I2cVirtualAddressMappingTableItem mapping_item :
         ipc::I2cVirtualAddressMappingTable) {
        if (mapping_item.virtual_address == virtual_i2c_address) {
            return mapping_item.is_ocp_device;
        }
    }
    return false;
}

void i2c_manual_nack(ipchandler::Id src_i2c_ipchandler_id, std::span<uint8_t> response_span)
{
    using namespace nv::ipc;
    usb::Task::to_usb(src_i2c_ipchandler_id, 0, response_span, nv::i2c::I2cStatus::Nak);
    return;
}

bool isI3cQueue(nv::ipchandler::Id id)
{
    // Check if the id corresponds to an I3C queue
    return (id == nv::ipchandler::Id::I3c0 || id == nv::ipchandler::Id::I3c1);
}

}  // namespace

HidSmb::HidSmb() : _queue(ipc::Queue::make(ipc::QueueId::UsbHid)) {}

void HidSmb::receive(Buffer& tx_buffer, Buffer& rx_buffer, bool& is_tx_send)
{
    is_tx_send = false;

    auto& hid_report = hid_from(rx_buffer);

    if (hid_report.report_id == ReportId::DataReadReq) {
        auto& data_read_pkt = hid_10_from(rx_buffer);

        const uint8_t SlaveAddr                  = data_read_pkt.slave_addr >> 1u;
        auto [ipchandler_id, mapping_slave_addr] = i2c_addr_mapping(SlaveAddr);
        auto is_ocp_device                       = is_i2c_ocp_device(SlaveAddr);

        read_length = (data_read_pkt.length_h << 8) | data_read_pkt.length_l;

        // prevent uninitialized data in buffer
        i2c::I2cHidBuffer  buffer = {};
        std::span<uint8_t> buffer_span(buffer.data(), i2c::I2cHidSmbBufferSize);

        // Nack when write out of bounds on buffer.data()
        if (static_cast<size_t>(read_length) > buffer.size()
            || ipchandler_id == ipchandler::Id::Unuse) {
            _read_context.reset();
            i2c_manual_nack(ipchandler_id, _read_context.buffer);
            _state = State::Read;
            return;
        }

        // nack if its a OCP device, OCP spec only allow block read/write
        if (is_ocp_device) {
            _read_context.reset();
            i2c_manual_nack(ipchandler_id, _read_context.buffer);
            _state = State::Read;
            return;
        }

        if (ipchandler_id == ipchandler::Id::Iox) {
            if constexpr (nv::ipc::EnableIoxEmulation) {
                iox::Task::send_i2c_request(ipchandler::Id::Usb,
                                            mapping_slave_addr,
                                            ipchandler_id,
                                            0,
                                            read_length,
                                            buffer_span);
            }
        }
        else if (isI3cQueue(ipchandler_id)) {
            i3c::Task::to_i2c(ipchandler::Id::Usb,
                              mapping_slave_addr,
                              ipchandler_id,
                              0,
                              read_length,
                              buffer_span);
        }
        else {
            i2c::Task::to_i2c(ipchandler::Id::Usb,
                              mapping_slave_addr,
                              ipchandler_id,
                              0,
                              read_length,
                              buffer_span);
        }
        _state = State::Read;
    }

    if (hid_report.report_id == ReportId::DataWriteReadReq) {
        auto& data_read_write_pkt = hid_11_from(rx_buffer);

        const uint8_t TarAddrLen                 = data_read_write_pkt.tar_addr_len;
        const uint8_t SlaveAddr                  = data_read_write_pkt.slave_addr >> 1u;
        auto [ipchandler_id, mapping_slave_addr] = i2c_addr_mapping(SlaveAddr);
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

        std::span<uint8_t> span_item(buffer.data(), i2c::I2cBufferSize);
        read_length = data_read_write_pkt.length_l | (data_read_write_pkt.length_h << 8);

        if (ipchandler_id == ipchandler::Id::Iox) {
            if constexpr (nv::ipc::EnableIoxEmulation) {
                iox::Task::send_i2c_request(ipchandler::Id::Usb,
                                            mapping_slave_addr,
                                            ipchandler_id,
                                            TarAddrLen,
                                            data_read_write_pkt.length_l,
                                            span_item);
            }
        }
        else if (isI3cQueue(ipchandler_id)) {
            i3c::Task::to_i2c(ipchandler::Id::Usb,
                              mapping_slave_addr,
                              ipchandler_id,
                              TarAddrLen,
                              read_length,
                              span_item);
        }
        else {
            i2c::Task::to_i2c(ipchandler::Id::Usb,
                              mapping_slave_addr,
                              ipchandler_id,
                              TarAddrLen,
                              read_length,
                              span_item);
        }
        _state = State::Read;
    }

    if (hid_report.report_id == ReportId::CancelTransfer) {
        // TODO: need to figure out how to do that
        _state = State::Idle;
    }

    if (hid_report.report_id == ReportId::DataWrite) {
        auto& data_write_pkt = hid_14_from(rx_buffer);

        const uint8_t SlaveAddr                  = data_write_pkt.slave_addr >> 1u;
        auto [ipchandler_id, mapping_slave_addr] = i2c_addr_mapping(SlaveAddr);
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

        std::span<uint8_t> span_item(buffer.data(), i2c::I2cBufferSize);

        if (ipchandler_id == ipchandler::Id::Iox) {
            if constexpr (nv::ipc::EnableIoxEmulation) {
                iox::Task::send_i2c_request(ipchandler::Id::Usb,
                                            mapping_slave_addr,
                                            ipchandler_id,
                                            Length,
                                            0,
                                            span_item);
            }
        }
        else if (isI3cQueue(ipchandler_id)) {
            i3c::Task::to_i2c(
                ipchandler::Id::Usb, mapping_slave_addr, ipchandler_id, Length, 0, span_item);
        }
        else {
            i2c::Task::to_i2c(
                ipchandler::Id::Usb, mapping_slave_addr, ipchandler_id, Length, 0, span_item);
        }

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

        // Only verify queue size in COMPOSITE mode since HID is not used in MCTP mode
#if defined(USB_CONFIG_COMPOSITE)
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
