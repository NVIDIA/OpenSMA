/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include <board/clock_config.h>
#include <board/peripherals.h>
#include <board/pin_mux.h>

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
#include "nv/diag/task.h"
#include NV_IPC_CONFIG_H

#include "nv/i2c/task.h"
#include "nv/usb/task.h"
#include "nv/ipc/ipc_task.h"
#include "nv/logger/task.h"
#include "nv/bootloader.h"
#include "nv/gpio/driver.h"
#include "nv/lstp/lstp_task.h"
#include "nv/ssif/task.h"
#include "nv/i3c/task.h"
#include "nv/ctimer/ctimer.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/watchdog/boot.h"
#include "nv/telemetry/cache.h"
#include "sys/sensor/sensor.h"
#include "nv/ipchandler/enums.h"
#include <trustzone/resource_config.h>
#include "sys/common/AHB_config.h"
#include "sys/smartdma/driver.h"
#include "nv/spi/flashrom_task.h"

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
            ipc::EventId::I2c1,
            ipc::QueueId::I2c1,
            i2c::Port::Three,
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
            ipc::EventId::I2c2,
            ipc::QueueId::I2c2,
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
            ipc::EventId::I2c0,
            ipc::QueueId::I2c0,
            i2c::Port::Zero,
            i2c::Task::DynAddr,
            ipc::BootedEventBits::I2c0};
        if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
            NVIC_EnableIRQ(sys::i2c::Driver::get_irq(config.port_id));
            i2c::make_task_by_port<i2c::Port::Three>(config);
        }
    }

    // LSTP
    {
        constexpr auto config = i2c::Task::Config{ipc::TaskId::I2c3,
                                                  "I2C3",
                                                  mctp::Client::End,
                                                  ipc::EventId::I2c3,
                                                  ipc::QueueId::I2c3,
                                                  i2c::Port::Six,
                                                  i2c::Task::DynAddr,
                                                  ipc::BootedEventBits::I2c3,
                                                  nv::ipc::TimerId::End,
                                                  nv::ipchandler::Id::I2c3,
                                                  ipc::TimerId::I2c3RepeatedStartTimeout};
        if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
            NVIC_EnableIRQ(sys::i2c::Driver::get_irq(config.port_id));
            i2c::make_task_by_port<i2c::Port::Six>(config);
        }
    }
}

static void make_i3c_task()
{
    using namespace nv;
    const nv::i3c::Task::Config config{
        std::get<1>(ipc::ClientInfos[static_cast<uint16_t>(mctp::Client::DsI3c0)]),
        "I3C",
        mctp::Client::DsI3c0,
        ipc::EventId::I3c0,
        ipc::QueueId::I3c0,
        i3c::Driver::Port::One,
        ipc::BootedEventBits::I3c0,
        {400000U, 1000000U, 12500000U},
        true,
        0x6c,
        0x4c,
        ipc::TimerId::Gpu1Seneor,
        {0, telemetry::TelemId::Gpu1Temp},
        {nv::gpio::InvalidGpioPort, nv::gpio::InvalidGpioPin},
        nv::ipchandler::Id::I3c0,
        {nv::i2c::Port::One, 0x50},
    };
    if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
        nv::i3c::Task::make(config);
    }
}

void make_spi_task()
{
    // LSTP
    using namespace nv;
    spi::FlashromTask::Config config{
        ipc::TaskId::Spi0,
        "SPI0",
        spi::Flexcomm::Seven,
        nv::ipc::SPI0_CS0_PORT,
        nv::ipc::SPI0_CS0_PIN,
        nv::ipc::SPI0_CS1_PORT,
        nv::ipc::SPI0_CS1_PIN,
        ipc::BootedEventBits::Spi0,
    };
    if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
        spi::FlashromTask::make(config);
    }
}

static void make_lstp_task()
{
    // LSTP GPIO
    using namespace nv;
    if constexpr (ipc::EnableLstp && lstp::EnableGpio) {
        lstp::LstpTask::Config config{
            ipc::TaskId::Lstp,
            "LSTP",
            ipc::BootedEventBits::Lstp,
        };
        if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
            lstp::LstpTask::make(config);
        }
    }
}

