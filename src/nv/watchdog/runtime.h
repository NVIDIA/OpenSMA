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
#include "sys/watchdog/wwdt_driver.h"
#include "nv/ipc/timer.h"
#include "nv/watchdog/notify_interface.h"
#include "nv/bootloader.h"
#include NV_IPC_CONFIG_H

namespace nv::watchdog {
class Runtime : public sys::watchdog::WwdtDriver
{
public:
    static void               init(uint32_t reset_ms, bool enable_reset);
    static void               feed();
    static void               start_task_status_query([[maybe_unused]] ipc::Timer& id);
    static void               mark_task_alive(TaskMonitorIndex index);
    static void               query_task_status(TaskMonitorIndex index);
    static void               record_reset(nv::bootloader::Driver::ImageIndex index);
    static void               clear_reset(nv::bootloader::Driver::ImageIndex index);
    static bool               is_reset(nv::bootloader::Driver::ImageIndex index);
    static constexpr uint32_t RetryThreshold = 2;
};

}  // namespace nv::watchdog