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
#pragma once
#include <array>
#include <bit>
#include <variant>

#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-validator.h"

#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"
#include "nv/ipc/task.h"
#include "nv/ipc/timer.h"
#include "nv/mctp/composer.h"
#include "nv/mctp/constants.h"
#include "nv/mctp/control.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/vendor.h"
using namespace std::chrono_literals;
namespace nv::mctp {

enum class Status
{
    Begin = 0,
    Ok    = Begin,  ///< Success
    QueueSendFail,  ///< queue send return fail
    EventSetFail,   /// event set fail
    InvalidClient,  ///< Invalid client
    Unknown,        ///< An unknown error occurred.
    End
};

class Driver
{
public:
    Driver(ipc::Task& task, common::Uuid& uuid);

    constexpr static auto EndpointStatusChangePeriod = std::chrono::milliseconds(
        nv::ipc::EndpointStatusChangePeriodMs);
    constexpr static auto    EnumeratePeriod = 3500ms;
    constexpr static uint8_t EndpointCount   = ipc::DownStreamNum;

    using Buffer               = std::array<uint8_t, Constants::BufferSize>;
    using Buffer_Multi_Pkt_Buf = std::array<uint8_t, Constants::MultiPktBufSize>;
    using Buffer_Mctp_Tx       = std::array<uint8_t, Constants::MctpTxBufSize>;
    using Validator            = pdk::mctp::app::Validator;

    void              init();
    [[noreturn]] void main();  ///< main event loop

    enum class CmdCode : uint16_t
    {
        Begin     = static_cast<uint16_t>(mctp::Client::End),
        Enumerate = Begin,
        EnumerateDone,
        DiscoveryNotify,
        RotStateInfoChange,
        EndPointStatusChange,
        EndPointTimerDone,
        WdtEvent,
        NsmT5FatalFaultEI,
        ProtocolReset,
        NsmEventCmd,
        SmbusCacheRefresh,
        End,
    };

    using MctpCmdData3 = std::array<uint8_t, 8>;
    struct Command
    {
        uint16_t     cmd;
        uint8_t      data1;
        uint8_t      data2;
        MctpCmdData3 data3;
    };

    struct [[gnu::packed]] EndpointStatus
    {
        uint8_t next_status;
        uint8_t curr_status;
    };

protected:
    void on_receive(Client client);                     ///< Receive from client
    void forward(Client client, const Packet& tx);      ///< Forward to client
    void dump_packet(const Packet& pkt, uint32_t len);  ///< Dump packet to UART
    void on_forward_message(ipc::Queue::Item& rx_item);
    void on_receive_application(nv::ipc::Queue& queue, ipc::Queue::Item& rx_item);
    void on_enumerate();
    void on_enumerate_done();
    void on_discovery_notify();
    void on_type6_event(NsmFwEvent event);
    void on_receive_event(uint8_t nsmType, uint8_t eventId, MctpCmdData3 eventInfo);
    void on_endpoint_status_change_handler(uint8_t downstream_index, uint8_t status);
    void on_endpoint_status_change(uint8_t downstream_index, uint8_t status);
    void on_endpoint_timer_done(uint8_t endpoint_index);
    void on_wdt_event();

    ipc::Queue::Status                        on_receive_queue(nv::ipc::Queue&           queue,
                                                               ipc::Queue::Item&         item,
                                                               std::chrono::microseconds timeout);
    void                                      on_protocol_reset();
    Control                                   _control;
    Vendor                                    _vendor;
    Nsm                                       _nsm;
    Validator                                 _validator;
    bool                                      _is_send_notify{};
    std::array<EndpointStatus, EndpointCount> _endpoint_status{};

    Buffer               _tx_buf{};
    Buffer               _rx_buf{};
    Buffer_Multi_Pkt_Buf _multi_pkt_buf{};
    Buffer_Mctp_Tx       _mctp_tx_buf{};

    Composer _composer;

    /* queues to all client */
    ipc::Queue&                           _mctp_data_queue;
    ipc::Queue&                           _pldm_queue;
    ipc::Queue&                           _spdm_queue;
    ipc::Queue&                           _cmd_queue;
    ipc::Queue&                           _router_queue;
    ipc::Timer&                           _enum_done_timer;
    std::array<ipc::Timer, EndpointCount> _endpoint_status_change_timers;
    ipc::Task&                            _task;

    /* casting standard array to MctpPacket due to queue api always need standard array input */
    static Packet& from(Buffer& arr) { return *std::bit_cast<Packet*>(&arr[0]); }
    static Packet& from(Buffer_Multi_Pkt_Buf& arr) { return *std::bit_cast<Packet*>(&arr[0]); }
    static Packet& from(Buffer_Mctp_Tx& arr) { return *std::bit_cast<Packet*>(&arr[0]); }

public:  // static api section
    static Status mctp_send(ipc::Queue::Item& item, ipc::QueueId id, Client client);
    static Status mctp_send(ipc::Queue::Item& item, Client client);
    static Status mctp_send_cmd(CmdCode             cmd,
                                uint8_t             data1    = 0,
                                uint8_t             data2    = 0,
                                bool                is_front = false,
                                const MctpCmdData3& data3    = {});
    static Status mctp_send_from_pldm(ipc::Queue::Item& item);
    static Status mctp_send_from_spdm(ipc::Queue::Item& item);
    static Status mctp_send_from_us_usb_4k(ipc::Queue::Item& item);
    static Status endpoint_status_change(uint8_t endpoint_index, uint8_t status);
    static void   protocol_reset();
    static bool   is_allow_bridge(const Packet& pkt);
    static void   wdt_notify();
    static void   on_enumeration_done_timer([[maybe_unused]] ipc::Timer& id);
    static void   on_endpoint_status_change_timer(ipc::Timer& id);
    static void   on_apgood_engage_timer([[maybe_unused]] ipc::Timer& id);
    static void   fill_event_log(EventLog&  event_log,
                                 NsmMsgType nv_msg_type,
                                 uint8_t    event_version,
                                 uint8_t    event_id,
                                 uint8_t    event_class,
                                 uint16_t   event_state,
                                 uint8_t    data_size);
};

static_assert(Constants::BufferSize == sizeof(mctp::Packet), "invalid Buffer Size");

}  // namespace nv::mctp
