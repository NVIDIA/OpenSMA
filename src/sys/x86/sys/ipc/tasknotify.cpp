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
#include "nv/ipc/tasknotify.h"

#include <limits>

#include "portmacro.h"

#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"

using namespace nv::ipc;

TaskNotify::Status TaskNotify::send(TaskId id, Bits bits)
{
    auto& h = handle(id);

    if (sys::ipc::is_in_isr()) {
        BaseType_t higher_prio_task_woken = pdFALSE;
        xTaskNotifyFromISR(h, bits, eSetBits, &higher_prio_task_woken);
        portYIELD_FROM_ISR(higher_prio_task_woken);  // NOLINT
    }
    else {
        xTaskNotify(h, bits, eSetBits);
    }

    return Status::Ok;
}

TaskNotify::Status TaskNotify::clear(TaskId id, Bits bits)
{
    ulTaskNotifyValueClear(handle(id), bits);
    return Status::Ok;
}

TaskNotify::Return TaskNotify::get(TaskId id)
{
    return ulTaskNotifyValueClear(handle(id), 0);
}

TaskNotify::Status TaskNotify::reset(TaskId id)
{
    ulTaskNotifyValueClear(handle(id), std::numeric_limits<Bits>::max());
    return Status::Ok;
}

TaskNotify::Return TaskNotify::wait(Usecs timeout, bool clear_on_return)
{
    if (sys::ipc::is_in_isr()) {
        return Status::InvalidFromIsr;
    }

    const auto one_sec_in_usecs = 1'000'000U;
    // coverity[cert_int31_c_violation] - timeout is cast to unsigned long
    const TickType_t ticks_to_wait         = (timeout == Usecs::max())
                                               ? portMAX_DELAY
                                               : static_cast<TickType_t>(configTICK_RATE_HZ
                                                                 * timeout.count()
                                                                 / one_sec_in_usecs);
    const uint32_t   bits_to_clear_on_exit = clear_on_return ? std::numeric_limits<Bits>::max()
                                                             : 0U;
    uint32_t         notification_value    = 0;
    const BaseType_t result                = xTaskNotifyWait(
        0, bits_to_clear_on_exit, &notification_value, ticks_to_wait);
    if (result != pdTRUE) {
        return Status::Timeout;
    }
    return notification_value;
}
