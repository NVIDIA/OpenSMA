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

#include "sys/common/fault_define.h"
#include "nv/logger/log_fault.h"
#include <cstdint>
#include <cassert>
#include "fsl_mailbox.h"

using namespace sys::fault;

class c2c_fault
{
public:
    static bool is_sec_vio_from_core1(uint32_t misc_info);
    static bool is_core1_dump_ready(uint32_t core1_fault_buffer_address);
    static void wait_core1_dump_ready(uint32_t core1_fault_buffer_address);
    static void trigger_self_fault();
    static void
    core_1_write_fault_info(uint8_t* fault_buffer, nv::logger::Fault fault, bool ready = false);
    static void dump_core1_fault_info();
    static bool another_core_ready_handle_fault();
    static void notify_another_core_ready();
    static bool is_trigger_by_another_core_fault();
    static void wait_another_core_ready();
    static bool is_under_fault_state();
};
