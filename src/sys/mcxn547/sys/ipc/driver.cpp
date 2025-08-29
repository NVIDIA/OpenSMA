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
#include "nv/ctimer/ctimer.h"
#include "nv/ipc/common.h"
#include "nv/ipc/ipc_task.h"
#include "nv/ipc/supervisor.h"
#include "mpu_syscall_numbers.h"
#include "nv/common/utils.h"
#include <cstring>
using namespace nv::ipc::task;

namespace {
nv::ipc::task::Status kstatus_to_status(mcmgr_status_t sts)
{
    switch (sts) {
        case kStatus_MCMGR_Success       : return nv::ipc::task::Status::Ok;
        case kStatus_MCMGR_Error         : return nv::ipc::task::Status::McmgrError;
        case kStatus_MCMGR_NotImplemented: return nv::ipc::task::Status::McmgrNotImplemented;
        case kStatus_MCMGR_NotReady      : return nv::ipc::task::Status::McmgrNotReady;
        default                          : return nv::ipc::task::Status::Error;
    }
    return nv::ipc::task::Status::Error;
}

mcmgr_event_type_t event_type_to_kMCMGR_event(nv::ipc::task::EventType event)
{
    // kMCMGR means non-sense, since it is defined in SDK mcmgr.h
    switch (event) {
        case nv::ipc::task::EventType::Communication: return kMCMGR_RemoteApplicationEvent;
        default                                     : return kMCMGR_RemoteApplicationEvent;
    }
}

}  // namespace

#if defined(__cplusplus)
extern "C" {
#endif
NV_SHARED_DATA uint32_t shared_base_stream_buffer_address = 0;
NV_SHARED_DATA bool     core_communication_ready          = false;
NV_SHARED_DATA uint16_t C2CEventData                      = 0U;
void                    C2CEventHandler(uint16_t event_data, void* context)
{
    switch (event_data) {
        case (static_cast<uint16_t>(nv::ipc::task::CmdCode::Core0Ready)): {
            nv::ipc::task::Driver::send_start_up_queue(
                nv::ipc::QueueId::IpcTaskStartUp, nv::ipc::task::CmdCode::Core0Ready, true);
            break;
        }
        case (static_cast<uint16_t>(nv::ipc::task::CmdCode::Core1Ready)): {
            nv::ipc::task::Driver::send_start_up_queue(
                nv::ipc::QueueId::IpcTaskStartUp, nv::ipc::task::CmdCode::Core1Ready, true);
            break;
        }
        case (static_cast<uint16_t>(nv::ipc::task::CmdCode::InterCoreSendWriteDone)): {
            // Callback function to notify write done
            nv::ipc::task::Driver::set_event(
                nv::ipc::EventId::IpcTask,
                nv::ipc::task::EventBits::InterCoreSendWriteDoneEvent,
                true);
            break;
        }
        default: {
            break;
        }
    }
}

NV_PRIVILEGED_FUNCTION nv::ipc::task::Status
                       IPC_Write_Queue_Request_Priv(const nv::ipc::task::Request&    request,
                                                    const nv::ipc::Queue::ConstItem& item)
{
    taskENTER_CRITICAL();
    auto status = sys::ipc::task::Driver::write_queue_request_impl(request, item);
    taskEXIT_CRITICAL();
    return status;
}

NV_PRIVILEGED_FUNCTION nv::ipc::task::Status
                       IPC_Write_Event_Request_Priv(const nv::ipc::task::Request& request)
{
    taskENTER_CRITICAL();
    auto status = sys::ipc::task::Driver::write_event_request_impl(request);
    taskEXIT_CRITICAL();
    return status;
}
#if defined(__cplusplus)
}
#endif /* __cplusplus*/

uint32_t Driver::get_shared_base_stream_buffer_address()
{
    return shared_base_stream_buffer_address;
}

void Driver::set_shared_base_stream_buffer_address(uint32_t address)
{
    shared_base_stream_buffer_address = address;
}

