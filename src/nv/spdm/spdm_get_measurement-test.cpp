/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/spdm/spdm_get_measurement.h"

#include <algorithm>
#include <array>
#include <span>

#include "nv/ut/unittest.h"

using namespace nv::spdm::measurement;
using namespace nv::ut;

namespace {

constexpr uint8_t Untouched = 0xA5;

// True when the measurement is exactly `expected` and nothing after it was written.
bool reports(MeasNumT index, std::span<const uint8_t> expected)
{
    std::array<uint8_t, 64> buffer{};
    buffer.fill(Untouched);
    spdm_get_measurement(index, buffer.data());

    const std::span<const uint8_t> written(buffer.data(), expected.size());
    const std::span<const uint8_t> rest(buffer.data() + expected.size(),
                                        buffer.size() - expected.size());
    return MeasInfos.at(index).meas_size == expected.size()
        && std::ranges::equal(written, expected)
        && std::ranges::all_of(rest, [](uint8_t byte) { return byte == Untouched; });
}

// True when the measurement is `size` bytes of `value`.
bool reports(MeasNumT index, size_t size, uint8_t value)
{
    std::array<uint8_t, 48> expected{};
    expected.fill(value);
    return reports(index, std::span<const uint8_t>(expected).first(size));
}

static_assert(MeasReservedIndex2 == 2 && MeasReservedIndex45 == 45 && MeasMax == 53,
              "measurement indexes must not move");

}  // namespace

// Attestation verifiers compare these values with reference values, so they must not change.
TEST(SpdmMeasurement, ReservedIndexesKeepTheirValues)
{
    constexpr std::array<uint8_t, 4> NoType{'N', 'O', 'N', 0x00};
    ensure::is_true(reports(MeasReservedIndex2, NoType));

    for (const auto index :
         {MeasReservedIndex9, MeasReservedIndex10, MeasReservedIndex31, MeasReservedIndex32}) {
        ensure::is_true(reports(index, 48, 0x00));
    }
    for (const auto index : {MeasReservedIndex21, MeasReservedIndex22}) {
        ensure::is_true(reports(index, 8, 0xff));
    }
    for (const auto index : {MeasReservedIndex27, MeasReservedIndex28}) {
        ensure::is_true(reports(index, 16, 0xff));
    }
    for (const auto index : {MeasReservedIndex35, MeasReservedIndex36}) {
        ensure::is_true(reports(index, 4, 0xff));
    }
    for (const auto index : {MeasReservedIndex41, MeasReservedIndex42}) {
        ensure::is_true(reports(index, 4, 0x00));
    }
    for (const auto index : {MeasReservedIndex44, MeasReservedIndex45}) {
        ensure::is_true(reports(index, 1, 0x00));
    }
};
