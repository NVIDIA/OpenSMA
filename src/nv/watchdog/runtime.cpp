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
#include "runtime.h"

#include "nv/flash/task.h"
#include "nv/ipc/event.h"
#include "nv/logger/task.h"
#include "nv/usb/task.h"
#include "nv/pldm/task.h"
#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/mctp/driver.h"
#include "boot.h"
using namespace nv::watchdog;
void Runtime::start_task_status_query([[maybe_unused]] ipc::Timer& id)
{
    auto& task_alive_event = ipc::Event::make(ipc::EventId::TaskAliveStatus);

    auto value    = task_alive_event.bits().value();
    bool feed_wdt = false;
    if (value == 0) {
        feed_wdt = true;
        Runtime::feed();
    }
    else {
        // Query the task that doesn't clear the event bit yet
        for (auto wdt_notify_id : nv::ipc::TaskMonitorList) {
            if (value & nv::common::bit(wdt_notify_id)) {
                query_task_status(wdt_notify_id);
            }
        }
    }

    if (feed_wdt) {
        for (auto wdt_notify_id : nv::ipc::TaskMonitorList) {
            query_task_status(wdt_notify_id);
        }
    }
}

void Runtime::mark_task_alive(TaskMonitorIndex index)
{
    auto&          event     = ipc::Event::make(ipc::EventId::TaskAliveStatus);
    const uint32_t alive_bit = nv::common::bit(index);
    event.clear(alive_bit);
}

void Runtime::query_task_status(TaskMonitorIndex index)
{
    auto&          event        = ipc::Event::make(ipc::EventId::TaskAliveStatus);
    const uint32_t query_bit    = nv::common::bit(index);
    auto           event_status = event.set(query_bit);

    if (event_status != ipc::Event::Status::Ok) {
        return;
    }

    switch (index) {
        case TaskMonitorIndex::Flash: {
            nv::flash::Task::wdt_notify();
        } break;
        case TaskMonitorIndex::Logger: {
            nv::logger::Task::wdt_notify();
        } break;
        case TaskMonitorIndex::Usb: {
            nv::usb::Task::wdt_notify();
        } break;
        case TaskMonitorIndex::Mctp: {
            mctp::Driver::wdt_notify();
        } break;
        case TaskMonitorIndex::Pldm: {
            pldm::Task::wdt_notify();
        } break;
        case TaskMonitorIndex::I2c0:
        case TaskMonitorIndex::I2c1:
        case TaskMonitorIndex::I2c2:
        case TaskMonitorIndex::I2c3:
        case TaskMonitorIndex::I2c4:
        case TaskMonitorIndex::I2c5:
        case TaskMonitorIndex::I2c6:
        case TaskMonitorIndex::I2c7:
        case TaskMonitorIndex::I2c8:
        case TaskMonitorIndex::I2c9: {
            i2c::Task::wdt_notify(index);
        } break;
        case TaskMonitorIndex::I3c0:
        case TaskMonitorIndex::I3c1: {
            i3c::Task::wdt_notify(index);
        } break;

        default: break;
    }
}
