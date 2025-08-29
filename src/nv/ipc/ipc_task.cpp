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
, _event_id(config.event_id)
, _start_up_queue_id(config.start_up_queue_id)
, _core1_image_address(config.core1_image_address)
, _shared_base_stream_buffer_address(config.shared_base_stream_buffer_address)
, _shared_stream_buffer_size(config.shared_stream_buffer_size)
, _peer_core_is_ready(false)
, _driver()
{
    static_assert(sys::ipc::StreamBufferSize == 0
                  || (sys::ipc::StreamBufferSize
                      == sys::ipc::StreamBufferInternalAlignment + sizeof(size_t)
                             + sizeof(task::Request) + sizeof(size_t) + (4096 + 32)));
}

void Task::set_up_core_communication()
{
    auto status = Driver::init(_shared_base_stream_buffer_address, _core1_image_address);
    if (status != task::Status::Ok) {
        debug_core("[FAILURE] init failed\n");
    }
    const Command Cmd{};
    auto          cmd_item = ipc::Queue::Item(std::bit_cast<uint8_t*>(&Cmd), sizeof(Cmd));

    while (_peer_core_is_ready == false) {
        auto cmd_status = ipc::Queue::make(_start_up_queue_id).recv(cmd_item, 1s);

        if (cmd_status == ipc::Queue::Status::Ok) {
            switch (Cmd.cmd) {
                case CmdCode::Core0Ready: {
                    _peer_core_is_ready = true;
                    task::Driver::set_core_communication_ready(true);
                    break;
                }

                case CmdCode::Core1Ready: {
                    auto status = task::Driver::notify(
                        task::EventType::Communication,
                        static_cast<uint16_t>(task::CmdCode::Core0Ready));
                    if (status != task::Status::Ok) {
                        debug_core("[FAILURE] notify Core0Ready failed\n");
                        break;
                    }
                    _peer_core_is_ready = true;
                    task::Driver::set_core_communication_ready(true);
                    break;
                }
                default:
                    debug_core("[FAILURE] received unknown command: %d\n",
                               static_cast<uint16_t>(Cmd.cmd));
                    break;
            }
        }
    }
    debug_core("[SETUP]Task start() peer core is ready\n");
    debug_core("[SETUP]Task start() Driver::get_shared_base_stream_buffer_address(): 0x%x\n",
               Driver::get_shared_base_stream_buffer_address());
    debug_core("[SETUP]Task start() Driver::get_shared_stream_buffer_size(): 0x%x\n",
               _shared_stream_buffer_size);
}

void Task::start()
{
    set_up_core_communication();

    // coverity[no_escape] should never leave here
    while (true) {
        auto wait         = EventBits::InterCoreSendWriteDoneEvent;
        auto event        = ipc::Event::make(_event_id);
        auto event_status = event.wait(wait, false, false, 1s);
        auto event_bits   = event_status.value();

        // Wait for the peer core to write done
        if (event_bits & EventBits::InterCoreSendWriteDoneEvent) {
            // Read data from shared memory
            uint32_t read_size = 0;
            auto     status    = _driver.read(_buffer, read_size);
            if (status == task::Status::Ok) {
                // Handle data read from shared memory
                status = handle_data_read(read_size);
                if (status != task::Status::Ok) {
                    debug_core("[FAILURE] handle_data_read failed %d\n",
                               static_cast<uint16_t>(status));
                }
            }
            // To check if stream buffer is empty
            // If no data to read, clear the event
            auto bytes_available = Driver::bytes_available_to_read(false);
            if (bytes_available == 0) {
                (void)event.clear(EventBits::InterCoreSendWriteDoneEvent);
            }
        }

        // To check if stream buffer is not empty
        // If have data to read, set the event
        else {
            auto bytes_available = Driver::bytes_available_to_read(false);
            if (bytes_available > 0) {
                (void)event.set(EventBits::InterCoreSendWriteDoneEvent);
            }
        }
    }
}

Status Task::handle_data_read(uint32_t read_size)
{
    // When it is queue request, will have two stream buffer read in one time
    if (_is_queue_item_data_pending == true) {
        _is_queue_item_data_pending = false;
        if (auto* queue_request = std::get_if<QueueRequest>(&_request)) {
            if (nv::common::align_to(queue_request->length, sys::ipc::StreamBufferAlignment)
                != read_size) {
                return task::Status::InvalidParameter;
            }
            else {
                // Create Queue::Item from _buffer based on the
                // queue_request->length
                const auto item = ipc::Queue::Item(_buffer.begin(),
                                                   _buffer.begin() + queue_request->length);

                // Check if queue_id is valid
                if (!nv::common::is_in_range(queue_request->queue_id)) {
                    return task::Status::InvalidParameter;
                }
                // Handle Queue request data from _buffer
                if (queue_request->is_front) {
                    // TODO: Decide the timeout value
                    auto queue_status = ipc::Queue::make(queue_request->queue_id)
                                            .send_front(item, 1s);
                    if (queue_status != ipc::Queue::Status::Ok) {
                        return task::Status::QueueSendFrontFailed;
                    }
                }
                else {
                    // TODO: Decide the timeout value
                    auto queue_status = ipc::Queue::make(queue_request->queue_id)
                                            .send(item, 1s);
                    if (queue_status != ipc::Queue::Status::Ok) {
                        return task::Status::QueueSendFailed;
                    }
                }
            }
        }
        else {
            return task::Status::InvalidVariant;
        }
        return task::Status::Ok;
    }

    Request request{};

    // All Request has the same size
    if (read_size != sizeof(Request)) {
        return task::Status::InvalidParameter;
    }
    // Read Request from _buffer
    memcpy(&request, _buffer.begin(), sizeof(Request));

    if (std::get_if<QueueRequest>(&request)) {
        // There will be a subsequence stream buffer read. Save queue request data
        _is_queue_item_data_pending = true;
        _request                    = request;
    }
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
    // Check if the core communication is ready
    if (Driver::get_core_communication_ready() == false) {
        return task::Status::CommunicationNotReady;
    }

    // Prepare Queue request data
    const Request request = QueueRequest{
        static_cast<uint16_t>(item.size() & UINT16_MAX), is_front, queue_id};

    // Write Queue request to stream buffer
    auto status = sys::ipc::task::Driver::write_queue_request(request, item);
    if (status != task::Status::Ok) {
        return status;
    }

    // Notify the peer core that the write is done
    status = task::Driver::notify(task::EventType::Communication,
                                  static_cast<uint16_t>(task::CmdCode::InterCoreSendWriteDone));
    if (status != task::Status::Ok) {
        return status;
    }

    return task::Status::Ok;
}

Status Task::handle_event_data(ipc::EventId event_id, bool is_set, uint32_t event_bits)
{
    // Check if the core communication is ready
    if (Driver::get_core_communication_ready() == false) {
        return task::Status::CommunicationNotReady;
    }

    // Prepare Event request data
    const Request request = EventRequest{is_set, event_bits, event_id};

    // Write Event request to stream buffer
    auto status = sys::ipc::task::Driver::write_event_request(request);
    if (status != task::Status::Ok) {
        return status;
    }

    // Notify the peer core that the write is done
    status = task::Driver::notify(task::EventType::Communication,
                                  static_cast<uint16_t>(task::CmdCode::InterCoreSendWriteDone));
    if (status != task::Status::Ok) {
        return status;
    }

    return task::Status::Ok;
}

}  // namespace nv::ipc::task
