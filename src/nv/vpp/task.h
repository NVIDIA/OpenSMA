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

#include <string_view>

#include NV_IPC_CONFIG_H

#include "nv/ipc/event.h"
#include "nv/ipc/task.h"
#include "nv/vpp/vpp.h"
#include "sys/adc/adc.h"

namespace nv::vpp {

class Task;

// Configs
// ---------------------------------------------
struct TaskConfig
{
    // supervisor config
    nv::ipc::TaskId        task_id;
    const std::string_view task_name;

    std::array<VPP::VppConfig, nhp::NumNhpInstances> vpp_configs;

    nv::ipc::Event::IdType hotSwapEventId;
};

class Task : public nv::ipc::Task
{
public:
    // Public Functions
    // ---------------------------------------------

    // creates task objects for supervisor
    static void make(const TaskConfig& task_config);

    // initalizer
    Task(const TaskConfig& config) noexcept;

    // entrypoint from supervisor to start task
    static void entrypoint(void* params);

    // main loop just starts i2c driver and closes
    void start();

    // callback for gpio irq
    static void gpio_interrupt_callback(nv::gpio::GpioPort port,
                                        nv::gpio::GpioPin  pin,
                                        uint8_t            vppInstance,
                                        uint8_t            driveIndex);

    // callback for adc irq
    static void adc_interrupt_callback(uint8_t                 vppInstance,
                                       sys::adc::AdcPeripheral peripheral,
                                       uint8_t                 fifoIndex);

private:
    // Helper Functions
    // ---------------------------------------------
    void checkForHotSwapTimers();

    void tryRunAdcTrigger();

    // Member Constants
    // ---------------------------------------------

    // Member Variables
    // ---------------------------------------------
    static Task*                          taskInstance;
    std::array<VPP, nhp::NumNhpInstances> vpp_instances;
    uint8_t                               current_trigger = 0;

    nv::ipc::Event& hotSwapTimerEvent;
};

}  // namespace nv::vpp