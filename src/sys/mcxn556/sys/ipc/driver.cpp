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
#include "mcmgr_wrapper.h"
#include "nv/ipc/ipc_task.h"
#include "nv/ipc/supervisor.h"
#include "mpu_syscall_numbers.h"
#include "nv/common/utils.h"
#include <cstring>
#include "sys/common/c2c_fault.h"
using namespace nv::ipc::task;

#if defined(__cplusplus)
extern "C" {
#endif
NV_SHARED_DATA bool peer_core_interrupt_ready = false;
// Static member variable for Driver class
// coverity[declared_but_not_referenced] - access from core 1
static uint32_t shared_memory_base_address = 0U;

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

nv::ipc::task::Status Driver::init(uint32_t shared_base_c2c_memory_address,
                                   uint32_t core1_image_address)
{
    auto                  core   = nv::ipc::get_current_core();
    nv::ipc::task::Status status = nv::ipc::task::Status::Ok;
    if (core == nv::ipc::CoreId::Core0) {
        // Set interrupt callback on core0 for C2C communication
        status = sys::ipc::task::Mcmgr::register_event(nv::ipc::task::EventType::Communication);
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        // Set interrupt callback on core0 for Core1Ready event
        status = sys::ipc::task::Mcmgr::register_event(nv::ipc::task::EventType::Core1Ready);
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        // Start core1
        status = sys::ipc::task::Mcmgr::start_core(nv::ipc::CoreId::Core1,
                                                   (void*)(char*)(core1_image_address),
                                                   shared_base_c2c_memory_address,
                                                   kMCMGR_Start_Asynchronous);

        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        status = sys::ipc::task::Mcmgr::register_event(nv::ipc::task::EventType::Core1Fault);
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        return nv::ipc::task::Status::Ok;
    }
    // coverity[dead_error_line] - Expect cannot reach here in core0
    else if (core == nv::ipc::CoreId::Core1) {
        uint32_t startup_data     = 0;
        auto     start_time_stamp = nv::ctimer::Driver::read_ticks();

        // Get startup data from core0: core0 will send shared_base_c2c_memory_address
        // 1 sec timeout
        while (sys::ctimer::Driver::get_counter_difference(start_time_stamp,
                                                           nv::ctimer::Driver::read_ticks())
               < sys::ipc::task::Core1StartupTimeout) {
            status = sys::ipc::task::Mcmgr::get_startup_data(nv::ipc::CoreId::Core0,
                                                             &startup_data);
            if (status == nv::ipc::task::Status::Ok) {
                break;
            }
        }

        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        // Set peer core interrupt ready
        // Core0's interrupt is already ready before booting core1
        set_peer_core_interrupt_ready(true);

        // Set interrupt callback on core1 for C2C communication
        status = sys::ipc::task::Mcmgr::register_event(nv::ipc::task::EventType::Communication);
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        status = sys::ipc::task::Mcmgr::register_event(nv::ipc::task::EventType::Core0Fault);
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        // Notify core0 that core1 is ready to receive interrupt
        status = sys::ipc::task::Mcmgr::trigger_event_force(
            nv::ipc::get_peer_core(),
            nv::ipc::task::EventType::Core1Ready,
            static_cast<uint16_t>(nv::ipc::task::CmdCode::Core1Ready));
        if (status != nv::ipc::task::Status::Ok) {
            return status;
        }

        // Store c2c handle address based on startup_data
        init_c2c_communication(startup_data, true);

        nv::ipc::task::Driver::set_shared_memory_base_address(startup_data);

        return nv::ipc::task::Status::Ok;
    }
    else {
        return nv::ipc::task::Status::InvalidParameter;
    }
}

nv::ipc::task::Status Driver::write(const std::span<const uint8_t> data)
{
    auto handle = get_c2c_handle(true);
    if (handle == nullptr) {
        return nv::ipc::task::Status::C2CIdInvalid;
    }

    auto res = nv::ipc::StreamBuffer::send(handle, data);

    // Check if the data is sent successfully
    if (res != nv::common::align_to(data.size(), sys::ipc::StreamBufferSendAlignment)) {
        return nv::ipc::task::Status::C2CSendFailed;
    }

    return nv::ipc::task::Status::Ok;
}

nv::ipc::task::Status
Driver::read(std::span<uint8_t> data, uint32_t& read_size, nv::ipc::StreamBuffer::Usecs timeout)
{
    auto handle = get_c2c_handle(false);
    if (handle == nullptr) {
        return nv::ipc::task::Status::C2CIdInvalid;
    }

    read_size = nv::ipc::StreamBuffer::recv(handle, data, timeout);

    // Check if the data is received successfully
    if (read_size == 0) {
        return nv::ipc::task::Status::C2CRecvFailed;
    }

    return nv::ipc::task::Status::Ok;
}

size_t Driver::bytes_available_to_read(bool is_tx)
{
    auto handle = get_c2c_handle(is_tx);
    if (handle == nullptr) {
        return 0;
    }

    auto res = nv::ipc::StreamBuffer::bytes_available_to_read(handle);

    return res;
}

size_t Driver::bytes_available_to_write(bool is_tx)
{
    auto handle = get_c2c_handle(is_tx);
    if (handle == nullptr) {
        return 0;
    }

    auto res = nv::ipc::StreamBuffer::bytes_available_to_write(handle);

    return res;
}

nv::ipc::StreamBuffer::IdType Driver::get_c2c_id(bool is_tx)
{
    const nv::ipc::CoreId current_core = nv::ipc::get_current_core();
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

StreamBufferHandle_t Driver::get_c2c_handle(bool is_tx)
{
    return nv::ipc::StreamBuffer::get_stream_buffer_handle(Driver::get_c2c_id(is_tx));
}

bool Driver::get_peer_core_interrupt_ready()
{
    return peer_core_interrupt_ready;
}

void Driver::set_peer_core_interrupt_ready(bool ready)
{
    peer_core_interrupt_ready = ready;
}

void Driver::init_c2c_communication(uint32_t base_addr, bool direct_memory_access)
{
    if constexpr (EnableDualCore) {
        for (int i = 0; i < static_cast<int>(ipc::StreamBufferId::C2CEnd); i++) {
            auto& stream_buffer = ipc::StreamBuffer::make(
                static_cast<ipc::StreamBufferId>(i), false, base_addr, direct_memory_access);
            ipc::StreamBuffer::set_stream_buffer_handle(
                static_cast<ipc::StreamBufferId>(i),
                stream_buffer.get_stream_buffer_handle_by_core());
        }
    }
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
            > sys::ipc::task::WaitForC2CHasEnoughSpaceTimeOut) {
            return nv::ipc::task::Status::C2CHasNoEnoughSpace;
        }
        available_space = nv::ipc::task::Driver::bytes_available_to_write(true);
    }
    return nv::ipc::task::Status::C2CHasNoEnoughSpace;
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
    if (nv::common::align_to(item.size(), sys::ipc::StreamBufferSendAlignment)
        > UINT32_MAX - (sizeof(size_t) + sizeof(request) + sizeof(size_t))) {
        return nv::ipc::task::Status::C2CSendSizeOverflow;
    }
    // Check if there is enough space for two c2c send
    // 1st message buffer send will write size_t first, then request
    // 2nd message buffer send will write size_t first, then item
    auto status = wait_for_enough_space(
        sizeof(size_t) + sizeof(request) + sizeof(size_t)
            + nv::common::align_to(item.size(), sys::ipc::StreamBufferSendAlignment),
        std::chrono::microseconds(sys::ipc::task::WaitForC2CHasEnoughSpaceTimeOut));

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
    // No enough space for two c2c send
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
    // Check if there is enough space for one c2c send
    // only one message buffer send, will write size_t first, then request
    auto status = wait_for_enough_space(
        sizeof(size_t) + sizeof(request),
        std::chrono::microseconds(sys::ipc::task::WaitForC2CHasEnoughSpaceTimeOut));

    // Write request to message buffer
    if (status == nv::ipc::task::Status::Ok) {
        auto request_span = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                                 sizeof(request));
        status            = nv::ipc::task::Driver::write(request_span);
    }
    // No enough space for one c2c send
    else {
        // TODO: Add log?
        return status;
    }
    return status;
}

