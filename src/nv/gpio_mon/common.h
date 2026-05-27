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
#pragma once

#include <cstddef>
#include <cstdint>

namespace nv::gpio_mon {

// 5 banks (GPIO0..4) is the protocol contract. See SW requirement spec:
// https://nvidia.atlassian.net/wiki/spaces/SSGSystem/pages/2637607020/Software+requirement+and+validation+state
constexpr uint8_t  BankCount    = 5;
constexpr uint32_t ScanPeriodMs = 100;

enum class PacketHeader : uint32_t
{
    InitGpiTriggerMask = 0x80000001,  // Host -> MCU
    FullIoDump         = 0x90000000,  // MCU  -> Host (1 per scan tick)
    Bank0InterruptDump = 0x90000001,  // MCU  -> Host (per GPIO ISR)
    Bank1InterruptDump = 0x90000002,
    Bank2InterruptDump = 0x90000003,
    Bank3InterruptDump = 0x90000004,
    Bank4InterruptDump = 0x90000005,
};

constexpr uint32_t bank_interrupt_header(uint8_t bank)
{
    return static_cast<uint32_t>(PacketHeader::Bank0InterruptDump) + bank;
}

// All records are 32-bit word aligned; sizes in bytes.
// Header (4B) + Bank PDIRs (BankCount * 4B)
constexpr size_t InitMaskCmdSize = 4 + BankCount * 4;
// Header (4B) + Timestamp (4B) + Bank PDIRs (BankCount * 4B)
constexpr size_t FullIoDumpRecordSize = 4 + 4 + BankCount * 4;
// Header (4B) + Timestamp (4B) + Bank PDIR (4B)
constexpr size_t BankInterruptDumpRecordSize = 4 + 4 + 4;

}  // namespace nv::gpio_mon