nv::ipc::task::Status Driver::init(uint32_t shared_base_stream_buffer_address,
                                   uint32_t core1_image_address)
{
    auto core = sys::ipc::task::Driver::get_current_core();
    if (core == nv::ipc::CoreId::Core0) {
        nv::ipc::task::Status status = nv::ipc::task::Status::Ok;
        // Register event for c2c interrupt
        status = nv::ipc::task::Driver::register_event(nv::ipc::task::EventType::Communication);
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        set_shared_base_stream_buffer_address(shared_base_stream_buffer_address);

        auto mcmgr_status = MCMGR_StartCore(kMCMGR_Core1,
                                            (void*)(char*)(core1_image_address),
                                            shared_base_stream_buffer_address,
                                            kMCMGR_Start_Asynchronous);

        if (mcmgr_status != kStatus_MCMGR_Success) {
            return kstatus_to_status(mcmgr_status);
        }

        return nv::ipc::task::Status::Ok;
    }
    // coverity[dead_error_line] - Expect cannot reach here in core0
    else if (core == nv::ipc::CoreId::Core1) {
        uint32_t       startupData  = 0;
        mcmgr_status_t mcmgr_status = kStatus_MCMGR_Success;

        // Get startup data from core0: core0 will send shared_base_stream_buffer_address
        do {
            mcmgr_status = MCMGR_GetStartupData(&startupData);
        } while (mcmgr_status != kStatus_MCMGR_Success);

        // Set shared_base_stream_buffer_address
        set_shared_base_stream_buffer_address(startupData);

        // Register event for c2c interrupt
        nv::ipc::task::Status status = nv::ipc::task::Status::Ok;
        status = nv::ipc::task::Driver::register_event(nv::ipc::task::EventType::Communication);
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        for (int i = 0; i < static_cast<int>(nv::ipc::StreamBufferId::End); i++) {
            auto& stream_buffer = nv::ipc::StreamBuffer::make(
                static_cast<nv::ipc::StreamBufferId>(i),
                false,
                get_shared_base_stream_buffer_address(),
                true);
            nv::ipc::StreamBuffer::set_stream_buffer_handle(
                static_cast<nv::ipc::StreamBufferId>(i),
                stream_buffer.get_stream_buffer_handle_by_core());
        }

        // Notify core0 that core1 is ready
        status = nv::ipc::task::Driver::notify(
            nv::ipc::task::EventType::Communication,
            static_cast<uint16_t>(nv::ipc::task::CmdCode::Core1Ready));
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }
        return nv::ipc::task::Status::Ok;
    }
    else {
        return nv::ipc::task::Status::InvalidParameter;
    }
}

nv::ipc::task::Status Driver::write(const std::span<const uint8_t> data)
{
    auto handle = get_stream_buffer_handle(true);
    if (handle == nullptr) {
        return nv::ipc::task::Status::StreamBufferIdInvalid;
    }

    auto res = nv::ipc::StreamBuffer::send(handle, data);

    // Check if the data is sent successfully
    if (res != nv::common::align_to(data.size(), sys::ipc::StreamBufferAlignment)) {
        return nv::ipc::task::Status::StreamBufferSendFailed;
    }

    return nv::ipc::task::Status::Ok;
}

nv::ipc::task::Status Driver::read(std::span<uint8_t> data, uint32_t& read_size)
{
    auto handle = get_stream_buffer_handle(false);
    if (handle == nullptr) {
        return nv::ipc::task::Status::StreamBufferIdInvalid;
    }

    auto recv_size = nv::ipc::StreamBuffer::recv(handle, data);
    read_size      = recv_size;

    // Check if the data is received successfully
    if (recv_size == 0) {
        return nv::ipc::task::Status::StreamBufferRecvFailed;
    }

    return nv::ipc::task::Status::Ok;
}

size_t Driver::bytes_available_to_read(bool is_tx)
{
    auto handle = get_stream_buffer_handle(is_tx);
    if (handle == nullptr) {
        return 0;
    }

    auto res = nv::ipc::StreamBuffer::bytes_available_to_read(handle);

    return res;
}