bool sys::ipc::task::Driver::can_direct_access_on_current_core(nv::ipc::TaskId task_id)
{
    return can_direct_access_on_current_core(
        std::get<1>(nv::ipc::TaskInfos.at(nv::common::to_underlying(task_id))));
}

bool sys::ipc::task::Driver::can_direct_access_on_current_core(nv::ipc::QueueId queue_id)
{
    return can_direct_access_on_current_core(
        std::get<3>(nv::ipc::QueueInfos.at(nv::common::to_underlying(queue_id))));
}

bool sys::ipc::task::Driver::can_direct_access_on_current_core(nv::ipc::EventId event_id)
{
    return can_direct_access_on_current_core(
        std::get<1>(nv::ipc::EventInfos.at(nv::common::to_underlying(event_id))));
}

bool sys::ipc::task::Driver::can_direct_access_on_current_core(nv::ipc::TimerId timer_id)
{
    return can_direct_access_on_current_core(
        std::get<1>(nv::ipc::TimerInfos.at(nv::common::to_underlying(timer_id))));
}

bool sys::ipc::task::Driver::can_direct_access_on_current_core(nv::mctp::Client client)
{
    return can_direct_access_on_current_core(
        std::get<1>(nv::ipc::ClientInfos.at(nv::common::to_underlying(client))));
}

bool sys::ipc::task::Driver::can_cross_core_access(nv::ipc::QueueId queue_id)
{
    return can_cross_core_access(
        std::get<3>(nv::ipc::QueueInfos.at(nv::common::to_underlying(queue_id))));
}

bool sys::ipc::task::Driver::can_cross_core_access(nv::ipc::EventId event_id)
{
    return can_cross_core_access(
        std::get<1>(nv::ipc::EventInfos.at(nv::common::to_underlying(event_id))));
}

bool sys::ipc::task::Driver::can_cross_core_access(nv::ipc::TaskId task_id)
{
    return can_cross_core_access(nv::ipc::get_core_from_task(task_id));
}

nv::ipc::CoreId sys::ipc::task::Driver::get_core_from_client(nv::mctp::Client client)
{
    return get_core_from_task(
        std::get<1>(nv::ipc::ClientInfos.at(nv::common::to_underlying(client))));
}

void nv::ipc::task::Driver::set_shared_memory_base_address(uint32_t address)
{
    constexpr auto cur_core = nv::ipc::get_current_core();
    if (cur_core == nv::ipc::CoreId::Core0) {
        return;
    }
    else {
        shared_memory_base_address = address;
    }
}

uint32_t nv::ipc::task::Driver::get_shared_memory_base_address()
{
    constexpr auto cur_core = nv::ipc::get_current_core();
    if (cur_core == nv::ipc::CoreId::Core0) {
        // Use the overload that returns StreamBuffer* with direct_memory_access = false
        auto* stream_buffer = nv::ipc::Supervisor::inst().memory_for(
            nv::ipc::StreamBufferId::Core0ToCore1, 0, false);
        return std::bit_cast<uint32_t>(stream_buffer);
    }
    else {
        return shared_memory_base_address;
    }
}

uint8_t* nv::ipc::task::Driver::get_core1_fault_buffer()
{
    return std::bit_cast<uint8_t*>(get_shared_memory_base_address());
}