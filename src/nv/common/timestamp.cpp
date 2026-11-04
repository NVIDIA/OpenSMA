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

#include "timestamp.h"

#include <array>
#include <cstring>

#include "nv/common/system.h"
#include "nv/ipc/supervisor.h"

namespace nv::common {

// Thresholds valid from year 2001 up to approx. year 2286
constexpr auto SecondsThreshold      = 10000000000ULL;
constexpr auto MillisecondsThreshold = 10000000000000ULL;
constexpr auto MicrosecondsThreshold = 10000000000000000ULL;
constexpr auto NanosecondsThreshold  = 18446744073709551615ULL;

// Lookup table for powers of 1000 matching the enum indices
constexpr auto SecondsPower100      = 1ULL;
constexpr auto MillisecondsPower100 = 1000ULL;
constexpr auto MicrosecondsPower100 = 1000000ULL;
constexpr auto NanosecondsPower100  = 1000000000ULL;

static constexpr std::array<uint64_t, TimeUnit::End> pwr_factors = []() {
    std::array<uint64_t, TimeUnit::End> temp{};
    temp[Seconds]      = SecondsPower100;
    temp[Milliseconds] = MillisecondsPower100;
    temp[Microseconds] = MicrosecondsPower100;
    temp[Nanoseconds]  = NanosecondsPower100;
    return temp;
}();

/**
 * @brief RoundOsTicks used to accurate over get_os_ticks()
 *
 *  get_os_ticks() is not accurated,
 *     reasons: "Integer Math Loss" and "Interrupt Latency Drift".
 *
 *  Integer Math Loss:
 *    Converting an OS tick count into milliseconds requires multiplying by a scale
 *     factor (e.g., if 1 tick = 1 ms, multiplying ticks by 1,000,000).
 *    This means your global timestamp will only ever update in massive, jagged jumps
 *     of milliseconds at a time.
 *  Interrupt Latency Drift:
 *    If an interrupt is blocked for even a fraction of a millisecond by another task,
 *     your tick count delays. Over hours of uptime, your global timestamp will not match the
 *     actual MCTP time source.
 */
static constexpr float RoundOsTicks = 0.9957005;

/**
 * @brief Detect the Time Unit from a timestamp 64 bits value
 * @param raw_value - timestamp 64 bits
 */
TimeUnit detect_time_unit(uint64_t timestamp)
{
    if (timestamp == 0) {
        return End;
    }
    if (timestamp < SecondsThreshold) {
        return Seconds;  // e.g., ~1.7 Billion (current era in seconds)
    }
    if (timestamp < MillisecondsThreshold) {
        return Milliseconds;  // e.g., ~1.7 Trillion
    }
    if (timestamp < MicrosecondsThreshold) {
        return Microseconds;  // e.g., ~1.7 Quadrillion
    }
    if (timestamp <= NanosecondsThreshold) {
        return Nanoseconds;  // e.g., ~1.7 Quintillion
    }
    return End;
}

/**
 * @brief Convert if necessary a timestamp 64 bits from a TimeUnit value to another TimeUnit
 * @param timestamp  - timestamp 64 bits
 * @param from_time_unit - the TimeUnit of the @a timestamp
 * @param to_time_unit  - desired TimeUnit to convert @val
 * @return @a val converted to @a to TimeUnit
 */
uint64_t
convert_timestamp_unit(uint64_t timestamp, TimeUnit from_time_unit, TimeUnit to_time_unit)
{
    if (from_time_unit == to_time_unit || from_time_unit >= TimeUnit::End
        || to_time_unit >= TimeUnit::End) {
        return timestamp;
    }
    if (from_time_unit < to_time_unit) {
        // Upscaling (e.g., Seconds to Milliseconds): Multiply
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,-warnings-as-errors)
        const uint64_t multiplier = pwr_factors[to_time_unit] / pwr_factors[from_time_unit];
        return timestamp * multiplier;
    }
    // Downscaling (e.g., Nanoseconds to Seconds): Divide
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,-warnings-as-errors)
    const uint64_t divider = pwr_factors[from_time_unit] / pwr_factors[to_time_unit];
    return timestamp / divider;
}

uint32_t os_ticks_to_milliseconds(uint32_t ticks)
{
    return ticks * nv::common::System::inst().tick_time_ms();
}

uint64_t priv_timestamp(nv::flash::Data dataLSB,
                        nv::flash::Data dataMSB,
                        nv::flash::Data saved_ticks,
                        nv::flash::Data cur_ticks,
                        TimeUnit        return_unit)
{
    uint64_t timestamp          = JoinTimestamp(dataLSB, dataMSB).timestamp();
    auto     cur_timestamp_unit = detect_time_unit(timestamp);
    timestamp = convert_timestamp_unit(timestamp, cur_timestamp_unit, return_unit);
    // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions)
    auto rounded_cur_ticks   = static_cast<const nv::flash::Data>(RoundOsTicks * cur_ticks);
    const uint64_t ms_ticks  = (rounded_cur_ticks > saved_ticks)
                                 ? os_ticks_to_milliseconds(rounded_cur_ticks - saved_ticks)
                                 : os_ticks_to_milliseconds(cur_ticks - saved_ticks);
    timestamp               += convert_timestamp_unit(ms_ticks, Milliseconds, return_unit);
    return timestamp;
}

uint64_t get_timestamp(TimeUnit unit)
{
    auto            cur_ticks = sys::ipc::get_os_ticks();
    uint64_t        timestamp{0};
    nv::flash::Data dataTICKS{0};
    nv::flash::Data dataLSB{0};
    nv::flash::Data dataMSB{0};
    using Flash = nv::flash::Flash;
    auto DataOK = nv::flash::Status::Ok;

    // get global timestamp stored, if exists
    if (Flash::get_data(nv::flash::Key::NpdsExtTimestampLSB, dataLSB) == DataOK
        && Flash::get_data(nv::flash::Key::NpdsExtTimestampMSB, dataMSB) == DataOK
        && Flash::get_data(nv::flash::Key::NpdsExtTimestampTick, dataTICKS) == DataOK) {
        timestamp = priv_timestamp(dataLSB, dataMSB, dataTICKS, cur_ticks, unit);
    }
    else {
        // only timestamp from MCU boot, keep as milliseconds
        timestamp = os_ticks_to_milliseconds(cur_ticks);
    }
    return timestamp;
}

}  // namespace nv::common
