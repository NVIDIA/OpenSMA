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

#include <array>
#include <source_location>
#include <string_view>

#include "nv/common/debug.h"
#include "nv/common/utils.h"
#include "nv/ut/report.h"
#include "nv/ut/reporter.h"

namespace nv::ut {
namespace details {

/// Structure that unittests can inherit from if they require a fixture.
struct Fixture
{
    Fixture() noexcept = default;
    virtual ~Fixture() = default;
    virtual void setup() {};
    virtual void teardown() {};
    NV_COMMON_COPY_MOVE(Fixture, delete);
};

/// Main Test class.
struct Test
{
    constexpr Test(const std::string_view& file,
                   const std::string_view& id,
                   const std::string_view& suite) noexcept
    : file(file)
    , report{suite, id}
    {}

    virtual ~Test()              = default;
    Test(Test&&) noexcept        = delete;
    Test(const Test&)            = delete;
    Test& operator=(const Test&) = delete;
    Test& operator=(Test&&)      = delete;

    virtual bool operator()() = 0;
    static Test& current() { return *current_test; }

    std::string_view                                 file;
    Report                                           report;
    static inline std::array<Test*, NV_UNITTEST_MAX> tests;                   // NOLINT
    static inline Test*                              current_test = nullptr;  // NOLINT
};

/// Assertions
template<bool Fatal>
struct Assert
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
    template<typename lhs, typename rhs>
    static auto
    is_eq(lhs&& l, rhs&& r, std::source_location loc = std::source_location::current()) noexcept
    {
        return Reporter::inst(Fatal, l == r, loc);
    }
    template<typename lhs, typename rhs>
    static auto
    is_ne(lhs&& l, rhs&& r, std::source_location loc = std::source_location::current()) noexcept
    {
        return Reporter::inst(Fatal, l != r, loc);
    }
    template<typename lhs, typename rhs>
    static auto
    is_gt(lhs&& l, rhs&& r, std::source_location loc = std::source_location::current()) noexcept
    {
        return Reporter::inst(Fatal, l > r, loc);
    }
    template<typename lhs, typename rhs>
    static auto
    is_lt(lhs&& l, rhs&& r, std::source_location loc = std::source_location::current()) noexcept
    {
        return Reporter::inst(Fatal, l < r, loc);
    }
    template<typename lhs, typename rhs>
    static auto
    is_ge(lhs&& l, rhs&& r, std::source_location loc = std::source_location::current()) noexcept
    {
        return Reporter::inst(Fatal, l >= r, loc);
    }
    template<typename lhs, typename rhs>
    static auto
    is_le(lhs&& l, rhs&& r, std::source_location loc = std::source_location::current()) noexcept
    {
        return Reporter::inst(Fatal, l <= r, loc);
    }
#pragma GCC diagnostic pop
    static auto is_true(bool                 result,
                        std::source_location loc = std::source_location::current()) noexcept
    {
        return Reporter::inst(Fatal, !!result, loc);
    }
    static auto is_false(bool                 result,
                         std::source_location loc = std::source_location::current()) noexcept
    {
        return Reporter::inst(Fatal, !result, loc);
    }

    using Func = void (*)();
};

template<typename FixtureType = Fixture>
struct TestImpl : Test
{
    using Func = void (*)(FixtureType&);

    constexpr TestImpl(const std::string_view& file,
                       const std::string_view& id,
                       const std::string_view& suite) noexcept
    : Test(file, id, suite)
    {}

    constexpr TestImpl(const std::string_view& file,
                       const std::string_view& id,
                       const std::string_view& suite,
                       const Func&             f) noexcept
    : Test(file, id, suite)
    , test(f)
    {
        // Automatically register all tests in order
        bool registered = false;
        for (auto& test : tests) {
            if (test == nullptr) {
                test       = this;
                registered = true;
                break;
            }
        }
        NV_ASSERT(registered, "Increase NV_UNITTEST_MAX");
    }
    ~TestImpl() override = default;

    TestImpl(TestImpl&&) noexcept        = delete;
    TestImpl(const TestImpl&)            = delete;
    TestImpl& operator=(const TestImpl&) = delete;
    TestImpl& operator=(TestImpl&&)      = delete;

    bool operator()() override
    {
        current_test = this;
        fixture.setup();
        if (test) {
            test(fixture);
        }
        fixture.teardown();
        return true;
    }

    auto operator<<(const Func& f) noexcept
    {
        return TestImpl{file, report.test, report.suite, f};
    }

    mutable FixtureType fixture;
    const Func          test = nullptr;
};

struct Runner
{
    int operator()(int argc, const char* argv[]) const;  // NOLINT(*-c-arrays)
};

}  // namespace details

// Public interface
[[maybe_unused]] inline static const auto main = details::Runner{};
using Fixture                                  = details::Fixture;
using expect                                   = details::Assert<false>;
using ensure                                   = details::Assert<true>;
static inline const auto& all_tests            = details::Test::tests;

#define TEST(s, id)                                                                            \
    static const auto ut##s##id = nv::ut::details::TestImpl<Fixture>{__FILE__, #id, #s}        \
                               << []([[maybe_unused]] Fixture & fixture)  // NOLINT
#define TEST_F(s, id)                                                                          \
    static const auto ut##s##id = nv::ut::details::TestImpl<s>{__FILE__, #id, #s}              \
                               << []([[maybe_unused]] s & fixture)  // NOLINT

}  // namespace nv::ut
