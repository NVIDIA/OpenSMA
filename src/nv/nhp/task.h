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

#include <array>

#include "nv/ipc/task.h"
#include NV_IPC_CONFIG_H
#include "nv/nhp/nhp.h"
#include "sys/adc/adc.h"

namespace nv::nhp {

class Task;

// Configs
// ---------------------------------------------
struct TaskConfig
{
    // supervisor config
    nv::ipc::TaskId        task_id;
    const std::string_view task_name;

    std::array<NHP::NhpConfig, NumNhpInstances> nhp_configs;
};

class Task : public nv::ipc::Task
{
public:
    // Public Functions
    // ---------------------------------------------

    // creates task objects for supervisor
    static void make(const TaskConfig& task_config);
    // static void make(TaskConfig& config0, TaskConfig& config1);

    // initalizer
    Task(const TaskConfig& config) noexcept;

    // entrypoint from supervisor to start task
    static void entrypoint(void* params);

    // main loop just starts i2c driver and closes
    void start();

    // callback for gpio irq
    static void gpio_interrupt_callback(nv::gpio::GpioPort port,
                                        nv::gpio::GpioPin  pin,
                                        uint8_t            nhpInstance,
                                        uint8_t            driveIndex);

    // callback for adc irq
    static void adc_interrupt_callback(uint8_t                 nhpInstance,
                                       sys::adc::AdcPeripheral peripheral,
                                       uint8_t                 fifoIndex);

private:
    // Helper Functions
    // ---------------------------------------------
    void tryRunAdcTrigger();

    // Member Constants
    // ---------------------------------------------

    // Member Variables
    // ---------------------------------------------
    static Task*                     taskInstance;
    std::array<NHP, NumNhpInstances> nhp_instances;
    uint8_t                          current_trigger = 0;
};

}  // namespace nv::nhp
