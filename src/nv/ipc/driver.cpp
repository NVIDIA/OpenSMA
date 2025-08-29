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
#include "nv/ipc/driver.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/event.h"
#include "nv/ipc/supervisor.h"
#include <bit>
using namespace nv::ipc::task;
using namespace std::chrono_literals;

Status Driver::set_event(ipc::EventId event_id, EventBits event_bits, bool isr)
{
    auto& event  = ipc::Event::make(event_id);
    auto  status = event.set(event_bits, isr);
    if (status != ipc::Event::Status::Ok) {
        return Status::EventSetFailed;
    }
    return Status::Ok;
}

Status Driver::send_start_up_queue(ipc::QueueId queue_id, CmdCode cmd, bool isr)
{
    const Command Cmd{.cmd = cmd};
    auto          cmd_item  = ipc::Queue::Item(std::bit_cast<uint8_t*>(&Cmd), sizeof(Command));
    auto&         cmd_queue = ipc::Queue::make(queue_id);
    auto          queue_status = ipc::Queue::Status::Ok;
    if (isr) {
        queue_status = cmd_queue.send_isr(cmd_item);
    }
    else {
        queue_status = cmd_queue.send(cmd_item, 100ms);
    }
    if (queue_status != ipc::Queue::Status::Ok) {
        return Status::QueueSendFailed;
    }
    return Status::Ok;
}