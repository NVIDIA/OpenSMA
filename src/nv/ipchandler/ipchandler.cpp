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
#include "nv/ipchandler/ipchandler.h"
#include "nv/logger/log.h"
using namespace nv;
using namespace ipchandler;

// #define NV_IPCHANDLER_DEBUG 1

Status
Driver::send(Id src_id, Id dst_id, nv::ipc::Queue::Item item, uint16_t size, bool is_request)
{
    Header hdr{};
    Driver driver_instance;
    auto   status = driver_instance.construct_packet(hdr, src_id, dst_id, size, is_request);
    if (status != Status::Success) {
        return status;
    }

    driver_instance.dump_packet(hdr, item);

    status = driver_instance.handler(hdr, item);

    return status;
}

Status
Driver::construct_packet(Header& hdr, Id src_id, Id dst_id, uint16_t size, bool is_request)
{
    hdr.src_id     = src_id;
    hdr.dst_id     = dst_id;
    hdr.size       = size;
    hdr.is_request = is_request ? 1 : 0;

    return Status::Success;
}

Status Driver::handler(const Header hdr, nv::ipc::Queue::Item item)
{
    auto status = Status::Success;

    switch (hdr.dst_id) {
        case Id::I3c0:
            status = to_i3c(item, nv::i3c::Task::GetQueueFromIpchandlerId<Id::I3c0>());
            break;
        case Id::I3c1:
            status = to_i3c(item, nv::i3c::Task::GetQueueFromIpchandlerId<Id::I3c1>());
            break;
        case Id::I2c1: status = to_i2c(item, nv::ipc::QueueId::I2c1); break;
        case Id::I2c2: status = to_i2c(item, nv::ipc::QueueId::I2c2); break;
        case Id::I2c4: status = to_i2c(item, nv::ipc::QueueId::I2c4); break;
        case Id::I2c5: status = to_i2c(item, nv::ipc::QueueId::I2c5); break;
        case Id::Usb : {
            status = to_usb(item);
        } break;
        case Id::Pldm: {
            status = to_pldm(item);
        } break;
        default: {
            return Status::UnknownTask;
        }
    }

    return status;
}

Status Driver::to_i3c(nv::ipc::Queue::Item item, nv::ipc::QueueId queue_id)
{
    nv::ipc::Queue& queue  = nv::ipc::Queue::make(queue_id);
    auto            status = queue.send(item);
    if (status != nv::ipc::Queue::Status::Ok) {
        nv::info("queue status = %d\n", status);
        return Status::SendQueueFail;
    }
    return Status::Success;
}

Status Driver::to_i2c(nv::ipc::Queue::Item item, nv::ipc::QueueId queue_id)
{
    nv::ipc::Queue& queue  = nv::ipc::Queue::make(queue_id);
    auto            status = queue.send(item);
    if (status != nv::ipc::Queue::Status::Ok) {
        nv::info("queue status = %d\n", status);
        return Status::SendQueueFail;
    }
    return Status::Success;
}

Status Driver::to_usb(nv::ipc::Queue::Item item)
{
    nv::ipc::Queue& queue  = nv::ipc::Queue::make(nv::ipc::QueueId::UsbHid);
    auto            status = queue.send(item);

    if (status != nv::ipc::Queue::Status::Ok) {
        nv::info("queue status = %d\n", status);
        return Status::SendQueueFail;
    }
    return Status::Success;
}

Status Driver::to_pldm(nv::ipc::Queue::Item item)
{
    nv::ipc::Queue& queue  = nv::ipc::Queue::make(nv::ipc::QueueId::PldmRx);
    auto            status = queue.send(item);

    if (status != nv::ipc::Queue::Status::Ok) {
        return Status::SendQueueFail;
    }
    return Status::Success;
}

void Driver::dump_packet(const Header hdr, nv::ipc::Queue::Item item)
{
#ifdef NV_IPCHANDLER_DEBUG
    nv::info("src [0x%x] dst [0x%x] size [0x%x] is_request[0x%x]\n",
             hdr.src_id,
             hdr.dst_id,
             hdr.size,
             hdr.is_request);
    for (uint32_t i = 0; i < 64; i += 4) {
        nv::info("0x%x: 0x%x 0x%x 0x%x 0x%x\n",
                 i,
                 item[i + 0],
                 item[i + 1],
                 item[i + 2],
                 item[i + 3]);
    }
#endif
}
