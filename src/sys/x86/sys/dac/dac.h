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

#include <array>
#include <cstdint>

namespace sys::dac {

class Dac
{
public:
    using Peripheral = uint32_t;

    static constexpr uint32_t ResolutionBits = 12;
    static constexpr uint32_t MaxVoltage_mV  = 3300;

    // 12 bit DAC -> valid range is 0-4095
    // Bits out of range are ignored, e.g. if 0xF010 passed, 0x10 will be
    // written to the DAC
    static void set(uint32_t value, Peripheral peripheral) { peripherals[peripheral] = value; }

    static inline std::array<uint32_t, 2> peripherals;

    Dac() = delete;
};

}  // namespace sys::dac
