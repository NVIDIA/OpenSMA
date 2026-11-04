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

#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

#include "nv/flash/flash.h"

namespace nv::common {

enum TimeUnit
{
    Seconds,
    Milliseconds,
    Microseconds,
    Nanoseconds,
    End
};

enum TimestampAs32bits
{
    LSB  = 0,
    MSB  = 1,
    Size = 2
};

/**
 * @brief Just joins a separated timestamp in 2 parts of 32 bits
 *
 */
class JoinTimestamp
{
public:
    JoinTimestamp(uint32_t left_lsb, uint32_t rigth_msb) : _timestamp{0}
    {
        // default is little endian
        if constexpr (std::endian::native == std::endian::little) {
            _words[TimestampAs32bits::LSB] = left_lsb;
            _words[TimestampAs32bits::MSB] = rigth_msb;
        }
        else {
            _words[TimestampAs32bits::LSB] = rigth_msb;
            _words[TimestampAs32bits::MSB] = left_lsb;
        }
        std::memcpy(&_timestamp, &_words[0], sizeof(uint64_t));
    }
    inline uint64_t timestamp() { return _timestamp; }

private:
    uint32_t _words[TimestampAs32bits::Size];
    uint64_t _timestamp;
};

/**
 * @brief  Gets full timestamp (if global timestamp available in flash)
 *
 *         As the MCU is not able to have a complete timestamp itself,
 *          it uses a global timestamp stored in flash which is saved
 *          in flash when 'Vendor' receives the MCTP message
 *          @sa VdmCmd::AddExtTimestamp.
 *
 * @return global current timestamp 64bit with values in @sa TimeUnit
 *
 * @sa nv::mctp::Vendor::on_add_ext_timestamp()
 */
uint64_t get_timestamp(TimeUnit type = TimeUnit::Milliseconds);

TimeUnit detect_time_unit(uint64_t raw_value);

}  // namespace nv::common
