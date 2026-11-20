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
#include "sys/rtc/rtc.h"

namespace sys::rtc {

void Driver::init() {}

void Driver::init_no_crystal() {}

void Driver::set_clockout([[maybe_unused]] Clockout clockout) {}

void Driver::get_datetime(datetime_t& datetime)
{
    datetime = {};
}

void Driver::set_datetime([[maybe_unused]] const datetime_t& datetime) {}

void Driver::set_alarm([[maybe_unused]] const datetime_t& datetime) {}

void Driver::get_alarm(datetime_t& datetime)
{
    datetime = {};
}

bool Driver::is_alarm_triggered()
{
    return false;
}

void Driver::clear_alarm() {}

bool Driver::por_detected()
{
    return false;
}

void Driver::clear_por() {}

}  // namespace sys::rtc
