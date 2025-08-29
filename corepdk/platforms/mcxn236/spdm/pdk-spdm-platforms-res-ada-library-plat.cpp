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
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-ada-library-plat.h"
#include "sys/ipc/event.h"
#include "nv/ipc/task.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"
#include "nv/spdm/task.h"
#include "nv/fw_parser/fw_parser.h"
#include "nv/pldm/task.h"

#include <numeric>

namespace pdk::spdm::platforms::res::ada::library {
using namespace nv;
using namespace std::chrono_literals;

/*
 * has_data_for_spdm_responsder()
 *
 * This function returns true when there is data for the responder to consume. This function
 * will be called by spdm library and it will return until there is a data.
 *
 * Returns: 0 for error.
 */
ResponsderNumber has_data_for_spdm_responsder()
{
    bool            receive_data     = false;
    nv::ipc::Event& event            = nv::ipc::Event::make(nv::ipc::EventId::SpdmTask);
    int             responder_number = 0;

    while (!receive_data) {
        auto event_bits = event.wait(nv::spdm::Task::RxWaitEventBit, false, false, 1s).value();
        // check data is recive
        if (event_bits & nv::spdm::Task::EventBits::UsbRxBit) {
            receive_data     = true;
            responder_number = 2;
        }
        // need to auth fw
        if (event_bits & nv::spdm::Task::EventBits::AuthenticateRequestBit) {
            event.clear(nv::spdm::Task::EventBits::AuthenticateRequestBit);
            // get the data from queue.
            nv::ipc::Queue& queue = nv::ipc::Queue::make(nv::ipc::QueueId::SpdmCryptoHelper);

            nv::spdm::crypto::CryptoReqResParameters request_parameters{};

            auto& request_parameters_arr_view = *std::bit_cast<
                std::array<uint8_t, sizeof(decltype(request_parameters))>*>(
                &request_parameters);

            nv::ipc::Queue::Item item(request_parameters_arr_view.data(),
                                      request_parameters_arr_view.size());

            auto status = queue.recv(item, 100ms);

            if (status != nv::ipc::Queue::Status::Ok) {
                // fail
            }
            // start to auth.
            auto auth_result           = nv::spdm::crypto::CryptoStatus::FailUnknown;
            auto request_task_id       = nv::ipc::TaskId::Begin;
            auto input_parsing_fw_type = nv::fw_parser::ParsingFwType::Begin;
            if (const auto* VarPtr = std::get_if<
                    nv::spdm::crypto::AuthenticateFirmwareRequestParameter>(
                    &request_parameters)) {
                auth_result = nv::spdm::crypto::authenticate_firmware(
                    VarPtr->input_parsing_fw_type);
                request_task_id       = VarPtr->request_task_id;
                input_parsing_fw_type = VarPtr->input_parsing_fw_type;
            }

            // find the callback func to according to its task.
            auto response_parameters = nv::spdm::crypto::AuthenticateFirmwareResponseParameter{
                .auth_result           = auth_result,
                .input_parsing_fw_type = input_parsing_fw_type,
                .request_task_id       = request_task_id};

            // notify task to receive the date.
            switch (request_task_id) {
                case nv::ipc::TaskId::Pldm:
                    nv::spdm::crypto::send_authenticate_firmware_result<nv::ipc::TaskId::Pldm>(
                        response_parameters);
                    break;
                default: break;
            }
            if (nv::ipc::Event::Status::Ok
                != event.set(nv::spdm::Task::EventBits::AuthenticateTaskIdle)) {
                nv::info("set AuthenticateTaskIdle fail\n");
            }
        }
        if constexpr (nv::ipc::SpdmI2cResponder) {
            if (event_bits & nv::spdm::Task::EventBits::I2cRxBit) {
                receive_data     = true;
                responder_number = 1;
            }
        }
    }
    return static_cast<ResponsderNumber>(responder_number);
}
/*
 * get_data_from_spdm_responsder()
 *
 * This function is called by the SPDM responder to send data to the transport layer.  This is
 * primarily for the SPDM responder to send responses to the requester.
 *
 * buffer is a pointer to the buffer containing bytes to send
 * num_bytes is the number of bytes to send
 *
 * Returns: void
 */
void get_data_from_spdm_responsder(std::span<const uint8_t> data_from_spdm_responsder)
{
    constexpr size_t   SpdmMsgTypeLength = 1;
    constexpr uint32_t ItemSize          = nv::ipc::SpdmRequestQueueSize;
    // the queue size should larger than the size of mctp header.
    static_assert(ItemSize > sizeof(nv::mctp::Header) + sizeof(nv::mctp::PrivateHeader)
                                 + SpdmMsgTypeLength);
    // the buffer for reciving the data, and sent to the mctp
    std::array<uint8_t, ItemSize> send_buffer{};
    const size_t                  ReceiveResponseLength = data_from_spdm_responsder.size();
    if (ReceiveResponseLength
        > send_buffer.size()
              - (sizeof(nv::mctp::Header) + sizeof(nv::mctp::PrivateHeader)
                 + SpdmMsgTypeLength)) {  // check the buffer/queue size is enough for
                                          // sending message to mctp module
        nv::info("spdm send_data: ReceiveResponseLength is larger than leave queue size\n");
        return;
    }

    nv::ipc::Queue::Item send_item(send_buffer.data(), ItemSize);

    nv::mctp::Packet packet = {};
    // prepare the packet to send back to spdm and set the message type to 5 which is spdm
    auto last_header         = nv::spdm::Task::get_last_header();
    auto last_private_header = nv::spdm::Task::get_last_private_header();

    // check the packet_length not lead to overflow
    uint16_t packet_length = 0;
    if (sizeof(nv::mctp::Header) + SpdmMsgTypeLength <= static_cast<size_t>(
            std::numeric_limits<decltype(packet_length)>::max() - packet_length)) {
        packet_length += static_cast<uint16_t>(sizeof(nv::mctp::Header) + SpdmMsgTypeLength);
    }
    if (ReceiveResponseLength > static_cast<size_t>(
            std::numeric_limits<decltype(packet_length)>::max() - packet_length)) {
        nv::info("packet_length overflow\n");
        return;
    }
    else {
        packet_length += ReceiveResponseLength;
    }

    nv::mctp::Composer::construct_mctp_header(packet,
                                              packet_length,
                                              true,
                                              last_header.src_eid,
                                              last_private_header.packet_interface,
                                              last_header.msg_tag,
                                              last_header.hdr_ver);
    packet.msg[0] = static_cast<uint8_t>(nv::mctp::MsgType::Spdm);

    // copy the prepared packet.
    std::memcpy(send_buffer.data(), &packet, sizeof(nv::mctp::Packet));
    // copy the response data from spdm library
    std::memcpy(send_buffer.data() + sizeof(nv::mctp::Header) + sizeof(nv::mctp::PrivateHeader)
                    + SpdmMsgTypeLength,
                data_from_spdm_responsder.data(),
                data_from_spdm_responsder.size());
    auto status = nv::mctp::Driver::mctp_send_from_spdm(send_item);
    if (status != nv::mctp::Status::Ok) {
        nv::logger::EventData log_data{};
        auto                  spdm_message_iter = send_buffer.begin() + sizeof(nv::mctp::Header)
                               + sizeof(nv::mctp::PrivateHeader) + SpdmMsgTypeLength;
        std::copy(spdm_message_iter, spdm_message_iter + log_data.size(), log_data.begin());
        nv::logger::info(nv::logger::Event::SpdmSendToMctpFail, log_data);
        nv::info("spdm send_data: mctp_send_from_spdm status %d\n", status);
    }
    return;
}

/*
 * send_data_to_spdm_responsder()
 *
 * This function is called by the SPDM responder to read data.
 *
 * buffer is a pointer to the buffer to copy the data into
 * *num_bytes is the number of bytes in the buffer, routine returns number of bytes copied into
 * buffer
 *
 * Returns: size_t
 */
size_t send_data_to_spdm_responsder(std::span<uint8_t> data_to_spdm_responsder)
{
    constexpr size_t SpdmMsgTypeLength = 1;
    // check it's need to revice data after has data return true, spdm library will call
    // this function until return is 0
    nv::ipc::Event& event                    = nv::ipc::Event::make(nv::ipc::EventId::SpdmTask);
    auto            event_bits               = event.bits(false).value();
    size_t          receive_spdm_data_length = 0;
    // check the rx event is set, and clear the rx envet bits.
    // currently we don't handle mutiple packets.
    if (event_bits & nv::spdm::Task::RxWaitEventBit) {
        event.clear(nv::spdm::Task::RxWaitEventBit);
    }
    else {
        // will reach here, to let spdm library know there is no more data to read.
        return receive_spdm_data_length;
    }

    nv::ipc::Queue& queue = nv::ipc::Queue::make(nv::ipc::QueueId::SpdmRx);

    std::array<uint8_t, nv::ipc::SpdmRxQueueSize> local_buf{};

    nv::ipc::Queue::Item item(local_buf.data(), nv::ipc::SpdmRxQueueSize);

    auto status = queue.recv(item, 100ms);
    if (status != nv::ipc::Queue::Status::Ok) {
        nv::info("spdm receive_data: fail to get data from SpdmRx queue\n");
        nv::logger::info(nv::logger::Event::SpdmReceiveFromMctpFail);
        return receive_spdm_data_length;
    }

    nv::mctp::Packet& packet_view_of_buffer = nv::mctp::Packet::from(item);

    // *num_bytes = message payload size - 1(message type size) - 4(header size)
    constexpr size_t UnusedSize = sizeof(nv::mctp::Header) + (SpdmMsgTypeLength);
    if (static_cast<size_t>(packet_view_of_buffer.priv.packet_length) >= UnusedSize) {
        receive_spdm_data_length = static_cast<size_t>(packet_view_of_buffer.priv.packet_length)
                                 - UnusedSize;
    }
    else {
        nv::info("*num_bytes overflow\n");

        return receive_spdm_data_length;
    }

    if (receive_spdm_data_length == 0 || receive_spdm_data_length > queue.item_size()) {
        nv::info("spdm receive_data: invalid package size during receive data\n");
        receive_spdm_data_length = 0;
        return receive_spdm_data_length;
    }

    // save the last private header / header in order to send back the package with correct
    // client.
    nv::spdm::Task::get_last_private_header() = packet_view_of_buffer.priv;
    nv::spdm::Task::get_last_header()         = packet_view_of_buffer.hdr;
    // copy the message payload to spdm library without msg type
    std::memcpy(data_to_spdm_responsder.data(),
                &packet_view_of_buffer.msg[1],
                receive_spdm_data_length);

    return receive_spdm_data_length;
}

/*
 *  spdm_platform_context_initialize()
 *
 *  This function allocates the platform data structure for holding any
 *  platform specific data for the instance of the responder.  In the
 *  glacier case, we have two statically allocated instances.  The responder
 *  will pass in the pointer associated with the instance for the other
 *  platform APIs.
 *
 *  [out] instance - return the pointer to the platform data structure for
 *                   the instance of the responder
 *
 *  Returns: void
 */
void spdm_platform_context_initialize(
    pdk::spdm::platforms::res::ada::library::PlatformContext** instance)
{
    constexpr uint8_t PlatformContextNotInUse = 255;
    if (instance != nullptr) {
        uint8_t existed_instance_num = 0;
        for (auto& inst : nv::spdm::Task::get_context_instance()) {
            if (inst.inst_num == PlatformContextNotInUse) {
                inst.inst_num = existed_instance_num;
                *instance     = &inst;
                return;
            }
            if (existed_instance_num
                < std::numeric_limits<decltype(existed_instance_num)>::max()) {
                existed_instance_num++;
            }
            else {
                break;
            }
        }
    }
    return;
}

}  // namespace pdk::spdm::platforms::res::ada::library