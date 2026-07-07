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
#include <cstdint>

#include "nv/common/utils.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/ipc/timer.h"
#include "nv/mctp/interface.h"
#include "corepdk/platforms/mcxn236/pldm-fd/src/pldm_wrap.h"
#include "nv/ipchandler/ipchandler.h"

namespace nv::pldm {

static constexpr size_t PldmBufferSize = 68;

using PldmBuffer = std::array<uint8_t, PldmBufferSize>;

enum class Status
{
    Ok,             ///< Success
    QueueSendFail,  ///< queue send return fail
    EventSetFail,   /// event set fail
    Unknown,        ///< An unknown error occurred.
};

enum class RequestType : uint8_t
{
    AuthMcuFinish,
    AuthApFinish
};

struct [[gnu::packed]] Request
{
    RequestType type;
    uint8_t     length;
    uint8_t     rsv1;
    uint8_t     rsv2;
    PldmBuffer  buffer;
};

class Task : public nv::ipc::Task
{
public:
    Task();

    enum class PldmTask : uint16_t
    {
        Rx            = 0,
        Rx4k          = 1,
        Timer         = 2,
        BgStart       = 3,
        BgEnd         = 4,
        WdtEvent      = 5,
        EventRequest  = 6,
        ProtocolReset = 7,
        ApBgStart     = 8,
    };

    enum PldmEventBits : uint32_t
    {
        MctpNoEventBit   = 0,
        PldmTaskRxBit    = common::bit(nv::pldm::Task::PldmTask::Rx),
        PldmTaskRx4kBit  = common::bit(nv::pldm::Task::PldmTask::Rx4k),
        TimerBit         = common::bit(nv::pldm::Task::PldmTask::Timer),
        BgStartBit       = common::bit(nv::pldm::Task::PldmTask::BgStart),
        BgEndBit         = common::bit(nv::pldm::Task::PldmTask::BgEnd),
        WdtEventBit      = common::bit(nv::pldm::Task::PldmTask::WdtEvent),
        EventRequestBit  = common::bit(nv::pldm::Task::PldmTask::EventRequest),
        ProtocolResetBit = common::bit(nv::pldm::Task::PldmTask::ProtocolReset),
        ApBgStartBit     = common::bit(nv::pldm::Task::PldmTask::ApBgStart),
    };

    static void make();
    static void entrypoint(void* params);
    void        left_shift_array(std::span<NvU8> arr, std::size_t shift);
    void        get_interface_info(PldmContextRecord&   pldm_context,
                                   PldmfwInterfaceInfo& interface_info,
                                   uint16_t&            size);
    static bool is_background_copy_automatic(bool log_init_setup);

    [[noreturn]] void main();

private:
    nv::ipc::Event& _event;
    nv::ipc::Queue& _rx;
    nv::ipc::Queue& _rx4k;
    nv::ipc::Timer& _timer;

public:  // static api section
    /**
     * Send pldm msg to pldm task
     *
     * @param[in]     packet            pldm msg
     *
     * @return nv::pldm::Status::Ok on no errors.
     */
    static nv::pldm::Status pldm_tx(const nv::mctp::Packet& packet);
    static nv::pldm::Status pldm_bg_start();
    static nv::pldm::Status pldm_ap_bg_start(uint16_t component_id);
    static uint16_t         latest_background_copy_component_id();
    static void             on_timer([[maybe_unused]] ipc::Timer& id);
    static void             set_timer(std::chrono::microseconds ms);
    static void             clear_timer();
    static uint32_t         get_ticks();
    static void             wdt_notify();
    static bool             to_pldm(ipchandler::Id src_id, Request request);
    static void             protocol_reset();
};

}  // namespace nv::pldm
