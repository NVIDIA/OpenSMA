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
#include "nv/bootloader.h"
#if defined(MCU)
#include "fsl_common.h"
#endif

// Shared wire format for Core0-Core1 IPC
#include "nv/ipc/wire_format.h"

// USB Proxy dispatch (for Core1 bare-metal USB)
#if NCSI_ENABLE
#include "nv/usb_proxy/task.h"
#endif

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
        uint32_t read_size = 0;  // NOLINT(misc-const-correctness) output parameter for read()
        // c2c read will have timeout
        auto status = _driver.read(_buffer, read_size, C2CReceiveTimeout);

        if (status != task::Status::Ok) {
            continue;
        }

        // C2C read success - process data
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

#if NCSI_ENABLE
        // Dispatch Core1 USB data (MCTP/HID/ACM) to usb_proxy
        if (nv::usb_proxy::dispatch_c2c_data(
                _queue_request.queue_id, _buffer.data(), _queue_request.length)) {
            return task::Status::Ok;
        }
#endif

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

    // Core1 bare-metal sends a fixed 16-byte structure:
    // offset 0: variant_index (1 byte + 3 padding)
    // offset 4: length (uint16_t)
    // offset 6: is_front (bool)
    // offset 7: padding
    // offset 8: queue_id (uint32_t)
    // offset 12: padding (4 bytes)
    // Total: 16 bytes
    //
    // We parse this directly instead of relying on std::variant layout

    constexpr size_t kBareMetalRequestSize = sizeof(ipc_bm::QueueRequestWire);
    // The write side sends sizeof(Request) bytes; the read side expects exactly
    // kBareMetalRequestSize.  If these differ, ALL IPC communication breaks.
    static_assert(sizeof(Request) == kBareMetalRequestSize,
                  "sizeof(Request) must equal sizeof(QueueRequestWire) for IPC compatibility");
    if (read_size != kBareMetalRequestSize || _buffer.size() < kBareMetalRequestSize) {
        return task::Status::InvalidParameter;
    }

    // Parse wire format using the shared wire format structs.
    // variant_index is at offset 0 in both QueueRequestWire and EventRequestWire.
    // Use data() to avoid clang-tidy array-index warning when C2CBufferSize == 0;
    // the size check above guarantees this access is safe at runtime.
    const uint8_t variant_index = _buffer.data()[0];

    // Case 1: Queue request (variant_index == 0)
    if (variant_index == 0) {
        ipc_bm::QueueRequestWire wire{};
        std::memcpy(&wire, _buffer.data(), sizeof(wire));

        _queue_request.length       = wire.length;
        _queue_request.is_front     = (wire.is_front != 0);
        _queue_request.queue_id     = static_cast<ipc::QueueId>(wire.queue_id);
        _is_queue_item_data_pending = true;
    }
    // Case 2: Event request (variant_index == 1)
    else if (variant_index == 1) {
        ipc_bm::EventRequestWire wire{};
        std::memcpy(&wire, _buffer.data(), sizeof(wire));

        auto event_id = static_cast<ipc::EventId>(wire.event_id);
        if (!nv::common::is_in_range(event_id)) {
            return task::Status::InvalidParameter;
        }
        if (wire.is_set != 0) {
            auto event_status = ipc::Event::make(event_id).set(wire.bits);
            if (event_status != ipc::Event::Status::Ok) {
                return task::Status::EventSetFailed;
            }
        }
        else {
            auto event_status = ipc::Event::make(event_id).clear(wire.bits);
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

    // Build wire-format request that Core1 bare-metal can parse.
    // Cannot use std::variant<QueueRequest> directly because its binary layout
    // (variant index position) is compiler-dependent and may not match
    // the QueueRequestWire format that Core1 expects.
    ipc_bm::QueueRequestWire bm_request = {};
    bm_request.variant_index            = 0;  // QueueRequest
    bm_request.length                   = static_cast<uint16_t>(item.size() & UINT16_MAX);
    bm_request.is_front                 = is_front ? 1 : 0;
    bm_request.queue_id                 = static_cast<uint32_t>(queue_id);

    // Wrap in Request-sized container for write_queue_request() SVC path.
    // write_queue_request_svc() handles both ISR context (direct privileged call)
    // and unprivileged thread context (SVC escalation).
    static_assert(sizeof(Request) >= sizeof(bm_request), "Request too small for wire format");
    Request request{};
    std::memcpy(&request, &bm_request, sizeof(bm_request));

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

    // Build wire-format request that Core1 bare-metal can parse.
    // Cannot use std::variant<EventRequest> directly because its binary layout
    // (variant index position) is compiler-dependent and may not match
    // the EventRequestWire format that Core1 expects.
    ipc_bm::EventRequestWire bm_request = {};
    bm_request.variant_index            = 1;  // EventRequest
    bm_request.is_set                   = is_set ? 1 : 0;
    bm_request.bits                     = event_bits;
    bm_request.event_id                 = static_cast<uint32_t>(event_id);

    // Wrap in Request-sized container for write_event_request() SVC path.
    static_assert(sizeof(Request) >= sizeof(bm_request), "Request too small for wire format");
    Request request{};
    std::memcpy(&request, &bm_request, sizeof(bm_request));

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
