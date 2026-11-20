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
#include <fsl_clock.h>
#include <portmacrocommon.h>
#include <task.h>
#include <mcmgr.h>

// #include <board/clock_config.h>
// #include <board/peripherals.h>
// #include <board/pin_mux.h>

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
#include "nv/ctimer/ctimer.h"
#include NV_IPC_CONFIG_H

#include "nv/i2c/task.h"
#include "nv/usb/task.h"
#include "nv/ipc/ipc_task.h"
#include "nv/logger/task.h"
#include "nv/bootloader.h"
#include "nv/gpio/driver.h"
#include "nv/ctimer/ctimer.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/watchdog/boot.h"
#include "nv/telemetry/cache.h"
#include "sys/sensor/sensor.h"

static void make_i2c_task()
{
    using namespace nv;
    // MCTP
    {
        constexpr auto config = i2c::Task::Config{
            std::get<1>(
                ipc::ClientInfos[static_cast<uint16_t>(ipc::DownStreamInfos[0].client)]),
            "I2C1",
            ipc::DownStreamInfos[0].client,
            i2c::Port::Zero,
            ipc::DownStreamInfos[0].port_address,
            ipc::BootedEventBits::I2c1};
        if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
            NVIC_EnableIRQ(sys::i2c::Driver::get_irq(config.port_id));
            i2c::make_task_by_port<i2c::Port::Zero>(config);
        }
    }

    {
        constexpr auto config = i2c::Task::Config{
            std::get<1>(
                ipc::ClientInfos[static_cast<uint16_t>(ipc::DownStreamInfos[1].client)]),
            "I2C2",
            ipc::DownStreamInfos[1].client,
            i2c::Port::One,
            ipc::DownStreamInfos[1].port_address,
            ipc::BootedEventBits::I2c2};
        if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
            NVIC_EnableIRQ(sys::i2c::Driver::get_irq(config.port_id));
            i2c::make_task_by_port<i2c::Port::One>(config);
        }
    }

    // I2C upstream
    {
        constexpr auto config = i2c::Task::Config{
            std::get<1>(ipc::ClientInfos[static_cast<uint16_t>(mctp::Client::UsI2c)]),
            "I2C0",
            mctp::Client::UsI2c,
            i2c::Port::Zero,
            i2c::Task::DynAddr,
            ipc::BootedEventBits::I2c0};
        if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
            NVIC_EnableIRQ(sys::i2c::Driver::get_irq(config.port_id));
            i2c::make_task_by_port<i2c::Port::Three>(config);
        }
    }
}

int main()
{
    // Enable IRQ for core1 to setup c2c
    __enable_irq();

    /* Init board hardware.*/
    /* enable clock for GPIO */
    // BOARD_InitBootPins();
    // BOARD_InitBootClocks();
    // BOARD_InitBootPeripherals();

    // Set interrupt callback & enable IRQ
    auto mcmgr_status = MCMGR_Init();

    ipc::task::Task::make(ipc::task::Task::Config{ipc::TaskId::Ipc1, "IPC1"});
    // IRQ is enabled manually, disable it
    __disable_irq();

    // Set priority for all irq
    bootloader::Driver::hw_init();

    // nv::watchdog::Boot::start_watchdog(nv::ipc::WatchdogResetMs);
    nv::debug("MCMGR_Init mcmgr_status: %d\n", mcmgr_status);
    nv::debug("USB MCU Start\n");
    nv::debug(
        "FW VERSION: %d.%d.%d.%d\n", MCU_FW_MAJOR, MCU_FW_MINOR, MCU_FW_PATCH, MCU_FW_BUILD);
    nv::debug("FW DATE: %d/%d/%d\n", MCU_FW_MONTH, MCU_FW_DAY, MCU_FW_YEAR);

    using namespace nv;

    // mctp::Task::make();
    // spdm::Task::make();
    // pldm::Task::make();
    // flash::Task::make();
    // usb::Task::make();
    // make_i2c_task();
    // logger::Task::make();

    // bootloader::Driver::boot_init();
    // gpio::Driver::init();
    // for (auto& [port, pin, det, sel] : nv::ipc::GpioInterruptSetup) {
    //     gpio::Driver::init_interrupt(port, pin, det, sel);
    // }
    // perf_mon::Driver::init();
    // telemetry::Cache::inst().init();
    // sys::sensor::Driver::init();

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
