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

#include "nv/ahs/task.h"
#include "nv/ctimer/ctimer.h"
#include "nv/gpio/common.h"
#include "sys/adc/adc.h"
#include "nv/nv.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include NV_IPC_CONFIG_H

extern void projectTryRunAdcTrigger();

namespace nv::ahs {

/** @brief Static pointer to the single task instance for callback access */
Task* Task::taskInstance = nullptr;

/**
 * @brief Creates task objects for supervisor management
 *
 * Static factory method that creates and configures the AHS task instance
 * with the specified configuration. Sets up the task stack, priority, and
 * entry point. This method is called by the supervisor to initialize the
 * AHS task before starting it.
 *
 * @param task_config Configuration structure for the task
 */
void Task::make(const TaskConfig& task_config)
{
    constexpr auto StackSize = std::max(2048, int(configMINIMAL_STACK_SIZE));

    // Create the task instance and stack
    NV_TASK_DATA static Task                       task(task_config);
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // Set up the task with stack and entry point
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Norm, Task::entrypoint);

    // Store the task instance pointer for static callback access
    Task::taskInstance = &task;
}

/**
 * @brief Constructor for AHS task instance
 *
 * Initializes the task with the provided configuration, creates AHS instances
 * for all NHP instances, and sets up the hot swap timer event. Each AHS
 * instance is constructed with its corresponding configuration and the hot
 * swap timer event for coordination.
 *
 * @param config Configuration structure for the task
 */
Task::Task(const TaskConfig& config) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, ahs_instances{}
, hotSwapTimerEvent(nv::ipc::Event::make(config.hotSwapEventId))
, delay_time(config.delay_time)
{
    // Create AHS instances for all NHP instances
    for (uint8_t i = 0; i < nv::nhp::NumNhpInstances; i++) {
        ahs_instances.at(i).emplace(config.ahs_configs.at(i), &hotSwapTimerEvent);
    }
    nv::info("Finished Task initialization\n");
}

/**
 * @brief Entry point function called by supervisor to start the task
 *
 * Static function that serves as the entry point for the task execution.
 * Casts the void* parameter to a Task pointer and calls the instance's
 * start method. After the start method completes, the task suspends itself.
 *
 * @param params Pointer to the task instance (cast from void*)
 */
void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.start();
    task.suspend();
}

/**
 * @brief Main task execution loop
 *
 * Starts the I2C driver and runs the main ADC polling loop. The loop
 * continuously monitors for ADC triggers and checks hot swap timers.
 * This method runs indefinitely until the task is suspended, providing
 * continuous monitoring of all AHS instances.
 *
 * The loop includes a delay to match the ADC hardware polling interval
 * and calls helper methods to manage ADC triggering and hot swap timer
 * checking.
 */
void Task::start()
{
    nv::info("Starting AHS ADC Polling Loop\n");

    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Nhp);

    // coverity[no_escape] should never leave here
    while (true) {
        // Delay for the configured delay time
        if (delay_time != std::chrono::microseconds::zero()) {
            nv::ipc::Task::delay(delay_time);
        }

        // Attempt to trigger ADC conversions for voltage monitoring
        projectTryRunAdcTrigger();

        // Check for expired hot swap timers across all AHS instances
        checkForHotSwapTimers();
    }
}

/**
 * @brief Checks if an AHS instance/drive is valid
 *
 * Checks if the AHS instance at the given index is initialized.
 *
 * @param ahsInstance AHS instance number to check
 * @param driveIndex Drive index within the AHS instance
 *
 * @return true if the AHS instance/drive is valid, false otherwise
 */
bool Task::isAhsInstanceDriveValid(uint8_t ahsInstance, uint8_t driveIndex)
{
    if (nullptr == Task::taskInstance) {
        return false;
    }

    if (ahsInstance >= Task::taskInstance->ahs_instances.size()) {
        nv::error("Invalid AHS instance: %d\n", ahsInstance);
        nv::logger::error(
            logger::Event::AhsInvalidInstance,
            {static_cast<uint8_t>(ahsInstance), static_cast<uint8_t>(driveIndex)});
        return false;
    }

    // Check if the optional has a value before dereferencing
    if (!Task::taskInstance->ahs_instances.at(ahsInstance).has_value()) {
        nv::info("AHS instance %d not initialized\n", ahsInstance);
        return false;
    }

    // Validate drive index
    if (driveIndex >= nv::nhp::NumE1sDrives) {
        nv::error("Invalid drive index: %d\n", driveIndex);
        nv::logger::error(
            logger::Event::AhsInvalidDriveIndex,
            {static_cast<uint8_t>(ahsInstance), static_cast<uint8_t>(driveIndex)});
        return false;
    }

    return true;
}

