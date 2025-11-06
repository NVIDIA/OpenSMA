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
#include "nv/common/console.h"
#include "nv/ut/reporter.h"
#include "nv/ut/unittest.h"

using namespace nv::ut;

namespace {

// NOLINTBEGIN
TEST(SelfTest, AllPass)
{
    constexpr auto repeats = 4;
    for (int i = 0; i < repeats; i++) {
        ensure::is_eq(0, 0);
    }
    ensure::is_ne(0, 1);
    ensure::is_gt(1, 0);
    ensure::is_ge(1, 0);
    ensure::is_lt(-1, 0);
    ensure::is_le(-1, 0);
    ensure::is_true(0 == 0);
    // pass
};

TEST(SelfTest, AllWarn)
{
    auto& report = details::Test::current().report;
    auto  warn   = report.warn;
    nv::common::Console::enable(false);  // hide the warning as it's expected
    expect::is_eq(1, 0);                 // warning
    nv::common::Console::enable(true);
    ensure::is_eq(warn + 1, report.warn);
    nv::common::Console::enable(false);  // hide the warning as it's expected
    expect::is_true(0 == 1);             // warning
    ensure::is_eq(warn + 2, report.warn);
    nv::common::Console::enable(true);
    // pass with warning
};

TEST(SelfTest, Errors)
{
    auto& r = nv::ut::details::Reporter::inst;
    // generate an error
    // modify framework to make sure we never see the error or throw
    nv::common::Console::enable(false);  // hide the warning as it's expected
    auto keep_going    = r.cfg().keep_going;
    r.cfg().keep_going = true;
    ensure::is_true(false);
    nv::ut::details::Test::current_test->report.fail--;  // not a fail
    r.cfg().keep_going = keep_going;
    nv::common::Console::enable(true);  // hide the warning as it's expected
};

struct SelftestFixture : public Fixture
{
    void setup() override {}
    void teardown() override {}
};

TEST_F(SelftestFixture, Core)
{
    ensure::is_eq(0, 0);
    ensure::is_ne(0, 1);
    ensure::is_gt(1, 0);
    ensure::is_ge(1, 0);
    ensure::is_lt(-1, 0);
    ensure::is_le(-1, 0);
};
// NOLINTEND

}  // namespace
