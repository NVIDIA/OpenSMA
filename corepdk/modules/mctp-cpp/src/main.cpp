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
#include <array>
#include <cstring>

// For more information about this example, please refer to the link below:
// https://nvidia.sharepoint.com/:w:/r/sites/GFWPlatformDevelopmentKit/Shared%20Documents/1.%20Documents/MCTP-CPP%20Usage%20Example.docx?&web=1

// Include CorePDK files
#include "app/pdk-mctp-app-composer.h"
#include "app/pdk-mctp-app-control.h"
#include "app/pdk-mctp-app-packet-plat.h"
#include "app/pdk-mctp-app-router-plat.h"
#include "app/pdk-mctp-app-validator.h"
#include "pdk-cmn-flowcontrol.h"
#include "pdk-mctp-platforms-enums.h"
#include "pdk/cmn/log/log.h"

using namespace pdk::cmn;

extern "C" {
void corepdk_exit_program(int ex);
void adainit();
}

// assume the max size of the multi-packet message is 1000 bytes
const size_t msg_payload_max_size = 1000;

//  This two structure are defined in the platform code
//  They are used to simulate the i2c over mctp message
struct [[gnu::packed]] I2cHeader
{
    uint8_t rw_dst   : 1;
    uint8_t dst_addr : 7;
    uint8_t cmd_code;
    uint8_t byte_cnt;
    uint8_t ipmi_src : 1;
    uint8_t src_addr : 7;
};
struct [[gnu::packed]] I2cPacket
{
    I2cHeader                       i2c_hdr;
    pdk::mctp::app::TransportHeader mctp_hdr;
    /// add 1 byte for PEC
    std::array<uint8_t, msg_payload_max_size + 1> msg;
};

// add 1 byte for PEC
const size_t i2c_msg_max_size = sizeof(I2cHeader) + pdk::mctp::app::hdrSize
                              + msg_payload_max_size + 1;
const size_t msg_max_size = pdk::mctp::app::privSize + pdk::mctp::app::hdrSize
                          + msg_payload_max_size;

// assume the multi-packet message payload size is 200 bytes
const size_t msg_payload_size = 200;
// msg_size = size of(TransportHeader) + size of(Payload)
const size_t msg_size = pdk::mctp::app::hdrSize + msg_payload_size;
// i2c_msg_size = size of(contents in I2C_Header after byte_cnt) + size
// of(TransportHeader) + size of(Payload)
const size_t i2c_msg_size = 1 + pdk::mctp::app::hdrSize + msg_payload_size;
// assume the client is I2C
// each platform can define its own interface enum
const auto client = pdk::mctp::platforms::Interface::UsI2c;
// assume the message is a PLDM over MCTP message
const auto msg_type = pdk::mctp::app::MsgType::Pldm;

void gen_i2c_message(const pdk::mctp::app::MsgType          msg_type,
                     std::array<uint8_t, i2c_msg_max_size>& i2c_message_buf);
void unbind_i2c_message(const std::array<uint8_t, i2c_msg_max_size>& i2c_message_buf,
                        std::array<uint8_t, msg_max_size>&           message_buf);
void bind_i2c_message(const std::array<uint8_t, msg_max_size>& message_buf,
                      std::array<uint8_t, i2c_msg_max_size>&   i2c_message_buf);

