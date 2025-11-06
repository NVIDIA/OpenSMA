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

#include "nv/nhp/task.h"
#include NV_IPC_CONFIG_H

#include <chrono>
#include <span>

#include "nv/gpio/common.h"
#include "nv/nv.h"
#include "sys/adc/adc.h"

namespace nv::nhp {

Task* Task::taskInstance = nullptr;

void Task::make(const TaskConfig& task_config)
{
    constexpr auto StackSize = std::max(2048, int(configMINIMAL_STACK_SIZE));

    NV_TASK_DATA static Task                       task(task_config);
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Norm, Task::entrypoint);

    Task::taskInstance = &task;
}

Task::Task(const TaskConfig& config) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, nhp_instances{}
{
    for (uint8_t i = 0; i < NumNhpInstances; i++) {
        // NOLINTNEXTLINE:
        new (&nhp_instances[i]) NHP(config.nhp_configs.at(i));
    }
    nv::info("Finished Task initialization\n");
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

void Task::start()
{
    nv::info("Starting NHP ADC Polling Loop\n");

    // coverity[no_escape] want this to run forever
    while (true) {
        using namespace std::chrono_literals;
        nv::ipc::Task::delay(2us);

        tryRunAdcTrigger();
    }
}

void Task::gpio_interrupt_callback(nv::gpio::GpioPort port,
                                   nv::gpio::GpioPin  pin,
                                   uint8_t            nhpInstance,
                                   uint8_t            driveIndex)
{
    Task::taskInstance->nhp_instances.at(nhpInstance).gpio_interrupt(port, pin, driveIndex);
}

void Task::adc_interrupt_callback(uint8_t                 nhpInstance,
                                  sys::adc::AdcPeripheral peripheral,
                                  uint8_t                 fifoIndex)
{
    int max_iterations = nv::nhp::MAX_ADC_POLL_ITERATIONS;
    while (sys::adc::ADC::get_fifo_count(0U, fifoIndex) != 0 && max_iterations-- > 0) {
        // NOLINTNEXTLINE: value gets modified in pop_fifo below
        uint16_t value = 0;
        // NOLINTNEXTLINE: command gets modified in pop_fifo below
        uint32_t command = 0;
        if (!sys::adc::ADC::pop_fifo(0U, fifoIndex, value, command)) {
            nv::error("Failed to read ADC%d FIFO %d\n", peripheral, fifoIndex);
            return;
        }

        // Check and adjust the command to 0-index
        if ((command < 1) || (command > 8)) {
            nv::error("Unexpected ADC command value(%d)\n", command);
            return;
        }
        command                  = command - 1;
        nhpInstance              = command / nv::nhp::NumE1sDrives;
        const uint8_t driveIndex = command % nv::nhp::NumE1sDrives;
        if (nhpInstance >= Task::taskInstance->nhp_instances.size()) {
            nv::error("Invalid NHP instance: %d\n", nhpInstance);
            return;
        }
        Task::taskInstance->nhp_instances.at(nhpInstance)
            .adc_interrupt(peripheral, driveIndex, value);
    }
}

// Could change this to interrupt event but doesn't improve performance or power
void Task::tryRunAdcTrigger()
{
    if (sys::adc::ADC::adc_ready(0U)) {
        sys::adc::ADC::trigger_read(0U, nv::nhp::AdcTriggers.at(current_trigger));
        if (++current_trigger == nv::nhp::NumAdcTriggers) {
            current_trigger = 0;
        }
    }
    else {
        nv::warn("ADC not finished. Skipping this polling tick.\n");
    }
}

}  // namespace nv::nhp