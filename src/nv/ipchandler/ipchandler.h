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
#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"
#include "nv/ipchandler/enums.h"
#include "nv/i3c/task.h"
namespace nv::ipchandler {

constexpr auto PktDataLen = 256;

struct Header
{
    Id       src_id;
    Id       dst_id;
    uint16_t size;
    uint8_t  is_request : 1;
    uint8_t  rsv        : 7;
};

class Driver
{
public:
    static Status
    send(Id src_id, Id dst_id, nv::ipc::Queue::Item item, uint16_t size, bool is_request);

private:
    Status construct_packet(Header& hdr, Id src_id, Id dst_id, uint16_t size, bool is_request);
    Status handler(const Header hdr, nv::ipc::Queue::Item item);
    void   dump_packet(const Header hdr, nv::ipc::Queue::Item item);

    Status to_i3c(nv::ipc::Queue::Item item, nv::ipc::QueueId client);
    Status to_i2c(nv::ipc::Queue::Item item, nv::ipc::QueueId client);
    Status to_usb(nv::ipc::Queue::Item item);
    Status to_pldm(nv::ipc::Queue::Item item);
};

}  // namespace nv::ipchandler