int main()
{
    adainit();
    log::here().info("cxx-only main()\n");
    flowcontrol::corepdk_assert(1 > 0, "This is assertion test");
    log::here().info("MCTP-CPP Usage Example\n");

    // 0.Initialize the components from CorePDK::MOD::MCTP-CPP
    pdk::mctp::app::Control   control;
    pdk::mctp::app::Validator validator(control.router());
    const size_t              test_max_size = 256;
    pdk::mctp::app::Composer  composer;
    // udpate the router info to pass packet validation
    // assume the destination EID is 0x01
    pdk::mctp::platforms::set_cur_eid(control.router(), static_cast<uint32_t>(client), 0x01);

    // 1. Generate a multi-packet i2c over MCTP message (I2C_Header) (MCTP Header)
    // (Msg 0) (Msg 1) ... (Msg n). This can be done in the application code.
    std::array<uint8_t, i2c_msg_max_size> i2c_message_buf = {};
    gen_i2c_message(msg_type, i2c_message_buf);

    // 2. Simulate the unbinding of the i2c message to internal MCTP message
    // (Private_Header) (MCTP Header) (Msg 0) (Msg 1) ... (Msg n). This is done in
    // the platform code (for MCU, it's done in the i2c task).
    std::array<uint8_t, msg_max_size> message_buf = {};
    unbind_i2c_message(i2c_message_buf, message_buf);

    // 3. MCTP-CPP message disassembly example
    // cast the message buffer to a packet to get the header information
    auto&          pkt        = *std::bit_cast<pdk::mctp::app::Packet*>(message_buf.data());
    const uint32_t msg_length = pdk::mctp::platforms::get_packet_length(pkt);
    // Decide how many packet should generate
    uint32_t loop = 1;
    if (msg_length > static_cast<uint32_t>(pdk::mctp::app::PktBufDataLen)) {
        loop += (msg_length - static_cast<uint32_t>(pdk::mctp::app::PktBufDataLen) - 1)
                  / pdk::mctp::app::msgSize
              + 1;
    }
    // define the multi-packet buffer to store the result of message disassembly
    // (Private_Header) (MCTP Header) (Msg 0) (Private_Header) (MCTP Header) (Msg
    // 1) ... (Private_Header) (MCTP Header) (Msg n)
    std::array<uint8_t, test_max_size> multi_pkt_buf = {};
    // start disassembly the message
    log::here().info("Message disassembly start");
    pdk::mctp::app::Packet gen_pkt{};
    bool                   is_complete      = false;
    bool                   gen_multi_status = false;
    uint32_t               gen_offset       = 0;
    for (uint32_t index = 0; index < loop; ++index) {
        gen_multi_status = composer.gen_packet(
            gen_pkt, msg_length, index, is_complete, message_buf);
        log::here().info("Packet #%d", index + 1);
        log::here().info("Length: %d", pdk::mctp::platforms::get_packet_length(gen_pkt));
        if (gen_multi_status == false) {
            log::here().info("Disassembly status: Failed");
            corepdk_exit_program(1);
        }
        // copy the packets to the multi-packet buffer
        memcpy(multi_pkt_buf.begin() + gen_offset,
               &gen_pkt,
               pdk::mctp::platforms::get_packet_length(gen_pkt));
        log::here().info("Status: Copied to buffer");
        gen_offset += pdk::mctp::platforms::get_packet_length(gen_pkt);
        if (is_complete == true) {
            log::here().info("Message disassembly complete\n");
            break;
        }
    }
    composer.clear();

    // 4. MCTP-CPP message validate + assembly example
    log::here().info("Message assembly start");
    // Define the recv_message_buf to store the result of message assembly
    std::array<uint8_t, msg_max_size>        recv_message_buf = {};
    pdk::mctp::platforms::Packet::LengthType full_pkt_size{};
    bool                                     validation_status = false;
    bool                                     recv_multi_status = false;
    uint32_t                                 index             = 0;
    uint32_t                                 recv_offset       = 0;
    while (true) {
        // cast the message buffer to a packet to get the header information
        const pdk::mctp::app::Packet& recv_pkt = *std::bit_cast<pdk::mctp::app::Packet*>(
            &multi_pkt_buf.at(recv_offset));
        log::here().info("Packet #%d", index + 1);
        log::here().info("Length: %d", pdk::mctp::platforms::get_packet_length(recv_pkt));
        auto recv_packet_interface = static_cast<pdk::mctp::platforms::Interface>(
            pdk::mctp::platforms::get_packet_interface(recv_pkt));
        validation_status = validator.validate(recv_pkt, recv_packet_interface);
        if (validation_status == false) {
            log::here().info("Validation status: Failed");
            corepdk_exit_program(1);
        }
        else {
            log::here().info("Validation status: Success");
        }
        recv_multi_status = composer.recv_packet(full_pkt_size, recv_pkt, recv_message_buf);
        log::here().info("Status: Copied to buffer");
        if (recv_multi_status == true) {
            // full_pkt_size is the size of the full packet: size of(Private_Header) +
            // size of(MCTP Header) + size of(Payload)
            log::here().info("Full packet size: %d", full_pkt_size);
            log::here().info("Message assembly complete\n");
            break;
        }
        recv_offset += pdk::mctp::platforms::get_packet_length(recv_pkt);
        index++;
    }
    composer.clear();

    // 5. Simulate the binding of the internal MCTP message to i2c message
    // This is done in the platform code (for MCU, it's done in the i2c task).
    std::array<uint8_t, i2c_msg_max_size> recv_i2c_message_buf = {};
    bind_i2c_message(recv_message_buf, recv_i2c_message_buf);

    corepdk_exit_program(0);
    return 0;
}

