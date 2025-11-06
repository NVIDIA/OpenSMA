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
#include <csetjmp>
#include <source_location>
#include <string_view>

#include "nv/ut/config.h"

namespace nv::ut::details {

/// Console output
class Reporter
{
public:
    Reporter() noexcept = default;

    /// To be called at the start of each suite.
    bool on_suite_begin(const std::string_view& suite, std::size_t ntests);

    /// To be called at the end of each suite.
    void on_suite_end(const std::string_view& suite);

    /// To be called at the start of each individual test.
    bool on_test_begin(const std::string_view& file,
                       const std::string_view& suite,
                       const std::string_view& test);

    /// To be called at the end of each individual test.
    bool on_test_end(const std::string_view& suite, const std::string_view& test);

    /// To be called once at the end of all tests.
    int on_complete();

    /// Main callback from asserts/ensures.
    bool operator()(bool fatal, bool result, const std::source_location& loc);

    /// access fatal flag.
    bool is_fatal() const { return fatal; }

    /// Access current configuration.
    const auto& cfg() const { return _cfg; }
    auto&       cfg() { return _cfg; }

    static Reporter inst;  // NOLINT(*-non-const-global-variables)

    friend struct Runner;

private:
    bool         fatal{};
    int          _col{};
    Config       _cfg{};
    std::jmp_buf assert_handler{};
};

}  // namespace nv::ut::details