/**
 * @brief Static callback for GPIO interrupt handling
 *
 * Routes GPIO interrupts to the appropriate AHS instance based on
 * the AHS instance number and drive index. This static method allows
 * the GPIO interrupt handler to access the task instance and forward
 * interrupts to the correct AHS instance for processing.
 *
 * @param port GPIO port that generated the interrupt
 * @param pin GPIO pin that generated the interrupt
 * @param ahsInstance AHS instance number to route the interrupt to
 * @param driveIndex Drive index within the AHS instance
 */
void Task::gpio_interrupt_callback(nv::gpio::GpioPort port,
                                   nv::gpio::GpioPin  pin,
                                   uint8_t            ahsInstance,
                                   uint8_t            driveIndex)
{
    if (!isAhsInstanceDriveValid(ahsInstance, driveIndex)) {
        return;
    }

    // Route the interrupt to the appropriate AHS instance
    Task::taskInstance->ahs_instances.at(ahsInstance)->gpio_interrupt(port, pin, driveIndex);
}

/**
 * @brief Static callback for ADC interrupt handling
 *
 * Routes ADC interrupts to the appropriate AHS instance and processes
 * ADC FIFO data for voltage monitoring. Reads all available data from
 * the ADC FIFO and determines which AHS instance and drive the data
 * corresponds to based on the ADC command value.
 *
 * @param value ADC value that generated the interrupt
 * @param ahsInstance AHS instance number to route the interrupt to
 * @param driveIndex Drive index within the AHS instance
 */
void Task::adc_interrupt_callback(uint16_t value, uint8_t ahsInstance, uint8_t driveIndex)
{
    if (!isAhsInstanceDriveValid(ahsInstance, driveIndex)) {
        return;
    }

    // Route the ADC data to the appropriate AHS instance
    Task::taskInstance->ahs_instances.at(ahsInstance)->adc_interrupt(0U, driveIndex, value);
}

/**
 * @brief Checks for expired hot swap timers across all AHS instances
 *
 * Iterates through all AHS instances to check if any hot swap timers
 * have expired and need to trigger state machine updates. When timers
 * expire, it clears the event bits and triggers the corresponding
 * hot swap timer interrupts after the appropriate delay.
 *
 * Note: Could use ctimer interrupt for this instead of polling.
 */
void Task::checkForHotSwapTimers()
{
    // Check if any hot swap timer events are pending
    auto eventBits = static_cast<uint32_t>(hotSwapTimerEvent.bits().value());
    if (eventBits != 0) {
        // Clear the event bits and wait for clock enable to PCIe reset delay
        hotSwapTimerEvent.clear(eventBits);
        nv::ctimer::Driver::delay_for_us(nv::nhp::ClkEnToPerstDelayUs);

        // Process each set bit to trigger hot swap timer interrupts
        uint8_t driveIndex = 0;
        while (eventBits != 0) {
            if ((eventBits & 0x1U) != 0) {
                // Calculate AHS instance index from drive index
                const uint8_t ahsIndex = driveIndex / nv::nhp::NumE1sDrives;
                // coverity[cert_int31_c_violation] dont care about lost bits
                ahs_instances.at(ahsIndex)->hotSwapTimerInterrupt(
                    driveIndex - (ahsIndex * nv::nhp::NumE1sDrives));
            }
            eventBits >>= 1;
            // hotSwapTimerEvent vector cant be larger than number of drives by design
            // coverity[cert_int30_c_violation] see comment above
            driveIndex++;
        }
    }
}

}  // namespace nv::ahs

// Weak function definition for projectTryRunAdcTrigger (for testrunner project(esp. in
// simulation))
__attribute__((weak)) void projectTryRunAdcTrigger()
{
    return;
}
