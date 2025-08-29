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
#include <array>
#include <cstdint>

#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <fsl_clock.h>
#include <portmacrocommon.h>
#include <task.h>

// TODO: Determine why these three files must be included before NV_IPC_CONFIG_H
#include <board/clock_config.h>
#include <board/peripherals.h>
#include <board/pin_mux.h>

#include NV_IPC_CONFIG_H

#include "nv/common/debug.h"
#include "nv/common/system.h"
#include "nv/ipc/supervisor.h"
#include "nv/mctp/task.h"
#include "nv/flash/task.h"
#include "nv/nv.h"
#include "nv/pldm/task.h"
#include "nv/spdm/task.h"

#include "nv/i2c/task.h"
#include "nv/usb/task.h"
#include "nv/logger/task.h"
#include "nv/bootloader.h"
#include "nv/gpio/driver.h"
#include "nv/ctimer/ctimer.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/watchdog/boot.h"
#include "nv/telemetry/cache.h"
#include "nv/vpp/task.h"
#include "sys/adc/adc.h"
#include "nhp_config.h"

void make_i2c_task()
{
    using namespace nv;
    i2c::Task::Config config{ipc::TaskId::I2c0,
                             "I2C0",
                             mctp::Client::UsI2c,
                             ipc::EventId::I2c0,
                             ipc::QueueId::I2c0,
                             i2c::Port::Zero,
                             i2c::Task::DynAddr,
                             ipc::BootedEventBits::I2c0};
    i2c::Task::make(config);
}

// Specific to P3957 board
// Either sets clock expander to 2 host clocks for each half of drives
//     or 1 host clock for all 8 drives
enum P3957ClockTopology
{
    SingleSource,
    DualSource,
    HardwareStrap
};

// control pins are 3 voltage level normally but board adds divider since we only want low
//     and mid
// Low = CLKIN0 goes to all output
// High = CLKIN0 goes to Bank 0, CLKIN1 goes to Bank 1
void set_p3957_clock_topology(P3957ClockTopology topology_setting,
                              nv::gpio::GpioPort p3957_clock_ctrl_port,
                              nv::gpio::GpioPin  p3957_clock_ctrl_pin)
{
    switch (topology_setting) {
        case SingleSource:  // low
            nv::gpio::Driver::init_pin(p3957_clock_ctrl_port,
                                       p3957_clock_ctrl_pin,
                                       nv::gpio::Direction::Output,
                                       nv::gpio::GpioState::Low);
            break;
        case DualSource:  // high
            nv::gpio::Driver::init_pin(p3957_clock_ctrl_port,
                                       p3957_clock_ctrl_pin,
                                       nv::gpio::Direction::Output,
                                       nv::gpio::GpioState::High);
            break;
        case HardwareStrap:  // high Z
            nv::gpio::Driver::init_pin(p3957_clock_ctrl_port,
                                       p3957_clock_ctrl_pin,
                                       nv::gpio::Direction::Input,
                                       nv::gpio::GpioState::Low);
            break;
        default:
            nv::error("Invalid p3957 clock topology %d passed to NHP\n", topology_setting);
            break;
            //
    }
}

int main()
{
    // IRQ is enabled by default, disable it
    __disable_irq();
    // setup board
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    nv::watchdog::Boot::start_watchdog(nv::ipc::WatchdogResetMs);
    nv::bootloader::Driver::set_fmc_wp();

    nv::info("USB MCU Start\n");
    nv::info(
        "FW VERSION: %d.%d.%d.%d\n", MCU_FW_MAJOR, MCU_FW_MINOR, MCU_FW_PATCH, MCU_FW_BUILD);
    nv::info("FW DATE: %d/%d/%d\n", MCU_FW_MONTH, MCU_FW_DAY, MCU_FW_YEAR);

    using namespace nv;

    mctp::Task::make();
    spdm::Task::make();
    pldm::Task::make();
    flash::Task::make();
    usb::Task::make();
    make_i2c_task();
    logger::Task::make();

    bootloader::Driver::boot_init();
    ctimer::Driver::init();
    gpio::Driver::init();
    perf_mon::Driver::init();
    telemetry::Cache::inst().init();

    sys::adc::ADC::init_adc(0U);
    // DualSource is needed for A01 board
    // HardwareStrapped is needed for A02 board
    set_p3957_clock_topology(DualSource, 5U, BOARD_INITPINS_CLKSEL_MODE_GPIO_PIN);
    // set_p3957_clock_topology(HardwareStrap, 5U, BOARD_INITPINS_CLKSEL_MODE_GPIO_PIN);

    // Initialize the Hotplug Support Module/Task
    vpp::Task::make(nhp::vpp_task_config);

    ipc::Supervisor::inst().startup(0, nullptr);
    common::System::inst().scheduler_start();
    while (true) {}
}

// Required hooks for FreeRTOS with static allocation enabled
extern "C" void vApplicationGetTimerTaskMemory(StaticTask_t** taskTcbBuffer,
                                               StackType_t**  taskStackBuffer,
                                               uint32_t*      taskStackSize)
{
    static StaticTask_t                                          tcb;
    static std::array<StackType_t, configTIMER_TASK_STACK_DEPTH> stack;
    *taskTcbBuffer   = &tcb;
    *taskStackBuffer = stack.data();
    *taskStackSize   = stack.size();
}

extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** taskTcbBuffer,
                                              StackType_t**  taskStackBuffer,
                                              uint32_t*      taskStackSize)
{
    static StaticTask_t                                      tcb;
    static std::array<StackType_t, configMINIMAL_STACK_SIZE> stack;
    *taskTcbBuffer   = &tcb;
    *taskStackBuffer = stack.data();
    *taskStackSize   = stack.size();
}
