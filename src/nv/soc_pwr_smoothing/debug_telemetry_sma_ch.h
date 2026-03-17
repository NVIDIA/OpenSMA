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

#include "nv/common/fixed_point.h"
#include "nv/soc_pwr_smoothing/mpf.h"

#include <array>
#include <cstdint>

namespace nv::soc_pwr_smoothing {

// Computes the simple moving average of a percentage. Assumes that the max
// percentage is 150
class DebugTelemetrySmaCh : public mpf::FuncBlock
{
public:
    static constexpr uint32_t window_size_log2 = 8;
    static constexpr uint32_t window_size      = 1U << window_size_log2;

    struct Ports
    {
        mpf::port::In<SFXP22_10>  percent;
        mpf::port::Out<SFXP22_10> percent_averaged;
    };

    void evaluate(const Ports& ports)
    {
        // Ensure sum cannot overflow
        static_assert(in_s32_range(window_size * 150.0L));

        // Reduce precision of percentage so that it fits in unsigned 8-bit
        // integer (max percentage is 150)
        const SFXP32_0 percent = sfxp22_10_to_sfxp32_0(ports.percent);
        _sum                   = _sum - _buffer[_index] + percent;
        _buffer[_index]        = static_cast<UFXP8_0>(percent);
        _index                 = (_index + 1) & (window_size - 1);
        ports.percent_averaged = sfxp32_0_to_sfxp22_10(_sum) >> window_size_log2;
    }

private:
    std::array<UFXP8_0, window_size> _buffer{};
    uint32_t                         _index{0};
    SFXP32_0                         _sum{0};
};

}  // namespace nv::soc_pwr_smoothing
