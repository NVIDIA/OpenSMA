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

#include "c2c_fault.h"
#include "nv/logger/log.h"
#include "nv/ipc/driver.h"
#include "nv/ctimer/ctimer.h"
#include "sys/ipc/supervisor.h"
#include "sys/c2c_mailbox/c2c_mailbox.h"

bool c2c_fault::is_sec_vio_from_core1(uint32_t misc_info)
{
    return (misc_info & SEC_VIO_INFO_MASTER_CORE1_MASK) > 0;
}

bool c2c_fault::is_core1_dump_ready(uint32_t core1_fault_buffer_address)
{
    Core1FaultInfo* core1_fault_info = std::bit_cast<Core1FaultInfo*>(
        core1_fault_buffer_address);
    return (core1_fault_info->ready && core1_fault_info->identifier == Core1FaultIdentifier);
}

void c2c_fault::wait_core1_dump_ready(uint32_t core1_fault_buffer_address)
{
    uint32_t start_tick = nv::ctimer::Driver::read_ticks();
    uint32_t cur_tick   = start_tick;
    while (!is_core1_dump_ready(core1_fault_buffer_address)
           // coverity[cert_int30_c_violation]
           && (cur_tick - start_tick) < AnotherCoreFaultDumpReadyTimeUs) {
        nv::ctimer::Driver::delay_for_us(100);
        cur_tick = nv::ctimer::Driver::read_ticks();
    }
}

void c2c_fault::trigger_self_fault()
{
    volatile uint32_t* addr = std::bit_cast<uint32_t*>(SelfFaultAddr);
    *addr                   = SelfFaultValue;
}

void c2c_fault::core_1_write_fault_info(uint8_t*          fault_buffer,
                                        nv::logger::Fault fault,
                                        bool              ready)
{
    uint32_t        core1_fault_addr = nv::ipc::task::Driver::get_shared_memory_base_address();
    Core1FaultInfo* core1_fault_info = std::bit_cast<Core1FaultInfo*>(core1_fault_addr);

    uint8_t cur_entry_index = core1_fault_info->fault_entry_num;

    if (cur_entry_index >= NumEntryInCore1FaultBuffer) {
        return;
    }

    nv::logger::FaultItem* fault_item = &core1_fault_info->fault_buffer[cur_entry_index];
    fault_item->event                 = fault;
    fault_item->core_id               = 1;
    fault_item->version               = nv::logger::get_fw_version();

    memcpy(fault_item->data.data(), fault_buffer, sizeof(nv::logger::FaultBuffer));

    if (ready) {
        core1_fault_info->identifier = Core1FaultIdentifier;
    }

    core1_fault_info->fault_entry_num += 1;
    core1_fault_info->ready            = ready;
}

void c2c_fault::dump_core1_fault_info()
{
    uint32_t core1_fault_addr = nv::ipc::task::Driver::get_shared_memory_base_address();

    Core1FaultInfo* core1_fault_info = std::bit_cast<Core1FaultInfo*>(core1_fault_addr);

    uint8_t entry_num = core1_fault_info->fault_entry_num;
    for (uint32_t i = 0; i < entry_num; i++) {
        nv::logger::FaultItem* fault_item = &core1_fault_info->fault_buffer[i];
        nv::logger::FaultLogger::fault(
            fault_item->event, fault_item->data, nv::logger::FaultDataSize, 1);
    }
}

void c2c_fault::notify_another_core_ready()
{
    sys::c2c_mailbox::set_value(sys::c2c_mailbox::MailBoxValues::FaultNotifyAnotherCoreReady);
}

bool c2c_fault::another_core_ready_handle_fault()
{
    uint32_t value = sys::c2c_mailbox::get_value();
    return value
        == static_cast<uint32_t>(sys::c2c_mailbox::MailBoxValues::FaultNotifyAnotherCoreReady);
}

void c2c_fault::wait_another_core_ready()
{
    uint32_t start_tick = nv::ctimer::Driver::read_ticks();
    uint32_t cur_tick   = start_tick;

    while (!another_core_ready_handle_fault()
           // coverity[cert_int30_c_violation]
           && (cur_tick - start_tick) < AnotherCoreFaultDumpReadyTimeUs) {
        nv::ctimer::Driver::delay_for_us(100);
        cur_tick = nv::ctimer::Driver::read_ticks();
    }
}

bool c2c_fault::is_trigger_by_another_core_fault()
{
    constexpr uint32_t MMFAR_Address = 0xE000ED34U;
    uint32_t           mmfar_value   = *(uint32_t*)MMFAR_Address;
    return mmfar_value == SelfFaultAddr;
}

bool c2c_fault::is_under_fault_state()
{
    bool               is_in_isr          = sys::ipc::is_in_isr();
    constexpr uint32_t xPSR_FAULT_IRQ_NUM = 8;
    constexpr uint32_t xPSR_IRQ_NUM_MASK  = 0x1FF;
    if (is_in_isr) {
        uint32_t xPSR = __get_xPSR();
        return (xPSR & xPSR_IRQ_NUM_MASK) < xPSR_FAULT_IRQ_NUM;
    }

    return false;
}