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
#include "mcmgr_wrapper.h"
#include "nv/ipc/streambuffer.h"
#include "nv/ipc/driver.h"
#include "sys/common/c2c_fault.h"

using namespace sys::ipc::task;

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
        case nv::ipc::task::EventType::Core1Ready   : return kMCMGR_RemoteRPMsgEvent;
        case nv::ipc::task::EventType::Core1Fault   : return kMCMGR_RemoteExceptionEvent;
        case nv::ipc::task::EventType::Core0Fault   : return kMCMGR_RemoteExceptionEvent;
        default                                     : return kMCMGR_RemoteApplicationEvent;
    }
}
}  // namespace

#if defined(__cplusplus)
extern "C" {
#endif
uint16_t C2CEventData        = 0U;
uint16_t Core1ReadyEventData = 0U;
uint16_t Core1FaultEventData = 0U;
uint16_t Core0FaultEventData = 0U;

namespace {
void C2CEventHandlerImpl(uint16_t event_data, void* context)
{
    switch (event_data) {
        case (static_cast<uint16_t>(nv::ipc::task::CmdCode::InterCoreSendWriteDone)): {
            auto handle = nv::ipc::task::Driver::get_c2c_handle(false);
            if (handle == nullptr) {
                break;
            }
            // Callback function to notify write done
            nv::ipc::StreamBuffer::send_completed_isr(handle);
            break;
        }
        default: {
            break;
        }
    }
}

void Core1ReadyEventHandlerImpl(uint16_t event_data, void* context)
{
    switch (event_data) {
        case (static_cast<uint16_t>(nv::ipc::task::CmdCode::Core1Ready)): {
            // Core1 is ready to receive interrupt
            nv::ipc::task::Driver::set_peer_core_interrupt_ready(true);
            auto handle = nv::ipc::task::Driver::get_c2c_handle(false);
            if (handle == nullptr) {
                break;
            }
            // There may be data in c2c for core0 to read, wake up core0 to read it
            nv::ipc::StreamBuffer::send_completed_isr(handle);
            break;
        }
        default: {
            break;
        }
    }
}

void AnotherCoreFaultEventHandlerImpl(uint16_t event_data, void* context)
{
    c2c_fault::trigger_self_fault();
}
}  // namespace

// Platform-specific callback functions
#ifdef CPU_MCXN547VDF
void C2CEventHandler(uint16_t event_data, void* context)
{
    C2CEventHandlerImpl(event_data, context);
}

void Core1ReadyEventHandler(uint16_t event_data, void* context)
{
    Core1ReadyEventHandlerImpl(event_data, context);
}

void AnotherCoreFaultEventHandler(uint16_t event_data, void* context)
{
    AnotherCoreFaultEventHandlerImpl(event_data, context);
}
#elif defined(CPU_MCXN556SCDF)
void C2CEventHandler(mcmgr_core_t coreNum, uint16_t event_data, void* context)
{
    // Ignore coreNum parameter for MCXN556 as it's not used in the implementation
    (void)coreNum;
    C2CEventHandlerImpl(event_data, context);
}

void Core1ReadyEventHandler(mcmgr_core_t coreNum, uint16_t event_data, void* context)
{
    // Ignore coreNum parameter for MCXN556 as it's not used in the implementation
    (void)coreNum;
    Core1ReadyEventHandlerImpl(event_data, context);
}

void AnotherCoreFaultEventHandler(mcmgr_core_t coreNum, uint16_t event_data, void* context)
{
    // Ignore coreNum parameter for MCXN556 as it's not used in the implementation
    (void)coreNum;
    AnotherCoreFaultEventHandlerImpl(event_data, context);
}
#endif

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

nv::ipc::task::Status Mcmgr::start_core(nv::ipc::CoreId    core_id,
                                        void*              boot_address,
                                        uint32_t           startup_data,
                                        mcmgr_start_mode_t mode)
{
    auto mcmgr_core = (core_id == nv::ipc::CoreId::Core0) ? kMCMGR_Core0 : kMCMGR_Core1;
    return kstatus_to_status(MCMGR_StartCore(mcmgr_core, boot_address, startup_data, mode));
}

nv::ipc::task::Status Mcmgr::trigger_event_force(nv::ipc::CoreId          core_id,
                                                 nv::ipc::task::EventType event_type,
                                                 uint16_t                 event_data)
{
    // Only notify when peer core is ready to receive interrupt
    if (nv::ipc::task::Driver::get_peer_core_interrupt_ready() == false) {
        return nv::ipc::task::Status::Ok;
    }
#ifdef CPU_MCXN547VDF
    return kstatus_to_status(
        MCMGR_TriggerEventForce(event_type_to_kMCMGR_event(event_type), event_data));
#elif defined(CPU_MCXN556SCDF)
    auto mcmgr_core = (core_id == nv::ipc::CoreId::Core0) ? kMCMGR_Core0 : kMCMGR_Core1;
    return kstatus_to_status(MCMGR_TriggerEventForce(
        mcmgr_core, event_type_to_kMCMGR_event(event_type), event_data));
#endif
}

nv::ipc::task::Status Mcmgr::register_event(nv::ipc::task::EventType event_type)
{
    mcmgr_status_t status = kStatus_MCMGR_Success;
    if (event_type == nv::ipc::task::EventType::Communication) {
        status = MCMGR_RegisterEvent(
            event_type_to_kMCMGR_event(event_type), C2CEventHandler, (void*)&C2CEventData);
    }
    else if (event_type == nv::ipc::task::EventType::Core1Ready) {
        status = MCMGR_RegisterEvent(event_type_to_kMCMGR_event(event_type),
                                     Core1ReadyEventHandler,
                                     (void*)&Core1ReadyEventData);
    }
    else if (event_type == nv::ipc::task::EventType::Core1Fault) {
        status = MCMGR_RegisterEvent(event_type_to_kMCMGR_event(event_type),
                                     AnotherCoreFaultEventHandler,
                                     (void*)&Core1FaultEventData);
    }
    else if (event_type == nv::ipc::task::EventType::Core0Fault) {
        status = MCMGR_RegisterEvent(event_type_to_kMCMGR_event(event_type),
                                     AnotherCoreFaultEventHandler,
                                     (void*)&Core0FaultEventData);
    }
    else {
        return nv::ipc::task::Status::InvalidParameter;
    }
    return kstatus_to_status(status);
}

nv::ipc::task::Status Mcmgr::get_startup_data(nv::ipc::CoreId core_id, uint32_t* startup_data)
{
#ifdef CPU_MCXN547VDF
    return kstatus_to_status(MCMGR_GetStartupData(startup_data));
#elif defined(CPU_MCXN556SCDF)
    auto mcmgr_core = (core_id == nv::ipc::CoreId::Core0) ? kMCMGR_Core0 : kMCMGR_Core1;
    return kstatus_to_status(MCMGR_GetStartupData(mcmgr_core, startup_data));
#endif
}