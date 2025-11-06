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

using namespace nv::ipc;

Event::Event(EventId id) : sys::ipc::Event(), _id(id)
{
    nv::assert(handle() == nullptr);
    assert(common::is_in_range(_id));

    auto& super = Supervisor::inst();
    _handle     = xEventGroupCreateStatic(&super.static_events.at(common::to_underlying(_id)));

    nv::assert(handle() != nullptr);
}

Event::Return Event::bits() const
{
    const bool from_isr = sys::ipc::is_in_isr();
    return from_isr ? xEventGroupGetBitsFromISR(handle()) : xEventGroupGetBits(handle());
}

Event::Status Event::set(Bits bits, [[maybe_unused]] CoreId dest_core)
{
    const bool from_isr = sys::ipc::is_in_isr();
    if (from_isr) {
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

    return Status::Ok;
}

Event::Status Event::clear(Bits bits)
{
    const bool from_isr = sys::ipc::is_in_isr();
    if (from_isr) {
        xEventGroupClearBitsFromISR(handle(), bits);
    }
    else {
        xEventGroupClearBits(handle(), bits);
    }
    return Status::Ok;
}

Event::Return Event::wait(Bits bits, bool clear, bool wait_for_all, Usecs timeout)
{
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
