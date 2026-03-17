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

#include <cstring>

#include "nv/ut/reporter.h"

namespace nv::ut::details {

Reporter Reporter::inst;  // NOLINT(*-non-const-global-variables)

/**
 *   Cheks if a such test identified by 'suite' and 'test' is going to run
 *   Going to run:
 *     - No command line parameters
 *     - A command line parameter matches:
 *       -- suite
 *       -- suite.test
 *       -- suite::test
 * @param argc
 * @param argv
 * @param suite
 * @param test
 * @returns  true -> going to run, false -> skip
 */
bool is_to_run_test(int                    argc,
                    const char**           argv,
                    const std::string_view suite,
                    const std::string_view test)
{
    if (argc > 1) {
        // loop all command line arguments for the current test
        for (int counter = 1; counter < argc; ++counter) {
            std::string_view argument(argv[counter]);

            // check for <suite><separator><test>
            auto separator_position = argument.find(".");
            int  separator_size     = 1;
            if (separator_position == std::string_view::npos) {
                separator_position = argument.find("::");
                separator_size     = 2;
            }
            // argument matches <suite><separator><test>
            if (separator_position != std::string_view::npos) {
                const auto suite_part = argument.substr(0, separator_position);
                const auto test_part  = argument.substr(separator_position + separator_size);
                if (suite_part == suite && test_part == test) {
                    return true;
                }
            }
            else {  // the argument only contains the suite
                if (argument == suite) {
                    return true;
                }
            }
        }
        // nothing matches current test
        return false;
    }

    // if no command line parameter always return true
    return true;
}

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
                if (!is_to_run_test(argc, argv, test->report.suite, test->report.test)) {
                    continue;
                }
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
