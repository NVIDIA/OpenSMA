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

#if defined(NV_UART_INSTANCE) && (NV_UART_INSTANCE == 2)
#include "fsl_lpuart.h"
/* Definition of peripheral ID */
#define LP_FLEXCOMM2_UART_PERIPHERAL   ((LPUART_Type*)LP_FLEXCOMM2)
/* Definition of the clock source frequency */
#define LP_FLEXCOMM2_UART_CLOCK_SOURCE 12000000UL

const lpuart_config_t LP_FLEXCOMM2_uart_config = {
    .baudRate_Bps     = 115200UL,
    .parityMode       = kLPUART_ParityDisabled,
    .dataBitsCount    = kLPUART_EightDataBits,
    .isMsb            = false,
    .stopBitCount     = kLPUART_OneStopBit,
    .txFifoWatermark  = 0U,
    .rxFifoWatermark  = 1U,
    .enableRxRTS      = false,
    .enableTxCTS      = false,
    .txCtsSource      = kLPUART_CtsSourcePin,
    .txCtsConfig      = kLPUART_CtsSampleAtStart,
    .rxIdleType       = kLPUART_IdleTypeStartBit,
    .rxIdleConfig     = kLPUART_IdleCharacter1,
    .timeoutConfig    = {.rxExtendedTimeoutValue = 0U,
                         .txExtendedTimeoutValue = 0U,
                         .rxCounter0             = {.enableCounter    = false,
                                                    .timeoutCondition = kLPUART_TimeoutAfterCharacters,
                                                    .timeoutValue     = 0U},
                         .rxCounter1             = {.enableCounter    = false,
                                                    .timeoutCondition = kLPUART_TimeoutAfterCharacters,
                                                    .timeoutValue     = 0U},
                         .txCounter0             = {.enableCounter    = false,
                                                    .timeoutCondition = kLPUART_TimeoutAfterCharacters,
                                                    .timeoutValue     = 0U},
                         .txCounter1             = {.enableCounter    = false,
                                                    .timeoutCondition = kLPUART_TimeoutAfterCharacters,
                                                    .timeoutValue     = 0U}},
    .enableSingleWire = false,
    .rtsDelay         = 0,
    .enableTx         = true,
    .enableRx         = true,
};

static void enable_debug_uart2(void)
{
    LPI2C_MasterDeinit(LP_FLEXCOMM2_PERIPHERAL);
    LPUART_Init(LP_FLEXCOMM2_UART_PERIPHERAL,
                &LP_FLEXCOMM2_uart_config,
                LP_FLEXCOMM2_UART_CLOCK_SOURCE);
}
#endif  // #if defined(NV_UART_INSTANCE) && (NV_UART_INSTANCE == 2)

