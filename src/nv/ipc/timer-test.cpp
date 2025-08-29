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

#include "testrunner/config.h"

#include "nv/common/enum_ops.h"
#include "nv/ipc/supervisor.h"
#include "nv/ut/unittest.h"

using namespace nv::ut;
using namespace nv::ipc;
using namespace nv::common::enum_ops;
using namespace std::chrono_literals;

namespace {
volatile TimerId gid = TimerId::Begin;
}

TEST(Timer, Core)
{
    // check for all defined queue ids
    for (auto id = TimerId::Begin; id < TimerId::Test2; id++) {
        auto cb = [](Timer& tim) {
            ensure::is_eq(tim.period(), 20ms);
            tim.reset();
            tim.stop();
            gid = tim.id();
        };
        auto& timer                  = Timer::make(id, 10ms, cb);
        auto& timer_another_instance = Timer::make(id, 10ms, cb);
        ensure::is_eq(&timer, &timer_another_instance);

        ensure::is_false(timer.enabled());
        ensure::is_eq(timer.period(), 10ms);
        ensure::is_eq(timer.period(20ms), Timer::Status::Ok);
    }

    for (auto id = TimerId::Begin; id < TimerId::Test2; id++) {
        auto& timer = Supervisor::inst().timer(id);
        ensure::is_eq(timer.start(), Timer::Status::Ok);
        ensure::is_true(timer.enabled());
    }

    while (gid != TimerId::Test2) {}
};
