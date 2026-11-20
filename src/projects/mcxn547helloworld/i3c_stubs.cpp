/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/i3c/task.h"
#include "nv/watchdog/notify_interface.h"

namespace nv::i3c {

uint8_t initial_endpoint_status([[maybe_unused]] nv::mctp::Client client)
{
    return 0;
}

Task::Status Task::tx([[maybe_unused]] const nv::mctp::Packet& packet)
{
    return Status::InvalidInterface;
}

Task::Status Task::update_routing_table([[maybe_unused]] nv::mctp::Client client)
{
    return Status::InvalidInterface;
}

Task::Status Task::wdt_notify([[maybe_unused]] nv::watchdog::TaskMonitorIndex taskId)
{
    return Status::Ok;
}

Task::Status Task::init_bus([[maybe_unused]] nv::mctp::Client client,
                            [[maybe_unused]] bool             init)
{
    return Status::InvalidInterface;
}

bool Task::to_i2c([[maybe_unused]] ipchandler::Id     src_id,
                  [[maybe_unused]] uint8_t            address,
                  [[maybe_unused]] nv::ipchandler::Id i2c_ipchandler_id,
                  [[maybe_unused]] uint16_t           write_length,
                  [[maybe_unused]] uint16_t           read_length,
                  [[maybe_unused]] ipc::Queue::Item&  item)
{
    return false;
}

}  // namespace nv::i3c
