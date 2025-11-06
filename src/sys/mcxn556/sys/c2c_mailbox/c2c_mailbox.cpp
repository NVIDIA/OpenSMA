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

#include "sys/c2c_mailbox/c2c_mailbox.h"
#include "nv/ipc/ipc_task.h"

namespace sys::c2c_mailbox {

void set_value(MailBoxValues value)
{
    auto cur_core = nv::ipc::get_current_core();
    if (cur_core == nv::ipc::CoreId::Core0) {
        MAILBOX_SetValue(MAILBOX, kMAILBOX_CM33_Core1, static_cast<uint32_t>(value));
    }
    else {
        // coverity[dead_error_line] - Expect cannot reach here in core0
        MAILBOX_SetValue(MAILBOX, kMAILBOX_CM33_Core0, static_cast<uint32_t>(value));
    }
}

uint32_t get_value()
{
    auto cur_core = nv::ipc::get_current_core();
    if (cur_core == nv::ipc::CoreId::Core0) {
        return MAILBOX_GetValue(MAILBOX, kMAILBOX_CM33_Core0);
    }
    else {
        // coverity[dead_error_line] - Expect cannot reach here in core0
        return MAILBOX_GetValue(MAILBOX, kMAILBOX_CM33_Core1);
    }
}

};  // namespace sys::c2c_mailbox
