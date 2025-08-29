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
#include "nv/ut/reporter.h"

#include <csetjmp>

#include "nv/common/colors.h"
#include "nv/common/console.h"
#include "nv/nv.h"
#include "nv/ut/unittest.h"

// NOLINTBEGIN(*vararg*)
using namespace nv::ut::details;
using namespace nv::common;

namespace {

inline int check()
{
    Console::print("%s\xE2\x9C\x93%s", Green, Normal);
    return 1;
}

inline int cross(bool fail = false)
{
    if (Console::enabled()) {
        // GBS:BEGIN NO COVERAGE FIXME!!
        Console::print("%s\xE2\x9C\x97%s]\n", (fail ? Red : Orange), Normal);
        // GBS:END NO COVERAGE FIXME!!
    }
    else {
        Console::ScopeEnable en;
        Console::print("%s\xE2\x9C\x97%s", (fail ? Red : Orange), Normal);
    }
    return 1;
}

inline void endl()
{
    Console::print('\n');
}

inline void line()
{
    for (int i = 0; i < Reporter::inst.cfg().width; i++) {
        Console::print('-');
    }
    endl();
}

}  // namespace

bool Reporter::on_suite_begin(const std::string_view& suite, std::size_t ntests)
{
    Console::print("<<%s%s%s>>\n", Green, suite.data(), Normal);
    return true;
}

void Reporter::on_suite_end(const std::string_view& suiteid)
{
    endl();
}

bool Reporter::on_test_begin(const std::string_view& file,
                             const std::string_view& suite,
                             const std::string_view& test)
{
    Console::print(" - %s:%s::%s", file.data(), suite.data(), test.data());
    Console::print(" [");
    _col = 3 + file.length() + 1 + suite.length() + 2 + test.length() + 2 + 1;
    return true;
}

bool Reporter::on_test_end(const std::string_view& suite, const std::string_view& test)
{
    auto r = Test::current_test->report;
    using namespace nv::common;
    Console::print("]\n   %s%d pass %s%d warn %s%d fail%s\n",
                   Green,
                   r.pass,
                   Orange,
                   r.warn,
                   Red,
                   r.fail,
                   Normal);
    return fatal;
}

bool Reporter::operator()(bool isfatal, bool result, const std::source_location& loc)
{
    auto& test = Test::current();
    fatal      = isfatal && !result;
    if (_col >= _cfg.width - 3) {
        Console::print("...\n...");
        _col = 4;
    }
    if (result) {
        test.report.pass++;
        _col += check();
    }
    else {
        cross(isfatal);
        line();
        if (isfatal) {
            test.report.fail++;
            Console::print("%sFAIL%s  %s:%d\n", Red, Normal, loc.file_name(), loc.line());
            if (cfg().keep_going != true) {
                // NOLINTNEXTLINE
                std::longjmp(assert_handler, true);  // GBS: NO COVERAGE FIXME!!
            }
        }
        else {
            test.report.warn++;
            Console::print("%sWARN%s  %s:%d\n", Orange, Normal, loc.file_name(), loc.line());
        }
        line();
        Console::print(" - [");
        _col = 4;
    }
    return result;
}

int Reporter::on_complete()
{
    endl();
    line();

    Report totals{};
    for (const auto& test : Test::tests) {
        if (test != nullptr) {
            totals += test->report;
        }
    }
    Console::print("Totals: %s%d pass %s%d warn %s%d fail%s\n",
                   Green,
                   totals.pass,
                   Orange,
                   totals.warn,
                   Red,
                   totals.fail,
                   Normal);
    line();

    return totals.fail > 0;
}
// NOLINTEND(*vararg*)
