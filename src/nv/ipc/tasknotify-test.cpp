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
#include "nv/ipc/tasknotify.h"

#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace nv::ut;
using namespace nv::ipc;
using namespace std::chrono_literals;

namespace {
void test_tasknotify()
{
    using Status = TaskNotify::Status;

    constexpr auto id  = TaskId::TestRunner;
    constexpr auto All = 0xFFFFFFFFU;

    // Test send and clear
    ensure::is_eq(TaskNotify::clear(id, All), Status::Ok);
    ensure::is_eq(TaskNotify::send(id, 0b00110011U), Status::Ok);
    ensure::is_eq(TaskNotify::clear(id, 0b00110001U), Status::Ok);
    ensure::is_eq(TaskNotify::clear(id, All), Status::Ok);
}

void test_tasknotify_wait()
{
    using Status = TaskNotify::Status;

    // Unit tests run in the Unittest task, so send/clear/reset target TaskId::Unittest - the
    // same task wait() blocks on (wait() always waits on the calling task).
    constexpr auto id = TaskId::Unittest;

    // Clear all bits first
    ensure::is_eq(TaskNotify::reset(id), Status::Ok);

    // Test wait with timeout (should timeout since no bits are set)
    auto wait_result = TaskNotify::wait(10ms);
    ensure::is_true(!wait_result.has_value());
    ensure::is_eq(wait_result.error(), Status::Timeout);

    // Send some bits and wait (wake on any notification)
    ensure::is_eq(TaskNotify::send(id, 0b00000101U), Status::Ok);
    wait_result = TaskNotify::wait(100ms);
    ensure::is_true(wait_result.has_value());
    ensure::is_eq(wait_result.value() & 0b00000101U, 0b00000101U);

    // Caller clears the bit it handled
    ensure::is_eq(TaskNotify::clear(id, 0b00000001U), Status::Ok);

    // Re-notify so wait returns with remaining bit 2 (send(0) triggers notification)
    ensure::is_eq(TaskNotify::send(id, 0), Status::Ok);
    wait_result = TaskNotify::wait(100ms);
    ensure::is_true(wait_result.has_value());
    ensure::is_eq(wait_result.value() & 0b00000100U, 0b00000100U);

    ensure::is_eq(TaskNotify::clear(id, 0b00000100U), Status::Ok);

    // Now wait should timeout since all bits are cleared
    wait_result = TaskNotify::wait(10ms);
    ensure::is_true(!wait_result.has_value());
    ensure::is_eq(wait_result.error(), Status::Timeout);

    // Send bit 3 only; wait returns with that value
    ensure::is_eq(TaskNotify::send(id, 0b00001000U), Status::Ok);
    wait_result = TaskNotify::wait(10ms);
    ensure::is_true(wait_result.has_value());
    ensure::is_eq(wait_result.value(), 0b00001000U);

    ensure::is_eq(TaskNotify::reset(id), Status::Ok);
}
}  // namespace

TEST(TaskNotify, Core)
{
    test_tasknotify();
};

TEST(TaskNotifyWait, Core)
{
    test_tasknotify_wait();
};
