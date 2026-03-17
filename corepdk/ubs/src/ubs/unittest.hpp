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
#ifndef UBS_UNITTEST_CPP_COUNT
#error "UBS_UNITTEST_CPP_COUNT is not defined"
#endif

#include <array>
#include <cassert>
#include <cstdio>
#include <source_location>

namespace ubs::unittest {

namespace details {

struct Test
{
    constexpr Test() noexcept = default;
    virtual ~Test()           = default;
    virtual int operator()()  = 0;

    static inline std::array<Test*, UBS_UNITTEST_CPP_COUNT> tests;
};

struct Fixture
{
    Fixture() noexcept                          = default;
    Fixture(Fixture&&) noexcept                 = delete;
    Fixture(const Fixture&) noexcept            = delete;
    Fixture& operator=(const Fixture&) noexcept = delete;
    Fixture& operator=(Fixture&&) noexcept      = delete;
    virtual ~Fixture()                          = default;

    virtual void setup() {};
    virtual void teardown() {};
};

// ada exported
extern "C" int ubs_assertion_check(bool, bool, const char* loc);

/// Assertions
template<bool Fatal>
struct Assert
{
    using sloc = std::source_location;

    static int check(bool fatal, bool cond, const sloc& loc)
    {
        std::array<char, 256> buf;
        snprintf(buf.data(), buf.size(), "%s:%d", loc.file_name(), loc.line());
        return ubs_assertion_check(Fatal, cond, buf.data());
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
    template<typename lhs, typename rhs>
    static auto is_eq(lhs&& l, rhs&& r, sloc loc = sloc::current()) noexcept
    {
        return check(Fatal, l == r, loc);
    }
    template<typename lhs, typename rhs>
    static auto is_ne(lhs&& l, rhs&& r, sloc loc = sloc::current()) noexcept
    {
        return check(Fatal, l != r, loc);
    }
    template<typename lhs, typename rhs>
    static auto is_gt(lhs&& l, rhs&& r, sloc loc = sloc::current()) noexcept
    {
        return check(Fatal, l > r, loc);
    }
    template<typename lhs, typename rhs>
    static auto is_lt(lhs&& l, rhs&& r, sloc loc = sloc::current()) noexcept
    {
        return check(Fatal, l < r, loc);
    }
    template<typename lhs, typename rhs>
    static auto is_ge(lhs&& l, rhs&& r, sloc loc = sloc::current()) noexcept
    {
        return check(Fatal, l >= r, loc);
    }
    template<typename lhs, typename rhs>
    static auto is_le(lhs&& l, rhs&& r, sloc loc = sloc::current()) noexcept
    {
        return check(Fatal, l <= r, loc);
    }
#pragma GCC diagnostic pop
    static auto is_true(bool result, sloc loc = sloc::current()) noexcept
    {
        return check(Fatal, !!result, loc);
    }
    static auto is_false(bool result, sloc loc = sloc::current()) noexcept
    {
        return check(Fatal, !result, loc);
    }
};

template<typename FixtureType = Fixture>
struct TestImpl : Test
{
    using Func = void (*)(FixtureType&);

    constexpr TestImpl() noexcept : Test() {}

    constexpr TestImpl(const Func& f) noexcept : Test(), test(f)
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
        assert(registered && "Increase NV_UNITTEST_MAX");
    }
    ~TestImpl() override = default;

    TestImpl(TestImpl&&) noexcept        = delete;
    TestImpl(const TestImpl&)            = delete;
    TestImpl& operator=(const TestImpl&) = delete;
    TestImpl& operator=(TestImpl&&)      = delete;

    int operator()() override
    {
        fixture.setup();
        if (test) {
            test(fixture);
        }
        fixture.teardown();
        return true;
    }

    auto operator<<(const Func& f) noexcept { return TestImpl{f}; }

    mutable FixtureType fixture;
    const Func          test = nullptr;
};

}  // namespace details

extern "C" {
void          adainit();
void          adafinal();
extern int    gnat_argc;
extern char** gnat_argv;
extern char** gnat_envp;
}

inline int main(int argc, char* argv[], char* envp[])
{
    gnat_argc = argc;
    gnat_argv = argv;
    gnat_envp = envp;

    adainit();  // launches Ada Unittests

    for (auto& ptest : details::Test::tests) {
        if (ptest) {
            auto& test = *ptest;
            test();
        }
    }
    adafinal();
    return 0;
}

// expose
using Fixture = details::Fixture;
using ensure  = details::Assert<true>;
using expect  = details::Assert<false>;
}  // namespace ubs::unittest

#define UBS_TEST(s, id)                                                                        \
    static const auto ubs##s##id = ubs::unittest::details::TestImpl<Fixture>{}                 \
                                << []([[maybe_unused]] Fixture & fixture)  // NOLINT
