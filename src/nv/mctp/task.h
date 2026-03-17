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

#include "nv/common/uuid.h"
#include "nv/i2c/powersensor/device_manager.h"
#include "nv/ipc/task.h"

namespace nv::mctp {

class Task : public ipc::Task
{
public:
    Task() noexcept : ipc::Task(nv::ipc::TaskId::Mctp, "MCTP") {}

    common::Uuid uuid;

    /// Power sensor manager (HSC/HSCC)
    nv::i2c::power::DeviceManager& power_sensor_mgr() { return power_sensor_mgr_; }

    // Port Recovery EI bitmap accessors
    void     set_port_recovery_ei_bitmap(uint32_t bitmap) { port_recovery_ei_bitmap_ = bitmap; }
    uint32_t get_port_recovery_ei_bitmap() { return port_recovery_ei_bitmap_; }

    // Error-injection helpers (moved from Driver)
    bool ep_stall_detected();
    bool bridge_hang_detected();
    bool pldm_t5_hang_detected();

    static void make();
    static void entrypoint(void* params);

private:
    nv::i2c::power::DeviceManager power_sensor_mgr_;
    volatile uint32_t             port_recovery_ei_bitmap_{};
};

nv::i2c::power::DeviceManager& get_power_sensor_mgr();
Task&                          get_task();

}  // namespace nv::mctp
