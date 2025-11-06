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
#include "nv/ctimer/ctimer.h"

nv::ctimer::NV_Ticks nv::ctimer::Driver::read_ticks()
{
    return 0;
}

sys::ctimer::Ticks
sys::ctimer::Driver::get_counter_difference([[maybe_unused]] Ticks start_count,
                                            [[maybe_unused]] Ticks cur_count)
{
    return 0;
}

void nv::ctimer::Driver::delay_for_us([[maybe_unused]] uint32_t us)
{
    return;
}

void nv::ctimer::Driver::init()
{
    return;
}
