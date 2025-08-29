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

enum class Event : uint32_t
{
    Begin                  = 0,
    InterCoreSendWriteDone = Begin,
    End,
};

enum EventBits : uint32_t
{
    EventBitsNone               = 0,
    InterCoreSendWriteDoneEvent = nv::common::bit(Event::InterCoreSendWriteDone),
};

// For setup core communication queue
enum class CmdCode : uint16_t
{
    InterCoreSendWriteDone = static_cast<uint16_t>(Event::InterCoreSendWriteDone),
    Core0Ready,
    Core1Ready,
    End,
};

// For setup core communication
struct Command
{
    CmdCode cmd;
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
    EventId  event_id;
};

using Request = std::variant<QueueRequest, EventRequest>;

// For mcmgr event
enum class EventType : uint32_t
{
    Communication,
};

enum class Status : uint32_t
{
    Ok = 0,
    CommunicationNotReady,
    QueueSendFailed,
    QueueSendFrontFailed,
    EventSetFailed,
    EventClearFailed,
    InvalidVariant,
    InvalidParameter,
    McmgrError,
    McmgrNotReady,
    McmgrNotImplemented,
    StreamBufferIdInvalid,
    StreamBufferSendFailed,
    StreamBufferRecvFailed,
    StreamBufferSendSizeOverflow,
    StreamBufferHasNoEnoughSpace,
    Error,
};

}  // namespace nv::ipc::task