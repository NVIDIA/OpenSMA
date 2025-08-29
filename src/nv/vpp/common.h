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

#include <cstdint>

namespace nv::vpp {

// VPP Register Mapping
// applied to each bank of e.1s io expander (pca9555 emulator)
constexpr static uint8_t VppFaultedBit     = 0U;
constexpr static uint8_t VppFaultedBitMask = 1U << VppFaultedBit;

constexpr static uint8_t VppLocateLedBit     = 1U;
constexpr static uint8_t VppLocateLedBitmask = 1U << VppLocateLedBit;

constexpr static uint8_t VppPowerControlBit     = 2U;                        // unused
constexpr static uint8_t VppPowerControlBitmask = 1U << VppPowerControlBit;  // unused

constexpr static uint8_t VppSlotPowerOnBit     = 3U;                       // unused
constexpr static uint8_t VppSlotPowerOnBitmask = 1U << VppSlotPowerOnBit;  // unused

constexpr static uint8_t VppPrsntLBit     = 4U;
constexpr static uint8_t VppPrsntLBitmask = 1U << VppPrsntLBit;

constexpr static uint8_t VppSlotPwrLossBit     = 5U;                       // unused
constexpr static uint8_t VppSlotPwrLossBitmask = 1U << VppSlotPwrLossBit;  // unused

constexpr static uint8_t VppSlotPwrManageBit     = 6U;                         // unused
constexpr static uint8_t VppSlotPwrManageBitmask = 1U << VppSlotPwrManageBit;  // unused

constexpr static uint8_t VppPFABit     = 7U;               // unused
constexpr static uint8_t VppPFABitmask = 1U << VppPFABit;  // unused

}  // namespace nv::vpp