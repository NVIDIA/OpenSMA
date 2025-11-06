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
#include "nv/ipc/event.h"

#include "portmacro.h"

#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/ipc/ipc_task.h"

using namespace nv::ipc;

Event::Event(EventId id) : sys::ipc::Event(), _id(id)
{
    nv::assert(handle() == nullptr);
    assert(common::is_in_range(_id));

    // Only create event if it can be accessed directly on the current core
    if (sys::ipc::task::Driver::can_direct_access_on_current_core(_id)) {
        _can_direct_access = true;
        auto& super        = Supervisor::inst();
        _handle = xEventGroupCreateStatic(&super.static_events.at(common::to_underlying(_id)));

        nv::assert(handle() != nullptr);
    }
    // Set _can_cross_core_access if it can be accessed cross core
    _can_cross_core_access = sys::ipc::task::Driver::can_cross_core_access(_id);
}

Event::Return Event::bits() const
{
    if (_can_direct_access) {
        return xPortIsInsideInterrupt() ? xEventGroupGetBitsFromISR(handle())
                                        : xEventGroupGetBits(handle());
    }
    else {
        // Invalid operation, return 0
        return 0;
    }
}

Event::Status Event::set(Bits bits, CoreId dest_core)
{
    if ((dest_core == CoreId::Invalid && _can_direct_access)
        || (dest_core == nv::ipc::get_current_core())) {
        if (xPortIsInsideInterrupt()) {
            BaseType_t higher_prio_task_woken = false;
            if (xEventGroupSetBitsFromISR(handle(), bits, &higher_prio_task_woken) != pdFAIL) {
                portYIELD_FROM_ISR(higher_prio_task_woken);  // NOLINT
            }
            else {
                return Status::Unknown;
            }
        }
        else {
            xEventGroupSetBits(handle(), bits);
        }
    }
    else if (_can_cross_core_access) {
        auto status = nv::ipc::task::Task::handle_event_data(_id, true, bits);
        if (status != nv::ipc::task::Status::Ok) {
            return Status::CommunicationFailed;
        }
    }
    else {
        return Status::InvalidOperation;
    }

    return Status::Ok;
}

Event::Status Event::clear(Bits bits)
{
    if (_can_direct_access) {
        if (xPortIsInsideInterrupt()) {
            xEventGroupClearBitsFromISR(handle(), bits);
        }
        else {
            xEventGroupClearBits(handle(), bits);
        }
    }
    else if (_can_cross_core_access) {
        auto status = nv::ipc::task::Task::handle_event_data(_id, false, bits);
        if (status != nv::ipc::task::Status::Ok) {
            return Status::CommunicationFailed;
        }
    }
    else {
        return Status::InvalidOperation;
    }

    return Status::Ok;
}

Event::Return Event::wait(Bits bits, bool clear, bool wait_for_all, Usecs timeout)
{
    if (_can_direct_access) {
        constexpr auto OneSecInUsecs = 1'000'000;
        // coverity[cert_int31_c_violation] - timeout is cast to unsigned long
        return xEventGroupWaitBits(handle(),
                                   bits,
                                   clear,
                                   // coverity[cert_int32_c_violation] - valid cast
                                   wait_for_all,
                                   // coverity[cert_int32_c_violation] - valid cast
                                   configTICK_RATE_HZ * timeout.count() / OneSecInUsecs);
    }
    else {
        return Status::InvalidOperation;
    }
}