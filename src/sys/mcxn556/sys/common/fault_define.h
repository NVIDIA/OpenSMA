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

#include "nv/common/utils.h"
#include "nv/ctimer/ctimer.h"
#include "nv/logger/log_fault.h"
#include "sys/c2c_mailbox/c2c_mailbox.h"
#include <array>
namespace sys::fault {

constexpr uint32_t SEC_VIO_INFO_MASTER_SHIFT      = 8;
constexpr uint32_t SEC_VIO_INFO_MASTER_MASK       = 0x300;  // bit 8-12
constexpr uint32_t SEC_VIO_INFO_MASTER_CORE1_MASK = nv::common::bit(9) | nv::common::bit(10);
constexpr uint32_t Core1FaultBufferSize           = 512;

constexpr uint32_t SelfFaultAddr                   = 0x5A5A5A50;
constexpr uint32_t SelfFaultValue                  = 0xA5A5A5A5;
constexpr uint32_t Core1FaultIdentifier            = 0xAA55AA55;
constexpr uint32_t AnotherCoreFaultDumpReadyTimeUs = 200000;  // 200 ms

constexpr uint8_t NumEntryInCore1FaultBuffer = 8;

using Core1FaultBuffer = std::array<nv::logger::FaultItem, NumEntryInCore1FaultBuffer>;

struct Core1FaultInfo
{
    bool             ready{};
    uint8_t          fault_entry_num{};
    uint32_t         identifier{};
    uint8_t          reserve[2];
    Core1FaultBuffer fault_buffer;
};

}  // namespace sys::fault