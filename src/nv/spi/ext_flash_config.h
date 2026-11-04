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
#include <cstdint>

namespace nv::spi {

// Named region of the flash. Public read/write/erase APIs take a partition
// index plus an offset relative to the partition base, so callers don't need
// to know absolute flash addresses and can't accidentally cross region
// boundaries.
struct FlashPartition
{
    uint32_t base;
    uint32_t size;
};

namespace ExtFlashPartitionIndex {
constexpr uint8_t Primary  = 0;
constexpr uint8_t Metadata = 1;
}  // namespace ExtFlashPartitionIndex

// expected_capacity is a board sanity-check against the SFDP density readout;
// all other geometry is discovered via SFDP at init().
struct FlashSpec
{
    uint32_t expected_capacity;
    uint32_t op_timeout_ms;  // worst-case timeout for any blocking op
};

namespace ExtFlashSpecs {

constexpr FlashSpec Disabled = {
    .expected_capacity = 0,
    .op_timeout_ms     = 0,
};

}  // namespace ExtFlashSpecs

}  // namespace nv::spi