static void make_i2c_task()
{
    using namespace nv;

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
#if !defined(NV_UART_INSTANCE) || (NV_UART_INSTANCE != 2)
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
#endif  // #if !defined(NV_UART_INSTANCE) || (NV_UART_INSTANCE != 2)
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

#if 0
// Disable this code for now, but preserve it for future reference
// Specific to P3957 board
// There are two clock muxes on the board.  One selects the clock for SSD0-3
// the other selects the clock for SSD4-7.  For SSD0-3, the clock can be either
// MCIO0 or MCIO1 or split (SSD0-1 on MCIO0, SSD2-3 on MCIO1).  For SSD4-7, the clock
// can be either MCIO2 or MCIO3 or split (SSD4-5 on MCIO2, SSD6-7 on MCIO3).
// A low signal selects MCIO0/MCIO2, a high signal selects MCIO1/MCIO3, and a
// mid signal (input/high-Z) selects split.
enum P3957ClockTopology
{
    SingleSourceClock0,
    SingleSourceClock1,
    HardwareStrap
};

void set_p3957_clock_topology(P3957ClockTopology topology_setting)
{
    using namespace nv;
    auto const _port = ipc::Port::MCU_CLKSEL_MODE_PORT;
    auto const _pin = ipc::Pin::MCU_CLKSEL_MODE_PIN;
    auto const _output = nv::gpio::Direction::Output;
    auto const _input = nv::gpio::Direction::Input;
    auto const _low = nv::gpio::GpioState::Low;
    auto const _high = nv::gpio::GpioState::High;
    auto const _hi_z = nv::gpio::GpioState::Low;

    switch (topology_setting) {
        case SingleSourceClock0:  // low
            nv::gpio::Driver::init_pin(_port, _pin, _output, _low);
            break;
        case SingleSourceClock1:  // high
            nv::gpio::Driver::init_pin(_port, _pin, _output, _high);
            break;
        case HardwareStrap:  // high Z
            nv::gpio::Driver::init_pin(_port, _pin, _input, _hi_z);
            break;
        default:
            nv::error("Invalid p3957 clock topology %d passed to NHP\n", topology_setting);
            break;
    }
}
#endif  // #if 0

#if defined(DEBUG_E1S_INIT) && (DEBUG_E1S_INIT != 0)
// If DEBUG_E1S_INIT is non-zero, enable this code
// If DEBUG_E1S_INIT is 2, then check Drive 7 connected to force hard-coded init
// If DEBUG_E1S_INIT is 1, then just force hard-coded init without a check
void debug_init_e1_s_drives()
{
    using namespace nv;
    using namespace nv::nhp;
    nv::info("debug_init_e1_s_drives()\n");
    constexpr std::array<E1sInputPins, 8>  e1s_input_pins  = {Drive0InputPins,
                                                              Drive1InputPins,
                                                              Drive2InputPins,
                                                              Drive3InputPins,
                                                              Drive4InputPins,
                                                              Drive5InputPins,
                                                              Drive6InputPins,
                                                              Drive7InputPins};
    constexpr std::array<E1sOutputPins, 8> e1s_output_pins = {Drive0OutputPins,
                                                              Drive1OutputPins,
                                                              Drive2OutputPins,
                                                              Drive3OutputPins,
                                                              Drive4OutputPins,
                                                              Drive5OutputPins,
                                                              Drive6OutputPins,
                                                              Drive7OutputPins};

    // Initialize the GPIO driver for use by the E1.S drives
    nv::gpio::Driver::init();

#if (DEBUG_E1S_INIT == 2)
    // Read the presence detection state of drive 7.
    // This will be used to determine if we should boot the normal code,
    // or the static/init debug code.
    nv::gpio::Driver::init_pin(e1s_input_pins.at(7).prsnt_l_port,
                               e1s_input_pins.at(7).prsnt_l_pin,
                               nv::gpio::Direction::Input,
                               nv::gpio::GpioState::High);
    uint8_t                ssd7_prsnt_l = 0;
    const nv::gpio::Status status       = nv::gpio::Driver::read(
        e1s_input_pins.at(7).prsnt_l_port, e1s_input_pins.at(7).prsnt_l_pin, ssd7_prsnt_l);

    // If the read failed, log an error and return to boot the normal code
    if (status != gpio::Status::Ok) {
        nv::error("Failed to read prsntL for drive 7\n");
        return;
    }

    // If the presence detection state of drive 7 is high, return to boot the normal code
    if (ssd7_prsnt_l != 0) {
        return;
    }
#endif  // #if (DEBUG_E1S_INIT == 2)

    // Here, static init of all the SSD drive GPIOS should be done.
    for (uint8_t driveIndex = 0; driveIndex < 8; driveIndex++) {
        // Initialize the presence detection input pin as an input
        nv::gpio::Driver::init_pin(e1s_input_pins.at(driveIndex).prsnt_l_port,
                                   e1s_input_pins.at(driveIndex).prsnt_l_pin,
                                   nv::gpio::Direction::Input,
                                   nv::gpio::GpioState::High);

        // Initialize the PCIe reset deasserted output pin as HiZ (for Drive-On state)
        nv::gpio::Driver::init_pin(e1s_output_pins.at(driveIndex).perst_l_port,
                                   e1s_output_pins.at(driveIndex).perst_l_pin,
                                   nv::gpio::Direction::Input,
                                   nv::gpio::GpioState::High);

        // Initialize the clock enable output pin as Low (for Drive-On state)
        nv::gpio::Driver::init_pin(e1s_output_pins.at(driveIndex).clk_en_l_port,
                                   e1s_output_pins.at(driveIndex).clk_en_l_pin,
                                   nv::gpio::Direction::Output,
                                   nv::gpio::GpioState::Low);

        // Initialize the power disable output pin Low (for Drive-On state)
        nv::gpio::Driver::init_pin(e1s_output_pins.at(driveIndex).pwrdis_port,
                                   e1s_output_pins.at(driveIndex).pwrdis_pin,
                                   nv::gpio::Direction::Output,
                                   nv::gpio::GpioState::Low);

        // Initialize the 12V power enable asserted output pin as Low (for Drive-On state)
        nv::gpio::Driver::init_pin(e1s_output_pins.at(driveIndex).pwren_l_port,
                                   e1s_output_pins.at(driveIndex).pwren_l_pin,
                                   nv::gpio::Direction::Output,
                                   nv::gpio::GpioState::Low);

        // Initialize the LED tristate control output pin as Low (for Drive-On state - Blue)
        nv::gpio::Driver::init_pin(e1s_output_pins.at(driveIndex).led_tristate_ctrl_port,
                                   e1s_output_pins.at(driveIndex).led_tristate_ctrl_pin,
                                   nv::gpio::Direction::Output,
                                   nv::gpio::GpioState::Low);
    }

    // Now, just spin here forever.
    while (true) {
        // Just spin here forever.
    }
}
#endif  // #if defined(DEBUG_E1S_INIT) && (DEBUG_E1S_INIT != 0)

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

#if defined(NV_UART_INSTANCE) && (NV_UART_INSTANCE == 2)
    enable_debug_uart2();
    nv::info("DEBUG-UART2 Enabled\n");
#endif  // #if defined(NV_UART_INSTANCE) && (NV_UART_INSTANCE == 2)

#if defined(DEBUG_E1S_INIT) && (DEBUG_E1S_INIT != 0)
    // Initialize the E1.S drives for debug
    debug_init_e1_s_drives();
#endif  // #if defined(DEBUG_E1S_INIT) && (DEBUG_E1S_INIT != 0)

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

#if 0
    // Setup the clock topology for the P3957 board
    set_p3957_clock_topology(HardwareStrap);
#endif  // #if 0

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
    return false;
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
    return nv::ipc::QueueId::End;
}

void project_stop_polling_timers()
{
    constexpr int32_t DefaultPollingTimeout = 10;
    nv::ipc::Timer::make(nv::ipc::TimerId::Ap4Status,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::Ap5Status,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::Ap6Status,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::Ap7Status,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
    nv::ipc::Timer::make(nv::ipc::TimerId::Ap8Status,
                         std::chrono::seconds(DefaultPollingTimeout),
                         [](nv::ipc::Timer& timer) {})
        .stop();
}