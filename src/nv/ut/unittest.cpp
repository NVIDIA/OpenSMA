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
#include "nv/ut/unittest.h"

#include "nv/ut/reporter.h"

namespace nv::ut::details {

Reporter Reporter::inst;  // NOLINT(*-non-const-global-variables)

int Runner::operator()([[maybe_unused]] int         argc,
                       [[maybe_unused]] const char* argv[]) const  // NOLINT(*-c-arrays)
{
    auto& reporter = Reporter::inst;

    // TODO: pre order tests

    if (!setjmp(Reporter::inst.assert_handler)) {  // NOLINT
        // Run tests
        std::string_view* current_suite = nullptr;
        for (const auto& test : Test::tests) {
            if (test != nullptr) {
                if (current_suite && *current_suite != test->report.suite) {
                    reporter.on_suite_end(*current_suite);
                }
                if (current_suite == nullptr || *current_suite != test->report.suite) {
                    // new suite print out header
                    current_suite = &test->report.suite;
                    reporter.on_suite_begin(*current_suite, current_suite->size());
                }
                reporter.on_test_begin(test->file, test->report.suite, test->report.test);
                (*test)();
                auto fatal = reporter.on_test_end(test->report.suite, test->report.test);
                if (fatal && !Reporter::inst.cfg().keep_going) {
                    break;  // GBS: NO COVERAGE FIXME!!
                }
            }
        }
    }

    return reporter.on_complete();
}

}  // namespace nv::ut::details