size_t Driver::bytes_available_to_write(bool is_tx)
{
    auto handle = get_stream_buffer_handle(is_tx);
    if (handle == nullptr) {
        return 0;
    }

    auto res = nv::ipc::StreamBuffer::bytes_available_to_write(handle);

    return res;
}

nv::ipc::task::Status Driver::notify(EventType event, uint16_t send_data)
{
    mcmgr_status_t status = MCMGR_TriggerEvent(event_type_to_kMCMGR_event(event), send_data);
    return kstatus_to_status(status);
}

nv::ipc::task::Status Driver::register_event(EventType event)
{
    mcmgr_status_t status = MCMGR_RegisterEvent(
        event_type_to_kMCMGR_event(event), C2CEventHandler, (void*)&C2CEventData);
    return kstatus_to_status(status);
}

nv::ipc::StreamBuffer::IdType Driver::get_stream_buffer_id(bool is_tx)
{
    const nv::ipc::CoreId current_core = sys::ipc::task::Driver::get_current_core();
    if (current_core == nv::ipc::CoreId::Core0) {
        if (is_tx) {
            return nv::ipc::StreamBufferId::Core0ToCore1;
        }
        else {
            return nv::ipc::StreamBufferId::Core1ToCore0;
        }
    }
    else if (current_core == nv::ipc::CoreId::Core1) {
        if (is_tx) {
            return nv::ipc::StreamBufferId::Core1ToCore0;
        }
        else {
            return nv::ipc::StreamBufferId::Core0ToCore1;
        }
    }
    else {
        return nv::ipc::StreamBufferId::End;
    }
}

StreamBufferHandle_t Driver::get_stream_buffer_handle(bool is_tx)
{
    return nv::ipc::StreamBuffer::get_stream_buffer_handle(Driver::get_stream_buffer_id(is_tx));
}

bool Driver::get_core_communication_ready()
{
    return core_communication_ready;
}

void Driver::set_core_communication_ready(bool ready)
{
    core_communication_ready = ready;
}

nv::ipc::task::Status
sys::ipc::task::Driver::wait_for_enough_space(size_t size, std::chrono::microseconds timeout)
{
    auto current_time_stamp = nv::ctimer::Driver::read_ticks();
    auto available_space    = nv::ipc::task::Driver::bytes_available_to_write(true);
    while (1) {
        if (available_space >= size) {
            return nv::ipc::task::Status::Ok;
        }
        if (sys::ctimer::Driver::get_counter_difference(current_time_stamp,
                                                        nv::ctimer::Driver::read_ticks())
            > sys::ipc::task::WaitForStreamBufferHasEnoughSpaceTime) {
            return nv::ipc::task::Status::StreamBufferHasNoEnoughSpace;
        }
        available_space = nv::ipc::task::Driver::bytes_available_to_write(true);
    }
    return nv::ipc::task::Status::StreamBufferHasNoEnoughSpace;
}
nv::ipc::task::Status
sys::ipc::task::Driver::write_queue_request(const nv::ipc::task::Request&    request,
                                            const nv::ipc::Queue::ConstItem& item)
{
    return write_queue_request_svc(request, item);
}
NV_SYS_CALL nv::ipc::task::Status
sys::ipc::task::Driver::write_queue_request_svc(const nv::ipc::task::Request&    request,
                                                const nv::ipc::Queue::ConstItem& item)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(
        " .syntax unified                                       \n"
        " .extern IPC_Write_Queue_Request_Priv                       \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_IPC_Write_Queue_Request_Priv                     \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_IPC_Write_Queue_Request_Priv                   \n"
        " Privileged_IPC_Write_Queue_Request_Priv:                        \n"
        "     pop {r0}                                          \n"
        "     b IPC_Write_Queue_Request_Priv                         \n"
        " Unprivileged_IPC_Write_Queue_Request_Priv:                      \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_IPC_Write_Queue_Request)
        : "memory");
#else
    return IPC_Write_Queue_Request_Priv(request, item);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION nv::ipc::task::Status
