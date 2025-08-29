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
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <portmacrocommon.h>
#include <task.h>

#include "board/clock_config.h"
#include "board/peripherals.h"
#include "board/pin_mux.h"
#include "fsl_clock.h"

#include "nv/common/debug.h"
#include "nv/common/preproc.h"
#include "nv/common/system.h"
#include "nv/ipc/supervisor.h"
#include "nv/ipc/task.h"
#include "nv/mctp/task.h"
#include "nv/mctp/driver.h"
#include "nv/flash/task.h"
#include "nv/nv.h"
#include "nv/pldm/task.h"
#include "sys/ipc/task.h"
#include "nv/spdm/task.h"
#include NV_IPC_CONFIG_H

#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/usb/task.h"
#include "nv/bootloader.h"

#include "nv/logger/task.h"
#include "nv/gpio/driver.h"
#include "nv/ctimer/ctimer.h"
#include "nv/perf_mon/perf_mon.h"

#include "nv/watchdog/boot.h"
#include "nv/telemetry/cache.h"
#include "nv/ipchandler/enums.h"

static void make_i2c_task()
{
    using namespace nv;
    std::array<i2c::Task::Config, ipc::I2cDownStreamNum> list{
        i2c::Task::Config{ipc::TaskId::I2c1,
                          "I2C1", ipc::DownStreamInfos[0].client,
                          ipc::EventId::I2c1,
                          ipc::QueueId::I2c1,
                          i2c::Port::Four,
                          ipc::DownStreamInfos[0].port_address,
                          ipc::BootedEventBits::I2c1,
                          nv::ipc::TimerId::Ap1Status},
        i2c::Task::Config{ipc::TaskId::I2c2,
                          "I2C2", ipc::DownStreamInfos[1].client,
                          ipc::EventId::I2c2,
                          ipc::QueueId::I2c2,
                          i2c::Port::One,
                          ipc::DownStreamInfos[1].port_address,
                          ipc::BootedEventBits::I2c2,
                          nv::ipc::TimerId::Ap2Status},
    };
    for (const auto& config : list) {
        i2c::Task::make(config);
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

    // create a helloworld task and startup supervisor
    using namespace nv;

    mctp::Task::make();
    spdm::Task::make();
    pldm::Task::make();
    usb::Task::make();
    make_i2c_task();
    flash::Task::make();
    logger::Task::make();

    bootloader::Driver::boot_init();
    ctimer::Driver::init();
    gpio::Driver::init();
    for (auto& [port, pin, det, sel] : nv::ipc::GpioInterruptSetup) {
        gpio::Driver::init_interrupt(port, pin, det, sel);
    }
    perf_mon::Driver::init();
    telemetry::Cache::inst().init();

    // output mcu_mux_sel to high to enable i2c path
    gpio::Driver::init_pin(ipc::S_MCU_MUX_SEL_PORT,
                           ipc::S_MCU_MUX_SEL_PIN,
                           gpio::Direction::Output,
                           gpio::GpioState::High);

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
