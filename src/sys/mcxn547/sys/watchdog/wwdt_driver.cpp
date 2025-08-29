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
#include "sys/watchdog/wwdt_driver.h"
#include "nv/nv.h"
#include "sys/common/utils.h"
#include "nv/logger/log_fault.h"
#include "nv/ipc/supervisor.h"
#include "nv/bootloader.h"
#include "nv/perf_mon/perf_mon.h"
using namespace sys::watchdog;

namespace {
auto get_instance(wwdt_instance instance)
{
    if (instance == wwdt0) {
        return WWDT0;
    }
    return WWDT1;
}
}  // namespace

void LogWdtFault(uint32_t msp_frame, uint32_t lr_value)
{
    nv::logger::FaultBuffer fault_buffer{};
    // const uint32_t          OsTick = nv::ipc::Supervisor::get_os_ticks(true);
    const uint32_t OsTick = 0;
    memcpy(fault_buffer.data(), &OsTick, sizeof(OsTick));
    memcpy(fault_buffer.data() + sizeof(OsTick), &lr_value, sizeof(lr_value));

    const uint32_t CopySize = 8 * sizeof(uint32_t);
    auto           PSP      = std::bit_cast<uint32_t*>(__get_PSP());
    auto           MSP      = std::bit_cast<uint32_t*>(msp_frame);

    if (lr_value == nv::common::to_underlying(nv::logger::ExcReturn::HandlerModeMSP)
        || lr_value == nv::common::to_underlying(nv::logger::ExcReturn::ThreadModeMSP)
        || lr_value == nv::common::to_underlying(nv::logger::ExcReturn::HandlerModeMSPFlt)
        || lr_value == nv::common::to_underlying(nv::logger::ExcReturn::ThreadModeMSPFlt)) {
        // WWDT1_IRQHandler shall not be changed
        constexpr uint32_t StackOffset = 2;
        memcpy(fault_buffer.data() + sizeof(lr_value) + sizeof(OsTick),
               MSP + StackOffset,
               CopySize);
    }
    else if (lr_value == nv::common::to_underlying(nv::logger::ExcReturn::ThreadModePSP)
             || lr_value
                    == nv::common::to_underlying(nv::logger::ExcReturn::ThreadModePSPFlt)) {
        memcpy(fault_buffer.data() + sizeof(lr_value) + sizeof(OsTick), PSP, CopySize);
    }

    nv::logger::FaultLogger::fault(
        nv::logger::Fault::RuntimeWdt, fault_buffer, nv::logger::FaultDataSize);

    auto& event = nv::ipc::Event::make(nv::ipc::EventId::TaskAliveStatus);
    auto  bits  = event.bits(true);
    auto  bit   = bits.value();

    memset(fault_buffer.data(), 0, sizeof(fault_buffer));
    memcpy(fault_buffer.data(), &bit, sizeof(bit));
    memcpy(fault_buffer.data() + sizeof(bit), &PSP, sizeof(PSP));
    memcpy(fault_buffer.data() + sizeof(PSP) + sizeof(bit), &MSP, sizeof(MSP));

    nv::logger::FaultLogger::fault(
        nv::logger::Fault::RuntimeWdtAdditional, fault_buffer, nv::logger::FaultDataSize);

    const auto TaskNum = static_cast<uint32_t>(nv::ipc::TaskId::KernelEnd);

    // one for tick index
    constexpr uint32_t TaskNumPerEntry = (nv::logger::FaultDataSize / sizeof(uint32_t)) - 1;

    auto&              perfmon_driver = nv::perf_mon::Driver::inst();
    auto               index          = perfmon_driver.cur_index;
    constexpr uint32_t OneSecMs       = 1000U;
    uint32_t           rounds         = (nv::ipc::RuntimeWatchdogResetMs / OneSecMs) + 1;
    uint32_t           index_count    = 0;
    while (rounds > 0) {
        index = index == 0 ? perfmon_driver.get_buffer_size() - 1 : index - 1;

        uint32_t remain_task_num    = TaskNum;
        uint32_t current_task_index = 0;
        uint32_t buffer_offset      = 0;

        memset(fault_buffer.data(), 0, sizeof(fault_buffer));

        constexpr uint32_t ByteMask = 0xFF;
        if (index_count < ByteMask) {
            fault_buffer[0] = index_count;
        }
        while (remain_task_num > 0) {
            memcpy(fault_buffer.data() + (buffer_offset + 1) * 4,
                   &(perfmon_driver.buffer.at(index).cpu.at(current_task_index).execution_time),
                   4);
            current_task_index++;
            remain_task_num--;
            buffer_offset++;
            if (buffer_offset >= TaskNumPerEntry || remain_task_num == 0) {
                nv::logger::FaultLogger::fault(
                    nv::logger::Fault::CpuTime, fault_buffer, nv::logger::FaultDataSize);
                buffer_offset = 0;
                memset(fault_buffer.data(), 0, sizeof(fault_buffer));
            }
        }
        rounds--;
        index_count = nv::common::add(index_count, static_cast<uint32_t>(1));
    }

    nv::bootloader::Driver::write_application_fault_record(
        sys::bootloader::Driver::ApplicationFaultMagic, 0, 0, bit);

    WWDT_Deinit(get_instance(wwdt1));
    sys::watchdog::WwdtDriver::init(wwdt1, 1, true);
}