static void make_ssif_task()
{
    using namespace nv;
    ssif::Task::Config config{ipc::TaskId::Ssif,
                              "SSIF",
                              ipc::EventId::Ssif,
                              i2c::Port::Two,
                              nv::ipc::CPU0_SSIF_PORT,
                              nv::ipc::CPU0_SSIF_PIN};
    if (sys::ipc::task::Driver::can_direct_access_on_current_core(config.task_id)) {
        ssif::Task::make(config);
    }
}

void SystemInitHook(void)
{
    /* Initialize MCMGR - low level multicore management library. Call this
       function as close to the reset entry as possible to allow CoreUp event
       triggering. The SystemInitHook() weak function overloading is used in this
       application. */
    (void)MCMGR_EarlyInit();
    BOARD_InitBootTEE();
}

extern "C" {
extern uint8_t  __core1_text_start__;
extern uint32_t __shared_memory_start__;
}

int main()
{
    // IRQ is enabled by default, disable it
    __disable_irq();

    // setup board
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    // disable all flexcomm interrupt
    nv::bootloader::Driver::disable_all_flexcomm_irq();

    nv::watchdog::Boot::start_watchdog(nv::ipc::WatchdogResetMs);
    nv::bootloader::Driver::set_fmc_wp();

    // Set interrupt callback & enable IRQ
    auto mcmgr_status = MCMGR_Init();
    nv::info("MCMGR_Init mcmgr_status: %d\n", mcmgr_status);
    nv::info("USB MCU Start\n");
    nv::info(
        "FW VERSION: %d.%d.%d.%d\n", MCU_FW_MAJOR, MCU_FW_MINOR, MCU_FW_PATCH, MCU_FW_BUILD);
    nv::info("FW DATE: %d/%d/%d\n", MCU_FW_MONTH, MCU_FW_DAY, MCU_FW_YEAR);

    using namespace nv;

    // Init c2c communication to let core0 can start send data to core1
    nv::ipc::task::Driver::init_c2c_communication();

    mctp::Task::make();
    diag::Task::make();
    spdm::Task::make();
    pldm::Task::make();
    flash::Task::make();
    usb::Task::make();
    make_i2c_task();
    if constexpr (ipc::EnableSmartDMA) {
        sys::smartdma::Driver::init();
    }
    make_i3c_task();
    gpio::Driver::init_pin(nv::ipc::SPI0_CS0_PORT,
                           nv::ipc::SPI0_CS0_PIN,
                           gpio::Direction::Output,
                           gpio::GpioState::High);
    gpio::Driver::init_pin(nv::ipc::SPI0_CS1_PORT,
                           nv::ipc::SPI0_CS1_PIN,
                           gpio::Direction::Output,
                           gpio::GpioState::High);
    make_spi_task();
    logger::Task::make();

    bootloader::Driver::set_stack_cookie();
    bootloader::Driver::boot_init();
    ctimer::Driver::init();
    gpio::Driver::init();
    lstp::LstpTask::LstpGpioInit();
    for (auto& [port, pin, det, sel] : nv::ipc::GpioInterruptSetup) {
        gpio::Driver::init_interrupt(port, pin, det, sel);
    }
    make_lstp_task();

    // Set SSIF alert pin to high (inactive)
    gpio::Driver::init_pin(nv::ipc::CPU0_SSIF_PORT,
                           nv::ipc::CPU0_SSIF_PIN,
                           gpio::Direction::Output,
                           gpio::GpioState::High);
    make_ssif_task();

    perf_mon::Driver::init();
    telemetry::Cache::inst().init();
    sys::sensor::Driver::init();

    ipc::task::Task::make(
        ipc::task::Task::Config{ipc::TaskId::Ipc0,
                                "IPC0",
                                reinterpret_cast<uint32_t>(&__core1_text_start__),
                                reinterpret_cast<uint32_t>(&__shared_memory_start__)});

    ipc::Supervisor::inst().startup(0, nullptr);
    /*
        // Enable security check and configure AHB rules for the core1
        // It will fix the reset issue caused by authentication
        // after pldm update complete and  MCU reset
    */
    sys::common::AHBConfig();
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
