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
#include <cstdint>
#include <variant>
#include "nv/common/utils.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc::task {

// For interrupt event data
enum class CmdCode : uint16_t
{
    InterCoreSendWriteDone,
    Core1Ready,
    InterCoreAcmTxDone,
    End,
};

struct QueueRequest
{
    uint16_t length;
    bool     is_front;
    QueueId  queue_id;
};

struct EventRequest
{
    bool     is_set;  // indicate set event or clear event
    uint32_t bits;
    // EventId for events; for task notify use (EventId::End + TaskId) so event_id extends
    // beyond EventId::End
    uint32_t event_id;
};

using Request = std::variant<QueueRequest, EventRequest>;

// Will be converted to mcmgr event, for C2C communication interrupt
enum class EventType : uint32_t
{
    Communication,
    Core1Ready,
    Core0Fault,
    Core1Fault,
};

enum class Status : uint32_t
{
    Ok = 0,
    C2CHandleNotSet,
    QueueSendFailed,
    QueueSendFrontFailed,
    EventSetFailed,
    EventClearFailed,
    InvalidVariant,
    InvalidParameter,
    McmgrError,
    McmgrNotReady,
    McmgrNotImplemented,
    C2CIdInvalid,
    C2CSendFailed,
    C2CRecvFailed,
    C2CSendSizeOverflow,
    C2CHasNoEnoughSpace,
    Error,
};

}  // namespace nv::ipc::task