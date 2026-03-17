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
#include "sys/ctimer/ctimer.h"

// TBD: Implement whole nv::ctimer API GFWLYNT1-168

namespace nv::ctimer {

using NV_Ticks = sys::ctimer::Ticks;

class Driver : protected sys::ctimer::Driver
{
public:
    using sys::ctimer::Driver::read_ticks_inline;

    static void     init();
    static NV_Ticks read_ticks();
    static uint32_t ticks_to_us(NV_Ticks ticks);
    static void     delay_for_us(uint32_t us);
    static NV_Ticks us_to_ticks(uint32_t us);
};

}  // namespace nv::ctimer