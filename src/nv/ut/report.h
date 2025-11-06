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
#include <chrono>
#include <limits>

namespace nv::ut::details {

/// Test report
struct Report
{
    std::string_view          suite;    ///< Suite name.
    std::string_view          test;     ///< Test name.
    uint32_t                  pass{};   ///< Number of passes.
    uint32_t                  warn{};   ///< Number of warnings.
    uint32_t                  fail{};   ///< Number of fails.
    std::chrono::microseconds usecs{};  /// Time taken to run.
    auto                      total() const { return pass + warn + fail; }

    /// Accumulate values of two reports.
    Report& operator+=(const Report& b)
    {
        pass  += b.pass;
        warn  += b.warn;
        fail  += b.fail;
        usecs += b.usecs;
        return *this;
    }

    /// set all values to their minimum
    void min()
    {
        pass = warn = fail = std::numeric_limits<int>::min();
        usecs              = decltype(usecs)::min();
    }

    /// set all values to their maximum
    void max()
    {
        pass = warn = fail = std::numeric_limits<int>::max();
        usecs              = decltype(usecs)::max();
    }

    /// set all values to the minumum of of this and b.
    void min(const Report& b)
    {
        pass  = std::min(pass, b.pass);
        warn  = std::min(warn, b.warn);
        fail  = std::min(fail, b.fail);
        usecs = std::min(usecs, b.usecs);
    }

    /// set all values to the maximum of of this and b.
    void max(const Report& b)
    {
        pass  = std::max(pass, b.pass);
        warn  = std::max(warn, b.warn);
        fail  = std::max(fail, b.fail);
        usecs = std::max(usecs, b.usecs);
    }

    /// set all values to a running average approximation.
    void mean(const Report& b, int count)
    {
        auto m = [](auto& a, auto b, int count) {
            a += (b - a) / (count + 1);
        };
        m(pass, b.pass, count);
        m(fail, b.fail, count);
        m(warn, b.warn, count);
        m(usecs, b.usecs, count);
    }
};

}  // namespace nv::ut::details
