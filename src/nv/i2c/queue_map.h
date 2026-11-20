/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include <cstdint>

#include "nv/ipc/queue.h"
#include "nv/ipc/timer.h"
#include "nv/ipchandler/enums.h"
#include "nv/mctp/interface.h"
#include "nv/watchdog/notify_interface.h"

/// Maps the shared `I2cN` / `VrI2cN` naming to a project's `nv::ipc::QueueId`.
/// Slots a board does not declare resolve to `QueueId::End`. Templated on the
/// enum type so `if constexpr` skips undeclared slots without name-checking
/// their (missing) enumerators.
namespace nv::i2c {

/// Max `I2cN` slots in the shared namespace (from ipchandler::Id); boards may
/// declare fewer via QueueId.
constexpr int MaxI2cSlots = static_cast<int>(nv::ipchandler::Id::I2c9)
                          - static_cast<int>(nv::ipchandler::Id::I2c0) + 1;
/// Max `VrI2cN` slots in the shared namespace (from ipchandler::Id); boards may
/// declare fewer via QueueId.
constexpr int MaxVrI2cSlots = static_cast<int>(nv::ipchandler::Id::VrI2c1)
                            - static_cast<int>(nv::ipchandler::Id::VrI2c0) + 1;

template<typename QueueIdT = nv::ipc::QueueId>
constexpr QueueIdT i2c_queue(int slot)
{
#define NV_I2C_QUEUE_SLOT(N)                                                                   \
    if constexpr (requires { QueueIdT::I2c##N; }) {                                            \
        if (slot == (N)) {                                                                     \
            return QueueIdT::I2c##N;                                                           \
        }                                                                                      \
    }
    NV_I2C_QUEUE_SLOT(0)
    NV_I2C_QUEUE_SLOT(1)
    NV_I2C_QUEUE_SLOT(2)
    NV_I2C_QUEUE_SLOT(3)
    NV_I2C_QUEUE_SLOT(4)
    NV_I2C_QUEUE_SLOT(5)
    NV_I2C_QUEUE_SLOT(6)
    NV_I2C_QUEUE_SLOT(7)
    NV_I2C_QUEUE_SLOT(8)
    NV_I2C_QUEUE_SLOT(9)
#undef NV_I2C_QUEUE_SLOT
    return QueueIdT::End;
}

/// Shared I2C inbox matching an `I2cN` task, or `End` when either side is absent.
template<typename TaskIdT = nv::ipc::TaskId, typename QueueIdT = nv::ipc::QueueId>
constexpr QueueIdT task_to_i2c_queue(TaskIdT task_id)
{
#define NV_I2C_TASK_SLOT(N)                                                                    \
    if constexpr (requires { TaskIdT::I2c##N; }) {                                             \
        if (task_id == TaskIdT::I2c##N) {                                                      \
            return i2c_queue<QueueIdT>(N);                                                     \
        }                                                                                      \
    }
    NV_I2C_TASK_SLOT(0)
    NV_I2C_TASK_SLOT(1)
    NV_I2C_TASK_SLOT(2)
    NV_I2C_TASK_SLOT(3)
    NV_I2C_TASK_SLOT(4)
    NV_I2C_TASK_SLOT(5)
    NV_I2C_TASK_SLOT(6)
    NV_I2C_TASK_SLOT(7)
    NV_I2C_TASK_SLOT(8)
    NV_I2C_TASK_SLOT(9)
#undef NV_I2C_TASK_SLOT
    return QueueIdT::End;
}

template<typename QueueIdT = nv::ipc::QueueId>
constexpr QueueIdT vr_i2c_queue(int slot)
{
#define NV_VR_I2C_QUEUE_SLOT(N)                                                                \
    if constexpr (requires { QueueIdT::VrI2c##N; }) {                                          \
        if (slot == (N)) {                                                                     \
            return QueueIdT::VrI2c##N;                                                         \
        }                                                                                      \
    }
    NV_VR_I2C_QUEUE_SLOT(0)
    NV_VR_I2C_QUEUE_SLOT(1)
#undef NV_VR_I2C_QUEUE_SLOT
    return QueueIdT::End;
}

/// Map MCTP I2C client to the IPC queue for that downstream / US I2C task.
constexpr nv::ipc::QueueId client_to_i2c_queue_id(nv::mctp::Client client)
{
    switch (client) {
        case nv::mctp::Client::UsI2c : return i2c_queue(0);
        case nv::mctp::Client::DsI2c0: return i2c_queue(1);
        case nv::mctp::Client::DsI2c1: return i2c_queue(2);
        case nv::mctp::Client::DsI2c2: return i2c_queue(3);
        case nv::mctp::Client::DsI2c3: return i2c_queue(4);
        case nv::mctp::Client::DsI2c4: return i2c_queue(5);
        case nv::mctp::Client::DsI2c5: return i2c_queue(6);
        case nv::mctp::Client::DsI2c6: return i2c_queue(7);
        case nv::mctp::Client::DsI2c7: return i2c_queue(8);
        default                      : return nv::ipc::QueueId::End;
    }
}

/// Map a watchdog task-monitor slot to the I2C / VrI2C queue it monitors.
constexpr nv::ipc::QueueId wdt_to_i2c_queue(nv::watchdog::TaskMonitorIndex index)
{
    using Wdt = nv::watchdog::TaskMonitorIndex;
    if (index >= Wdt::I2c0 && index <= Wdt::I2c9) {
        return i2c_queue(static_cast<int>(index) - static_cast<int>(Wdt::I2c0));
    }
    if (index >= Wdt::VrI2c0 && index <= Wdt::VrI2c1) {
        return vr_i2c_queue(static_cast<int>(index) - static_cast<int>(Wdt::VrI2c0));
    }
    return nv::ipc::QueueId::End;
}

/// Inverse of `wdt_to_i2c_queue`: which monitor slot an I2C / VrI2C queue feeds.
constexpr nv::watchdog::TaskMonitorIndex i2c_queue_to_wdt(nv::ipc::QueueId queue_id)
{
    using Wdt = nv::watchdog::TaskMonitorIndex;
    if (queue_id == nv::ipc::QueueId::End) {
        return Wdt::End;
    }
    for (int slot = 0; slot < MaxI2cSlots; ++slot) {
        if (i2c_queue(slot) == queue_id) {
            return static_cast<Wdt>(static_cast<int>(Wdt::I2c0) + slot);
        }
    }
    for (int slot = 0; slot < MaxVrI2cSlots; ++slot) {
        if (vr_i2c_queue(slot) == queue_id) {
            return static_cast<Wdt>(static_cast<int>(Wdt::VrI2c0) + slot);
        }
    }
    return Wdt::End;
}

/// Map a logical I2C ipchandler destination to the owning task's queue.
constexpr nv::ipc::QueueId ipchandler_to_i2c_queue(nv::ipchandler::Id id)
{
    using Id = nv::ipchandler::Id;
    if (id >= Id::I2c0 && id <= Id::I2c9) {
        return i2c_queue(static_cast<int>(id) - static_cast<int>(Id::I2c0));
    }
    if (id >= Id::VrI2c0 && id <= Id::VrI2c1) {
        return vr_i2c_queue(static_cast<int>(id) - static_cast<int>(Id::VrI2c0));
    }
    return nv::ipc::QueueId::End;
}

/// Map an AP-status polling timer to the I2C task queue that owns that AP.
constexpr nv::ipc::QueueId ap_status_timer_to_i2c_queue(nv::ipc::TimerId timer_id)
{
    switch (timer_id) {
        case nv::ipc::TimerId::Ap1Status: return i2c_queue(1);
        case nv::ipc::TimerId::Ap2Status: return i2c_queue(2);
        case nv::ipc::TimerId::Ap3Status: return i2c_queue(3);
        default                         : return nv::ipc::QueueId::End;
    }
}

}  // namespace nv::i2c
