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

// Computes the simple moving average of SoC percentage over 400μs.
// With 100μs sampling period: 400μs / 100μs = 4 samples (power-of-2 for efficiency)
class SocSmaFilterCh : public mpf::FuncBlock
{
public:
    static constexpr uint32_t window_size_log2 = 2;                       // 2^2 = 4 samples
    static constexpr uint32_t window_size      = 1U << window_size_log2;  // 4 samples = 400μs

    struct Ports
    {
        mpf::port::In<SFXP22_10>  soc_percent;
        mpf::port::Out<SFXP22_10> soc_percent_filtered;
    };

    void evaluate(const Ports& ports)
    {
        // Ensure sum cannot overflow (max 100% × 4 samples = 400)
        static_assert(in_s32_range(window_size * 100.0L));

        // Update sliding sum
        _sum            += ports.soc_percent - _buffer[_index];
        _buffer[_index]  = ports.soc_percent;
        _index = (_index + 1) & (window_size - 1);  // Efficient modulo using bitwise AND

        // Calculate average: sum / window_size using right shift (no division!)
        ports.soc_percent_filtered = _sum >> window_size_log2;
    }

private:
    std::array<SFXP22_10, window_size> _buffer{};
    uint32_t                           _index{0};
    SFXP22_10                          _sum{0};
};

}  // namespace nv::soc_pwr_smoothing
