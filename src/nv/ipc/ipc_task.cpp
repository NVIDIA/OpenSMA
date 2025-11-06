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
#include "nv/ipc/ipc_task.h"
#include "nv/ipc/driver.h"
#include "nv/common/preproc.h"
#include "nv/logger/log.h"
#include "nv/ipc/streambuffer.h"
#include <cstring>
#include "nv/nv.h"
#include "sys/ipc/mcmgr_wrapper.h"
using namespace std::chrono_literals;
using namespace nv::ipc;
namespace nv::ipc::task {
void Task::make(Config config)
{
    constexpr auto StackSize = std::max(640, int(configMINIMAL_STACK_SIZE));

    NV_TASK_DATA static Task                       task(config);
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));

    // Decide priority
    task.setup(stack.span(), Priv, Priority::Ipc, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

Task::Task(Config config) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, _core1_image_address(config.core1_image_address)
, _shared_base_c2c_memory_address(config.shared_base_c2c_memory_address)
, _driver()
{
    static_assert(sys::ipc::task::C2CBufferSize == 0
                  || (sys::ipc::task::C2CBufferSize
                      == sys::ipc::StreamBufferRingOverhead
                             + sys::ipc::task::C2CUsbPldmUpdate4KSize
                             + sys::ipc::task::C2CBufferExtraSpace));

    // Core1: Get startup data from core0, setup interrupt callback, notify core0 that core1 is
    // ready to receive interrupt, setup c2c communication
    // Core1 setup here is to allow core1 could start send data to core0 via c2c
    if (nv::ipc::get_current_core() == nv::ipc::CoreId::Core1) {
        auto status = Driver::init(_shared_base_c2c_memory_address, _core1_image_address);
        if (status != task::Status::Ok) {
            // TODO: Add interrupt to notify core0 that core1 init failed
            // coverity[no_escape] should never leave here - Core1 init failed
            while (true) {}
        }
        else {
            nv::logger::info(nv::logger::Event::IpcTaskInitSuccess,
                             {static_cast<uint8_t>(nv::ipc::get_current_core())});
        }
    }
}

void Task::start()
{
    // Core0: Setup interrupt callback and start core1
    // Core0 setup here is to ensure AHB config is setup before core1 boot
    if (nv::ipc::get_current_core() == nv::ipc::CoreId::Core0) {
        (void)Driver::init(_shared_base_c2c_memory_address, _core1_image_address);
    }

    // coverity[no_escape] should never leave here
    while (true) {
        uint32_t read_size = 0;
        // c2c read will have timeout
        auto status = _driver.read(_buffer, read_size, C2CReceiveTimeout);
        // We allow failure here, since it may not read any data if c2c is empty
        if (status != task::Status::Ok) {
            continue;
        }
        // If c2c has data, handle it
        (void)handle_data_read(read_size);
    }
}

Status Task::handle_data_read(uint32_t read_size)
{
    // Case: Have read a queue request from previous c2c read
    if (_is_queue_item_data_pending == true) {
        _is_queue_item_data_pending = false;
        // Check if queue_id is valid
        if (!nv::common::is_in_range(_queue_request.queue_id)) {
            return task::Status::InvalidParameter;
        }
        // Create Queue::Item from _buffer based on the
        // _queue_request->length
        const auto item = ipc::Queue::Item(_buffer.begin(),
                                           _buffer.begin() + _queue_request.length);

        // Handle Queue request data from _buffer
        if (_queue_request.is_front) {
            auto queue_status = ipc::Queue::make(_queue_request.queue_id)
                                    .send_front(item, SendQueueTimeout);
            if (queue_status != ipc::Queue::Status::Ok) {
                return task::Status::QueueSendFrontFailed;
            }
        }
        else {
            auto queue_status = ipc::Queue::make(_queue_request.queue_id)
                                    .send(item, SendQueueTimeout);
            if (queue_status != ipc::Queue::Status::Ok) {
                return task::Status::QueueSendFailed;
            }
        }
        return task::Status::Ok;
    }

    // All Request has the same size
    if (read_size != sizeof(Request)) {
        return task::Status::InvalidParameter;
    }

    Request request{};

    // Copy Request from _buffer
    memcpy(&request, _buffer.begin(), sizeof(Request));

    // Case 1: Queue request
    if (auto* queue_request = std::get_if<QueueRequest>(&request)) {
        // There will be a subsequence c2c read. Save queue request data
        _is_queue_item_data_pending = true;
        _queue_request              = *queue_request;
    }
    // Case 2: Event request
    else if (auto* event_request = std::get_if<EventRequest>(&request)) {
        // Check if event_id is valid
        if (!nv::common::is_in_range(event_request->event_id)) {
            return task::Status::InvalidParameter;
        }
        if (event_request->is_set) {
            // Set event bits
            auto event_status = ipc::Event::make(event_request->event_id)
                                    .set(event_request->bits);
            if (event_status != ipc::Event::Status::Ok) {
                return task::Status::EventSetFailed;
            }
        }
        else {
            // Clear event bits
            auto event_status = ipc::Event::make(event_request->event_id)
                                    .clear(event_request->bits);
            if (event_status != ipc::Event::Status::Ok) {
                return task::Status::EventClearFailed;
            }
        }
    }
    else {
        return task::Status::InvalidVariant;
    }
    return task::Status::Ok;
}

Status
Task::handle_queue_data(const ipc::Queue::ConstItem& item, ipc::QueueId queue_id, bool is_front)
{
    // Check if the c2c handle is set
    auto handle = Driver::get_c2c_handle(true);
    if (handle == nullptr) {
        return nv::ipc::task::Status::C2CHandleNotSet;
    }
    // Prepare Queue request data
    const Request request = QueueRequest{
        static_cast<uint16_t>(item.size() & UINT16_MAX), is_front, queue_id};

    // Write Queue request to c2c
    auto status = sys::ipc::task::Driver::write_queue_request(request, item);
    if (status != task::Status::Ok) {
        return status;
    }

    // Notify the peer core that the write is done

    status = sys::ipc::task::Mcmgr::trigger_event_force(
        nv::ipc::get_peer_core(),
        task::EventType::Communication,
        static_cast<uint16_t>(task::CmdCode::InterCoreSendWriteDone));
    if (status != task::Status::Ok) {
        return status;
    }

    return task::Status::Ok;
}

Status Task::handle_event_data(ipc::EventId event_id, bool is_set, uint32_t event_bits)
{
    // Check if the c2c handle is set
    auto handle = Driver::get_c2c_handle(true);
    if (handle == nullptr) {
        return nv::ipc::task::Status::C2CHandleNotSet;
    }

    // Prepare Event request data
    const Request request = EventRequest{is_set, event_bits, event_id};

    // Write Event request to c2c
    auto status = sys::ipc::task::Driver::write_event_request(request);
    if (status != task::Status::Ok) {
        return status;
    }

    // Notify the peer core that the write is done
    status = sys::ipc::task::Mcmgr::trigger_event_force(
        nv::ipc::get_peer_core(),
        task::EventType::Communication,
        static_cast<uint16_t>(task::CmdCode::InterCoreSendWriteDone));
    if (status != task::Status::Ok) {
        return status;
    }

    return task::Status::Ok;
}

}  // namespace nv::ipc::task
