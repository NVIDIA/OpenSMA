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

#include "nv/common/enum_ops.h"
#include "nv/common/literals.h"
#include "nv/common/utils.h"
#include "nv/common/uuid.h"
#include "nv/nv.h"

namespace {

using namespace nv;
using namespace nv::ut;
using namespace nv::common;

// NOLINTBEGIN(*-avoid-magic-numbers)

/// boilerplate for testing common::Expected
template<typename Object, typename T>
void expected_test(T (*get)(const Object&))
{
    enum class ErrorCode
    {
        Ok,
        Error0,
        Error1,
        Error2
    };
    using Ret = Expected<Object, ErrorCode>;

    // Just return an error if i is negative otherwise wrap the value into the object.
    auto func = [](int i) -> Ret {
        if (i < 0) {
            return ErrorCode::Error1;
        }
        return Object{i};
    };

    // test has_value()
    ensure::is_true(func(0).has_value());
    ensure::is_false(func(-1).has_value());
    ensure::is_true(func(1).has_value());
    // test operator!
    ensure::is_false(!!func(-1));
    ensure::is_true(!func(-1));
    ensure::is_true(!!func(1));
    ensure::is_false(!func(1));
    // test value() and operator*
    for (int i = 1; i <= 10; i++) {
        ensure::is_eq(get(func(i).value()), i);
        ensure::is_eq(get(func(i).value()), get(*func(i)));
    }

    // return on error macro, error case
    auto wrap0 = [&]() -> ErrorCode {
        auto val = NV_TRY(func(-1));
        // GBS:BEGIN NO COVERAGE FIXME!!
        ensure::is_true(false);  //  this should never happen
        return get(val) ? ErrorCode::Error0 : ErrorCode::Error2;
        // GBS:END NO COVERAGE FIXME!!
    };
    ensure::is_true(wrap0() == ErrorCode::Error1);

    // return on error, no error case
    auto wrap1 = [&]() -> ErrorCode {
        auto val = NV_TRY(func(10));
        ensure::is_eq(get(val), 10);
        return ErrorCode::Ok;
    };
    ensure::is_true(wrap1() == ErrorCode::Ok);
}

TEST(Common, ExpectedInt)
{
    // test with a simple integer as expected value.
    auto get = [](const int& o) -> int {
        return o;
    };
    expected_test<int, int>(get);
};

TEST(Common, ExpectedStruct)
{
    // test with a POD struct as expected value.
    struct Obj
    {
        int i;
    };
    auto get = [](const Obj& o) -> int {
        return o.i;
    };
    expected_test<Obj, int>(get);
};

TEST(Common, EnumOps)
{
    using namespace enum_ops;

    // Create a Begin/End wrapped Enum class.
    enum class TestEnum : uint8_t
    {
        Begin = 0,
        Zero  = 0,
        One   = 0b01,
        Two   = 0b10,
        Three = 0b11,
        End,
    };

    TestEnum e{};
    ensure::is_true(common::is_in_range(e));
    e = TestEnum::End, ensure::is_false(common::is_in_range(e));
    e = TestEnum::Zero, ensure::is_true(common::is_in_range(e));
    // Test provided post increment/decrement operators
    ensure::is_eq(TestEnum::Zero, e);
    ensure::is_eq(TestEnum::Zero, e++);
    ensure::is_eq(TestEnum::One, e++);
    ensure::is_eq(TestEnum::Two, e++);
    ensure::is_eq(TestEnum::Three, e--);
    ensure::is_eq(TestEnum::Two, e--);
    ensure::is_eq(TestEnum::One, e--);
    ensure::is_eq(TestEnum::Zero, e);
    {
        Console::ScopeDisable _;  // Would normally print out an error
        e--;
        // ensure::is_exception([&] { e--; });  // -- at this point will assert
    }
    ensure::is_false(common::is_in_range(e));

    // Test binary or/and operators
    ensure::is_eq(TestEnum::One | TestEnum::Two, TestEnum::Three);
    ensure::is_eq(TestEnum::Three & TestEnum::Two, TestEnum::Two);
    ensure::is_eq(TestEnum::Three & TestEnum::One, TestEnum::One);
};

TEST(Common, Uuid)
{
    const Uuid uuid{0_u8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    for (uint8_t i = 0; i < 16; i++) {
        ensure::is_eq(i, uuid[i]);
    }
    ensure::is_eq(uuid.size(), 16);
    auto str = uuid.to_string();
    auto sv  = std::string_view(str.begin(), str.end());
    ensure::is_true(sv.compare("00010203-0405-0607-0809-0a0b0c0d0e0f"));
};

TEST(Common, Console)
{
    Console::print(DebugLevel::Debug, "test");
    ensure::is_eq(Console::last(), '\n');

    Console::ScopeDisable _disable;
    int                   arr[2]{1, 2};
    common::debug("test%x", arr);

    auto f = [] {
        return 0;
    };
    common::debug("test%d", NV_LAZY(f()));
};

TEST(Uuid, Basic)
{
    const Uuid uuid{0_u8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    for (uint8_t i = 0; i < 16; i++) {
        ensure::is_eq(i, uuid[i]);
    }

    ensure::is_eq(uuid.size(), 16);
    auto str = uuid.to_string();
    auto sv  = std::string_view(str.begin(), str.end());
    ensure::is_true(sv.compare("00010203-0405-0607-0809-0a0b0c0d0e0f"));
};

}  // namespace

// NOLINTEND(*-avoid-magic-numbers)
