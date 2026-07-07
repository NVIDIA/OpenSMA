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
#include "nv/usb/task.h"
#include "nv/bootloader.h"

#include "nv/logger/task.h"
#include "nv/gpio/driver.h"
#include "nv/lstp/lstp_task.h"
#include "nv/ctimer/ctimer.h"
#include "nv/perf_mon/perf_mon.h"

#include "nv/watchdog/boot.h"
#include "nv/telemetry/cache.h"
#include "nv/ipchandler/enums.h"
#include "nv/ahs/task.h"
#include "sys/adc/adc.h"
#include "nhp_config.h"

static void make_i2c_task()
{
    using namespace nv;

    // SSD<->bus mapping (FLEXCOMM):
    //   SSD0 - I2C1 - Port::Seven - DownStreamInfos[0] - Client::DsI2c0 - Ap1Status
    //   SSD1 - I2C2 - Port::One   - DownStreamInfos[1] - Client::DsI2c1 - Ap2Status
    //   SSD2 - I2C3 - Port::Two   - DownStreamInfos[2] - Client::DsI2c2 - Ap3Status
    //   SSD3 - I2C4 - Port::Four  - DownStreamInfos[3] - Client::DsI2c3 - Ap4Status
    //   SSD4 - I2C5 - Port::Five  - DownStreamInfos[4] - Client::DsI2c4 - Ap5Status
    //   SSD5 - I2C6 - Port::Eight - DownStreamInfos[5] - Client::DsI2c5 - Ap6Status
    //   SSD6 - I2C7 - Port::Three - DownStreamInfos[6] - Client::DsI2c6 - Ap7Status
    //   SSD7 - I2C8 - Port::Zero  - DownStreamInfos[7] - Client::DsI2c7 - Ap8Status
    i2c::make_task_by_port<i2c::Port::Seven>(
        i2c::Task::Config{ipc::TaskId::I2c1,
                          "I2C1",
                          ipc::DownStreamInfos[0].client,
                          ipc::EventId::I2c1,
                          ipc::QueueId::I2c1,
                          i2c::Port::Seven,
                          ipc::DownStreamInfos[0].port_address,
                          ipc::BootedEventBits::I2c1,
                          nv::ipc::TimerId::Ap1Status,
                          nv::ipchandler::Id::I2c1});
    i2c::make_task_by_port<i2c::Port::One>(
        i2c::Task::Config{ipc::TaskId::I2c2,
                          "I2C2",
                          ipc::DownStreamInfos[1].client,
                          ipc::EventId::I2c2,
                          ipc::QueueId::I2c2,
                          i2c::Port::One,
                          ipc::DownStreamInfos[1].port_address,
                          ipc::BootedEventBits::I2c2,
                          nv::ipc::TimerId::Ap2Status,
                          nv::ipchandler::Id::I2c2});
    i2c::make_task_by_port<i2c::Port::Two>(
        i2c::Task::Config{ipc::TaskId::I2c3,
                          "I2C3",
                          ipc::DownStreamInfos[2].client,
                          ipc::EventId::I2c3,
                          ipc::QueueId::I2c3,
                          i2c::Port::Two,
                          ipc::DownStreamInfos[2].port_address,
                          ipc::BootedEventBits::I2c3,
                          nv::ipc::TimerId::Ap3Status,
                          nv::ipchandler::Id::I2c3});
    i2c::make_task_by_port<i2c::Port::Four>(
        i2c::Task::Config{ipc::TaskId::I2c4,
                          "I2C4",
                          ipc::DownStreamInfos[3].client,
                          ipc::EventId::I2c4,
                          ipc::QueueId::I2c4,
                          i2c::Port::Four,
                          ipc::DownStreamInfos[3].port_address,
                          ipc::BootedEventBits::I2c4,
                          nv::ipc::TimerId::Ap4Status,
                          nv::ipchandler::Id::I2c4});
    i2c::make_task_by_port<i2c::Port::Five>(
        i2c::Task::Config{ipc::TaskId::I2c5,
                          "I2C5",
                          ipc::DownStreamInfos[4].client,
                          ipc::EventId::I2c5,
                          ipc::QueueId::I2c5,
                          i2c::Port::Five,
                          ipc::DownStreamInfos[4].port_address,
                          ipc::BootedEventBits::I2c5,
                          nv::ipc::TimerId::Ap5Status,
                          nv::ipchandler::Id::I2c5});
    i2c::make_task_by_port<i2c::Port::Eight>(
        i2c::Task::Config{ipc::TaskId::I2c6,
                          "I2C6",
                          ipc::DownStreamInfos[5].client,
                          ipc::EventId::I2c6,
                          ipc::QueueId::I2c6,
                          i2c::Port::Eight,
                          ipc::DownStreamInfos[5].port_address,
                          ipc::BootedEventBits::I2c6,
                          nv::ipc::TimerId::Ap6Status,
                          nv::ipchandler::Id::I2c6});
    i2c::make_task_by_port<i2c::Port::Three>(
        i2c::Task::Config{ipc::TaskId::I2c7,
                          "I2C7",
                          ipc::DownStreamInfos[6].client,
                          ipc::EventId::I2c7,
                          ipc::QueueId::I2c7,
                          i2c::Port::Three,
                          ipc::DownStreamInfos[6].port_address,
                          ipc::BootedEventBits::I2c7,
                          nv::ipc::TimerId::Ap7Status,
                          nv::ipchandler::Id::I2c7});
    i2c::make_task_by_port<i2c::Port::Zero>(
        i2c::Task::Config{ipc::TaskId::I2c8,
                          "I2C8",
                          ipc::DownStreamInfos[7].client,
                          ipc::EventId::I2c8,
                          ipc::QueueId::I2c8,
                          i2c::Port::Zero,
                          ipc::DownStreamInfos[7].port_address,
                          ipc::BootedEventBits::I2c8,
                          nv::ipc::TimerId::Ap8Status,
                          nv::ipchandler::Id::I2c8});
}

