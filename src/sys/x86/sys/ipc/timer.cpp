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
#include "nv/ipc/timer.h"

#include <FreeRTOS.h>
#include <timers.h>

#include "FreeRTOSConfig.h"

#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/nv.h"
#include "sys/ipc/timer.h"

using namespace nv::ipc;
namespace {
constexpr auto OneSecInUsecs = 1'000'000;
}

Timer::Timer(TimerId id, std::chrono::microseconds period, Callback cb, bool reload) noexcept
: sys::ipc::Timer()
, on_timeout(cb)
, _id(id)
{
    // pre conditions
    nv::assert(common::is_in_range(id));

    // add callback wrapper to convert handle to the Timer object
    auto timeout = [](TimerHandle_t handle) {
        // NOLINTNEXT
        auto& timer = *static_cast<Timer*>(pvTimerGetTimerID(handle));
        timer.on_timeout(timer);
    };

    auto& super = Supervisor::inst();
    // coverity[cert_int32_c_violation] - will not overflow
    auto tick_value = configTICK_RATE_HZ * period.count() / OneSecInUsecs;
    // coverity[cert_int31_c_violation] - valid cast
    const auto ticks = static_cast<TickType_t>(tick_value);
    // coverity[cert_exp60_cpp_violation]
    _handle = xTimerCreateStatic("mcu",
                                 ticks,  // coverity[cert_int31_c_violation]
                                 reload,
                                 this,  // coverity[cert_exp60_cpp_violation]
                                 timeout,
                                 &super.static_timers.at(common::to_underlying(
                                     id)));  // coverity[cert_exp60_cpp_violation]

    // post conditions
    nv::always_assert(_handle != nullptr);
    nv::always_assert(on_timeout != nullptr);
}

TimerId Timer::id() const
{
    return _id;
}

Timer::Status Timer::start()
{
    const bool from_isr = sys::ipc::is_in_isr();
    if (from_isr) {
        return xTimerStartFromISR(handle(), 0) == pdPASS ? Status::Ok : Status::Unknown;
    }
    return xTimerStart(handle(), 0) == pdPASS ? Status::Ok : Status::Unknown;
}

Timer::Status Timer::stop()
{
    const bool from_isr = sys::ipc::is_in_isr();
    if (from_isr) {
        return xTimerStopFromISR(handle(), 0) == pdPASS ? Status::Ok : Status::Unknown;
    }
    return xTimerStop(handle(), 0) == pdPASS ? Status::Ok : Status::Unknown;
}

Timer::Status Timer::reset()
{
    const bool from_isr = sys::ipc::is_in_isr();
    if (from_isr) {
        return xTimerResetFromISR(handle(), 0) == pdPASS ? Status::Ok : Status::Unknown;
    }
    return xTimerReset(handle(), 0) == pdPASS ? Status::Ok : Status::Unknown;
}

bool Timer::enabled() const
{
    return xTimerIsTimerActive(handle()) != pdFALSE;
}

std::chrono::microseconds Timer::period() const
{
    auto ticks = xTimerGetPeriod(handle());
    // coverity[cert_int31_c_violation] - valid cast
    const auto tick_duration_us = OneSecInUsecs / configTICK_RATE_HZ;
    // coverity[cert_int30_c_violation] - will not wrap (tick_duration_us * ticks)
    return std::chrono::microseconds{tick_duration_us * ticks};
}

Timer::Status Timer::period(std::chrono::microseconds us)
{
    // coverity[cert_int32_c_violation] - valid cast
    auto ticks = (configTICK_RATE_HZ * us.count()) / OneSecInUsecs;
    // coverity[cert_int31_c_violation] - valid cast
    return xTimerChangePeriod(handle(), ticks, 0) == pdPASS ? Status::Ok : Status::Unknown;
}