sys::ipc::task::Driver::write_queue_request_impl(const nv::ipc::task::Request&    request,
                                                 const nv::ipc::Queue::ConstItem& item)
{
    // Check if the size of the item will cause overflow
    if (nv::common::align_to(item.size(), sys::ipc::StreamBufferAlignment)
        > UINT32_MAX - (sizeof(size_t) + sizeof(request) + sizeof(size_t))) {
        return nv::ipc::task::Status::StreamBufferSendSizeOverflow;
    }
    // Check if there is enough space for two streambuffer send
    // 1st message buffer send will write size_t first, then request
    // 2nd message buffer send will write size_t first, then item
    auto status = wait_for_enough_space(
        sizeof(size_t) + sizeof(request) + sizeof(size_t)
            + nv::common::align_to(item.size(), sys::ipc::StreamBufferAlignment),
        std::chrono::microseconds(sys::ipc::task::WaitForStreamBufferHasEnoughSpaceTime));

    if (status == nv::ipc::task::Status::Ok) {
        auto request_span = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                                 sizeof(request));
        // Write request to message buffer
        status = nv::ipc::task::Driver::write(request_span);
        // Write item to message buffer only when 1st message buffer send is successful
        if (status == nv::ipc::task::Status::Ok) {
            status = nv::ipc::task::Driver::write(item);
        }
    }
    // No enough space for two streambuffer send
    else {
        // TODO: Add log?
        return status;
    }
    return status;
}

nv::ipc::task::Status
sys::ipc::task::Driver::write_event_request(const nv::ipc::task::Request& request)
{
    return write_event_request_svc(request);
}
NV_SYS_CALL nv::ipc::task::Status
sys::ipc::task::Driver::write_event_request_svc(const nv::ipc::task::Request& request)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(
        " .syntax unified                                       \n"
        " .extern IPC_Write_Event_Request_Priv                       \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_IPC_Write_Event_Request_Priv                     \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_IPC_Write_Event_Request_Priv                   \n"
        " Privileged_IPC_Write_Event_Request_Priv:                        \n"
        "     pop {r0}                                          \n"
        "     b IPC_Write_Event_Request_Priv                         \n"
        " Unprivileged_IPC_Write_Event_Request_Priv:                      \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_IPC_Write_Event_Request)
        : "memory");
#else
    return IPC_Write_Event_Request_Priv(request);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION nv::ipc::task::Status
sys::ipc::task::Driver::write_event_request_impl(const nv::ipc::task::Request& request)
{
    // Check if there is enough space for one streambuffer send
    // only one message buffer send, will write size_t first, then request
    auto status = wait_for_enough_space(
        sizeof(size_t) + sizeof(request),
        std::chrono::microseconds(sys::ipc::task::WaitForStreamBufferHasEnoughSpaceTime));

    // Write request to message buffer
    if (status == nv::ipc::task::Status::Ok) {
        auto request_span = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                                 sizeof(request));
        status            = nv::ipc::task::Driver::write(request_span);
    }
    // No enough space for one streambuffer send
    else {
        // TODO: Add log?
        return status;
    }
    return status;
}

bool sys::ipc::task::Driver::is_on_same_core(nv::ipc::TaskId task_id)
{
    return is_on_same_core(
        std::get<1>(nv::ipc::TaskInfos.at(nv::common::to_underlying(task_id))));
}

bool sys::ipc::task::Driver::is_on_same_core(nv::ipc::QueueId queue_id)
{
    return is_on_same_core(
        std::get<3>(nv::ipc::QueueInfos.at(nv::common::to_underlying(queue_id))));
}

bool sys::ipc::task::Driver::is_on_same_core(nv::ipc::EventId event_id)
{
    return is_on_same_core(
        std::get<1>(nv::ipc::EventInfos.at(nv::common::to_underlying(event_id))));
}

bool sys::ipc::task::Driver::is_on_same_core(nv::ipc::TimerId timer_id)
{
    return is_on_same_core(
        std::get<1>(nv::ipc::TimerInfos.at(nv::common::to_underlying(timer_id))));
}