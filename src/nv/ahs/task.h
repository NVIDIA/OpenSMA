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
#include <array>
#include <chrono>
#include <optional>

#include NV_IPC_CONFIG_H

#include "nv/ipc/event.h"
#include "nv/ipc/task.h"
#include "nv/ahs/ahs.h"
#include "sys/adc/adc.h"

namespace nv::ahs {

/**
 * @brief Forward declaration of the Task class
 */
class Task;

/**
 * @brief Configuration structure for AHS task initialization
 *
 * Contains all necessary configuration parameters for setting up the AHS task,
 * including task identification, AHS configurations for all instances, and
 * event identifiers for hot swap operations.
 */
struct TaskConfig
{
    /** @brief Supervisor task identifier */
    nv::ipc::TaskId task_id;

    /** @brief Human-readable task name for identification */
    const std::string_view task_name;

    /** @brief Array of AHS configurations for all NHP instances */
    std::array<AHS::AhsConfig, nhp::NumNhpInstances> ahs_configs;

    /** @brief Event identifier for hot swap timer events */
    nv::ipc::Event::IdType hotSwapEventId;

    /** @brief Delay time for the AHS task */
    std::chrono::microseconds delay_time;
};

/**
 * @brief AHS Task class for managing AHS operations across multiple instances
 *
 * The Task class extends the IPC Task base class to provide AHS-specific task
 * management. It coordinates operations across multiple AHS instances, handles
 * GPIO and ADC interrupts, manages hot swap timers, and runs the main ADC
 * polling loop for voltage monitoring.
 */
class Task : public nv::ipc::Task
{
public:
    // Public Functions
    // ---------------------------------------------

    /**
     * @brief Creates task objects for supervisor management
     *
     * Static factory method that creates and configures the AHS task instance
     * with the specified configuration. Sets up the task stack and priority.
     *
     * @param task_config Configuration structure for the task
     */
    static void make(const TaskConfig& task_config);

    /**
     * @brief Constructor for AHS task instance
     *
     * Initializes the task with the provided configuration, creates AHS instances
     * for all NHP instances, and sets up the hot swap timer event.
     *
     * @param config Configuration structure for the task
     */
    Task(const TaskConfig& config) noexcept;

    /**
     * @brief Entry point function called by supervisor to start the task
     *
     * Static function that serves as the entry point for the task execution.
     * Calls the instance's start method and then suspends the task.
     *
     * @param params Pointer to the task instance (cast from void*)
     */
    static void entrypoint(void* params);

    /**
     * @brief Main task execution loop
     *
     * Starts the I2C driver and runs the main ADC polling loop. The loop
     * continuously monitors for ADC triggers and checks hot swap timers.
     * This method runs indefinitely until the task is suspended.
     */
    void start();

    /**
     * @brief Static callback for GPIO interrupt handling
     *
     * Routes GPIO interrupts to the appropriate AHS instance based on
     * the AHS instance number and drive index.
     *
     * @param port GPIO port that generated the interrupt
     * @param pin GPIO pin that generated the interrupt
     * @param ahsInstance AHS instance number to route the interrupt to
     * @param driveIndex Drive index within the AHS instance
     */
    static void gpio_interrupt_callback(nv::gpio::GpioPort port,
                                        nv::gpio::GpioPin  pin,
                                        uint8_t            ahsInstance,
                                        uint8_t            driveIndex);

    /**
     * @brief Static callback for ADC interrupt handling
     *
     * Routes ADC interrupts to the appropriate AHS instance and processes
     * ADC FIFO data for voltage monitoring.
     *
     * @param peripheral ADC peripheral that generated the interrupt
     * @param command ADC command that generated the interrupt
     * @param value ADC value that generated the interrupt
     */
    static void adc_interrupt_callback(uint16_t value, uint8_t ahsInstance, uint8_t driveIndex);

private:
    // Helper Functions
    // ---------------------------------------------

    /**
     * @brief Checks for expired hot swap timers across all AHS instances
     *
     * Iterates through all AHS instances to check if any hot swap timers
     * have expired and need to trigger state machine updates.
     */
    void checkForHotSwapTimers();

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
    static bool isAhsInstanceDriveValid(uint8_t ahsInstance, uint8_t driveIndex);

    // Member Constants
    // ---------------------------------------------

    // Member Variables
    // ---------------------------------------------

    /** @brief Static pointer to the single task instance */
    static Task* taskInstance;

    /** @brief Array of AHS instances for all NHP instances */
    std::array<std::optional<AHS>, nhp::NumNhpInstances> ahs_instances;

    /** @brief Reference to the hot swap timer event */
    nv::ipc::Event& hotSwapTimerEvent;

    /** @brief Delay time for the AHS task */
    std::chrono::microseconds delay_time;
};

}  // namespace nv::ahs