void gen_i2c_message(const pdk::mctp::app::MsgType          msg_type,
                     std::array<uint8_t, i2c_msg_max_size>& i2c_message_buf)
{
    I2cPacket i2c_pkt{};
    // fill the i2c header
    i2c_pkt.i2c_hdr.rw_dst   = 0;
    i2c_pkt.i2c_hdr.dst_addr = 0x01;
    i2c_pkt.i2c_hdr.cmd_code = 0x00;
    i2c_pkt.i2c_hdr.byte_cnt = i2c_msg_size;
    i2c_pkt.i2c_hdr.ipmi_src = 0;
    i2c_pkt.i2c_hdr.src_addr = 0x00;
    // fill the mctp header
    i2c_pkt.mctp_hdr.hdr_ver = 1;
    i2c_pkt.mctp_hdr.rsvd    = 0;
    // assume the src_eid is 0x00
    i2c_pkt.mctp_hdr.src_eid = 0x00;
    // assume the dst_eid is 0x00
    i2c_pkt.mctp_hdr.dst_eid = 0x01;
    // assume msg_tag is 0
    i2c_pkt.mctp_hdr.msg_tag = 0;
    // assume the tag_owner is 1
    i2c_pkt.mctp_hdr.tag_owner = 1;
    i2c_pkt.mctp_hdr.pkt_seq   = 0;
    i2c_pkt.mctp_hdr.eom       = 1;
    i2c_pkt.mctp_hdr.som       = 1;
    // fill the msg_type
    // the msg_type is the first byte of the msg
    i2c_pkt.msg.at(0) = static_cast<uint8_t>(msg_type);
    // copy the i2c header to the message array
    std::memcpy(i2c_message_buf.begin(), &i2c_pkt.i2c_hdr, sizeof(I2cHeader));
    // copy the transport header to the message array
    // add 1 to the copy size to include the msg_type
    const size_t copy_size = pdk::mctp::app::hdrSize + 1;
    std::memcpy(i2c_message_buf.begin() + sizeof(I2cHeader), &i2c_pkt.mctp_hdr, copy_size);
    // fill the message with arbitrary data
    // skipping the first byte of the message because it contains the msg_type
    for (size_t i = sizeof(I2cHeader) + copy_size;
         i < msg_payload_size + sizeof(I2cHeader) + copy_size;
         ++i) {
        i2c_message_buf.at(i) = i % UINT8_MAX;
    }
    log::here().info("i2c message generated\n");
}

void unbind_i2c_message(const std::array<uint8_t, i2c_msg_max_size>& i2c_message_buf,
                        std::array<uint8_t, msg_max_size>&           message_buf)
{
    auto& i2c_mctp_pkt = *std::bit_cast<I2cPacket*>(i2c_message_buf.data());
    auto& mctp_pkt     = *std::bit_cast<pdk::mctp::app::Packet*>(message_buf.data());
    // remove the one byte Source Slave Address before the message
    pdk::mctp::platforms::set_packet_length(mctp_pkt, i2c_mctp_pkt.i2c_hdr.byte_cnt - 1U);
    // In MCU, client can be find in project configuration
    pdk::mctp::platforms::set_packet_interface(mctp_pkt, static_cast<uint8_t>(client));
    mctp_pkt.hdr = i2c_mctp_pkt.mctp_hdr;
    // copy the msg part
    std::memcpy(static_cast<void*>(&mctp_pkt.msg[0]),
                static_cast<void*>(&i2c_mctp_pkt.msg[0]),
                pdk::mctp::platforms::get_packet_length(mctp_pkt) - pdk::mctp::app::hdrSize);
    log::here().info("i2c message unbinding complete\n");
}

void bind_i2c_message(const std::array<uint8_t, msg_max_size>& message_buf,
                      std::array<uint8_t, i2c_msg_max_size>&   i2c_message_buf)
{
    auto& i2c_mctp_pkt = *std::bit_cast<I2cPacket*>(i2c_message_buf.data());
    auto& mctp_pkt     = *std::bit_cast<pdk::mctp::app::Packet*>(message_buf.data());
    // add 1 to the byte count to include the Source Slave Address
    auto byte_cnt                 = pdk::mctp::platforms::get_packet_length(mctp_pkt) + 1U;
    i2c_mctp_pkt.i2c_hdr.byte_cnt = byte_cnt;
    i2c_mctp_pkt.mctp_hdr         = mctp_pkt.hdr;
    // copy the msg part
    std::memcpy(static_cast<void*>(&i2c_mctp_pkt.msg[0]),
                static_cast<void*>(&mctp_pkt.msg[0]),
                pdk::mctp::platforms::get_packet_length(mctp_pkt) - pdk::mctp::app::hdrSize);
    log::here().info("i2c message binding complete\n");
}