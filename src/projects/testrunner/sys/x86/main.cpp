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
#include <cstdlib>
#include <thread>

#include "testrunner/config.h"

// FreeRTOS
#include <cstdint>
#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <portmacro.h>
#include <task.h>

#include "nv/common/debug.h"
#include "nv/common/enum_ops.h"
#include "nv/common/system.h"
#include "nv/flash/task.h"
#include "nv/i2c/lattice_driver.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/task.h"
#include "nv/mctp/task.h"
#include "nv/pldm/task.h"
#include "nv/spdm/task.h"
#include "nv/usb/task.h"

// CPLD stub instance for testrunner (x86 test environment)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)
nv::i2c::LatticeCpld cpld(CPLD_I2C_PORT_PRGM,
                          CPLD_I2C_ADDR_PRGM,
                          CPLD_I2C_PORT_USR,
                          CPLD_I2C_ADDR_USR,
                          CPLD_I2C_ADDR_DBG,
                          CPLD_I2C_ADDR_DBG_INSTALL);

using namespace nv;
using namespace std::chrono_literals;
using common::fatal;

class Task : public ipc::Task
{
public:
    Task() noexcept : ipc::Task(nv::ipc::TaskId::TestRunner, "TEST") {}

    static auto& make()
    {
        constexpr auto StackSize = std::max(1024, int(configMINIMAL_STACK_SIZE));

        static Task                           task;
        static sys::ipc::TaskStack<StackSize> stack;

        const std::span<uint8_t> priv;
        task.setup(stack.span(), priv, Priority::Norm, Task::entrypoint);
        return task;
    }

    static void entrypoint(void* params)
    {
        NV_ASSERT(params != nullptr);
        auto& task = *static_cast<nv::ipc::Task*>(params);
        task.suspend();
        task.delay(1ms);
        // vTaskDelete(nullptr);
        // coverity[no_escape] - intentional infinite loop
        while (true) {
            task.delay(1ms);
        }
    }  // GBS: NO COVERAGE FIXME!!
};

extern "C" int  ada_print_hello();
extern "C" void adainit(void);
extern "C" void adafinal(void);

void identify_i2c_temp_sensor()
{
    const bool bom_primary_source = true;

    // Use the CPU1_Die sensor to identify the BOM primary/secondary source
    const uint8_t CPU1_Die_Primary_Addr = 0x48;
    const uint8_t id                    = nv::i2c::ManufacturerId::TI;

    for (auto& sensor : nv::mctp::I2cTempSensorList) {
        const uint8_t addr = CPU1_Die_Primary_Addr;

        // based on the primary source identification, determine the Nvlink sensor model.
        if (sensor.sensor_id == nv::mctp::Type3TemperatureSensors::NvLink_Temp) {
            sensor.identified_addr = addr;
            if (bom_primary_source) {
                sensor.sensor_model = nv::i2c::SensorModel::Sensor_Tmp1075;
            }
            else {
                sensor.sensor_model = nv::i2c::SensorModel::Sensor_Nct70;
            }
        }
        else {
            if (id == nv::i2c::ManufacturerId::TI) {
                sensor.identified_addr = addr;
                sensor.sensor_model    = nv::i2c::SensorModel::Sensor_Tmp461;
            }
            else if (id == nv::i2c::ManufacturerId::Microchip) {
                sensor.identified_addr = addr;
                sensor.sensor_model    = nv::i2c::SensorModel::Sensor_Emc1812;
            }
            else {
                nv::info("No I2C Sensor Found, sID: %d\n", sensor.sensor_id);
                nv::logger::error(nv::logger::Event::T3I2cSensorNotFound, {sensor.sensor_id});
                continue;
            }
        }
        nv::info("Found Sensor: sId %d, iAddr %d, mId %d\n",
                 sensor.sensor_id,
                 sensor.identified_addr,
                 sensor.sensor_model);
        nv::logger::info(nv::logger::Event::T3I2cSensorFound,
                         {sensor.sensor_id, sensor.identified_addr, sensor.sensor_model});
    }
}

int main(int argc, const char* argv[])
{
    adainit();

    ada_print_hello();
    Task::make();
    mctp::Task::make();
    pldm::Task::make();
    flash::Task::make();
    usb::Task::make();
    logger::Task::make();
    // spdm task has a memory violation with flash-test in running unittest
    // spdm::Task::make();

    // Create all queue and events now
    using namespace nv::common::enum_ops;
    for (auto id = ipc::QueueId::Begin; id != ipc::QueueId::End; id++) {
        ipc::Queue::make(id);
    }
    for (auto id = ipc::EventId::Begin; id != ipc::EventId::End; id++) {
        ipc::Event::make(id);
    }

    identify_i2c_temp_sensor();

    auto& super = ipc::Supervisor::inst();
    super.startup(argc, argv);

    common::System::inst().scheduler_start();
    // Unittesting task calls scheduler_stop
    super.shutdown();
    auto ev = common::System::inst().exit_value();
    adafinal();
    return ev != common::ExitValue::Ok ? 1 : 0;
}

extern "C" {
// GBS:BEGIN NO COVERAGE FIXME!!
void vApplicationMallocFailedHook()
{
    fatal("malloc failed\n");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* task_name)
{
    fatal("Stack overflow in task %p:%s\n", task, task_name);
}
// GBS:END NO COVERAGE FIXME!!

void vApplicationTickHook() {}

void vApplicationIdleHook()
{
    std::this_thread::sleep_for(1us);
}

void vApplicationDaemonTaskStartupHook() {}

void vApplicationGetIdleTaskMemory(StaticTask_t** tcb,
                                   StackType_t**  stack_buf,
                                   uint32_t*      stack_size)
{
    static StaticTask_t _tcb{};
    static StackType_t  _stack[configMINIMAL_STACK_SIZE];
    *tcb        = &_tcb;
    *stack_buf  = static_cast<StackType_t*>(_stack);
    *stack_size = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t** tcb,
                                    StackType_t**  stack_buf,
                                    uint32_t*      stack_size)
{
    static StaticTask_t _tcb{};
    static StackType_t  _stack[configTIMER_TASK_STACK_DEPTH];
    *tcb        = &_tcb;
    *stack_buf  = static_cast<StackType_t*>(_stack);
    *stack_size = configTIMER_TASK_STACK_DEPTH;
}
}