void WwdtDriver::init(wwdt_instance instance, uint32_t reset_ms, bool enable_reset)
{
    auto           wwdt_inst = get_instance(instance);
    wwdt_config_t  config{};
    const uint32_t ClockFreq     = CLOCK_GetWdtClkFreq(instance == wwdt0 ? 0 : 1);
    const uint32_t WwdtTimerFreq = ClockFreq / WwdtPrescale;
    WWDT_GetDefaultConfig(&config);
    config.enableWatchdogReset = enable_reset;
    config.clockFreq_Hz        = ClockFreq;

    const uint32_t TargetTimeout = nv::common::mul(((WwdtTimerFreq) / (OneSecMs)), reset_ms);
    config.timeoutValue          = TargetTimeout;
#if 0
    nv::info("ClockFreq %d\n", ClockFreq);
    nv::info("TargetTimeout %d\n", TargetTimeout);
    nv::info("Inst 0x%x\n", wwdt_inst);
#endif
    WWDT_Init(wwdt_inst, &config);

    if (!config.enableWatchdogReset) {
        if (instance == wwdt0) {
            NVIC_EnableIRQ(WWDT0_IRQn);
            NVIC_SetPriority(WWDT0_IRQn, 0);
        }
        else {
            NVIC_EnableIRQ(WWDT1_IRQn);
            NVIC_SetPriority(WWDT1_IRQn, 0);
        }
    }
}

void WwdtDriver::feed(wwdt_instance instance)
{
    auto wwdt_inst = get_instance(instance);
    WWDT_Refresh(wwdt_inst);
}

void WwdtDriver::trigger_wdt_reset_if_enabled(wwdt_instance instance)
{
    if (is_enabled(instance)) {
        auto wwdt_inst = get_instance(instance);
        WWDT_Deinit(wwdt_inst);
        init(wwdt1, 1, true);
    }
}

bool WwdtDriver::is_enabled(wwdt_instance instance)
{
    auto wwdt_inst = get_instance(instance);
    return (wwdt_inst->MOD & WdenMask) > 0;
}

extern "C" {
void WWDT1_IRQHandler(void)
{
    auto stack_frame = (uint32_t)__get_MSP();

    uint32_t lr_value = 0;
    lr_value          = std::bit_cast<uint32_t>(__builtin_return_address(0));
    LogWdtFault(stack_frame, lr_value);

    // coverity[no_escape] suppress warning for while(1) loop
    while (true) {};
}
}