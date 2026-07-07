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
#pragma once
#include <chrono>
#include <cstdint>

#include <FreeRTOS.h>
#include <task.h>

#include "nv/common/expected.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

/**
 * FreeRTOS Task Notification wrapper - a lightweight alternative to Event.
 *
 * Task notifications are a lightweight alternative to event groups for
 * task-to-task or ISR-to-task signaling. Each FreeRTOS task has a built-in
 * 32-bit notification value that can be used to pass data or signal events.
 *
 * The non os-specific implementation can be found in nv/ipc/tasknotify.cpp,
 * while the os-specific implementation can be found in sys/{os}/sys/ipc/tasknotify.cpp
 */
class TaskNotify
{
public:
    enum class Status
    {
        Ok,              //< Success
        Timeout,         //< A timeout occurred.
        InvalidFromIsr,  //< Operation not allowed from ISR context.
        Unknown,         //< An unknown error has occurred.
    };

    using Bits   = uint32_t;
    using Return = common::Expected<Bits, Status>;
    using Usecs  = std::chrono::microseconds;

    /**
     * Set bits and send notification to the target task.
     *
     * @param[in] id    The target task notification.
     * @param[in] bits  The bits to set in the task's notification value.
     * @return          Status::Ok on success, error status otherwise.
     */
    static Status send(TaskId id, Bits bits);

    /**
     * Clear the given bits in the task's notification value.
     *
     * @param[in] id    The target task notification.
     * @param[in] bits  Bits to clear.
     * @return          Status::Ok on success.
     */
    static Status clear(TaskId id, Bits bits);

    /**
     * Read the current notification value without consuming the notification or clearing bits.
     * Can be called from any context (any task); does not block.
     *
     * @param[in] id  The target task notification.
     * @return        Current 32-bit notification value on success.
     */
    static Return get(TaskId id);

    /**
     * Reset: clear all bits and the notification state. Use e.g. on USB reset.
     *
     * @param[in] id  The target task notification.
     * @return        Status::Ok on success.
     */
    static Status reset(TaskId id);

    /**
     * Wait for a notification on the calling task.
     *
     * @param[in] timeout         Maximum time to wait.
     * @param[in] clear_on_return If true, clear all bits in the notification value before
     * returning. Default false (caller clears via clear()).
     * @return                    The full notification value when notified, or Status::Timeout.
     */
    static Return wait(Usecs timeout = Usecs::max(), bool clear_on_return = false);

private:
    /**
     * Look up the target task's FreeRTOS handle from the supervisor.
     *
     * @param[in] id  The target task.
     * @return        Reference to the registered task's FreeRTOS handle.
     */
    static TaskHandle_t& handle(TaskId id);
};

}  // namespace nv::ipc