static void make_lstp_task()
{
    using namespace nv;
    if constexpr (ipc::EnableLstp && lstp::EnableGpio) {
        lstp::LstpTask::Config config{
            ipc::TaskId::Lstp,
            "LSTP",
            ipc::BootedEventBits::Lstp,
        };
        lstp::LstpTask::make(config);
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
    usb::Task::make();
    make_i2c_task();
    flash::Task::make();
    logger::Task::make();

    bootloader::Driver::set_stack_cookie();
    bootloader::Driver::boot_init();
    ctimer::Driver::init();
    gpio::Driver::init();
    make_lstp_task();
    perf_mon::Driver::init();
    telemetry::Cache::inst().init();

    sys::adc::ADC::init_adc(0U);
    sys::adc::ADC::init_adc(1U);

    // Initialize the Hotplug Support Module/Task
    ahs::Task::make(nhp::ahs_task_config);

    ipc::Supervisor::inst().startup(0, nullptr);
    common::System::inst().scheduler_start();
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
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

// Strong overrides for the weak hooks declared in src/nv/i2c/task.cpp.
bool projectIsApStatusTimer(nv::ipc::Timer& timer)
{
    switch (timer.id()) {
        case nv::ipc::TimerId::Ap1Status: return true;
        case nv::ipc::TimerId::Ap2Status: return true;
        case nv::ipc::TimerId::Ap3Status: return true;
        case nv::ipc::TimerId::Ap4Status: return true;
        case nv::ipc::TimerId::Ap5Status: return true;
        case nv::ipc::TimerId::Ap6Status: return true;
        case nv::ipc::TimerId::Ap7Status: return true;
        case nv::ipc::TimerId::Ap8Status: return true;
        default                         : return false;
    }
}

nv::ipc::QueueId projectGetApStatusQueueId(nv::ipc::Timer& timer)
{
    switch (timer.id()) {
        case nv::ipc::TimerId::Ap1Status: return nv::ipc::QueueId::I2c1;
        case nv::ipc::TimerId::Ap2Status: return nv::ipc::QueueId::I2c2;
        case nv::ipc::TimerId::Ap3Status: return nv::ipc::QueueId::I2c3;
        case nv::ipc::TimerId::Ap4Status: return nv::ipc::QueueId::I2c4;
        case nv::ipc::TimerId::Ap5Status: return nv::ipc::QueueId::I2c5;
        case nv::ipc::TimerId::Ap6Status: return nv::ipc::QueueId::I2c6;
        case nv::ipc::TimerId::Ap7Status: return nv::ipc::QueueId::I2c7;
        case nv::ipc::TimerId::Ap8Status: return nv::ipc::QueueId::I2c8;
        default                         : return nv::ipc::QueueId::End;
    }
}
