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
constexpr uint8_t Primary = 0;
}  // namespace ExtFlashPartitionIndex

struct FlashSpec
{
    uint32_t page_size;      // bytes per page (program granularity)
    uint32_t sector_size;    // bytes per sector (smallest erase unit)
    uint32_t block_size;     // bytes per block (larger erase unit)
    uint32_t max_address;    // highest valid byte address; 0 => disabled
    uint32_t jedec_id;       // 24-bit mfg/type/capacity; verified at every op
    uint32_t op_timeout_ms;  // worst-case timeout for any blocking op
};

namespace ExtFlashSpecs {

constexpr FlashSpec Disabled = {
    .page_size     = 256,
    .sector_size   = 4096,
    .block_size    = 65536,
    .max_address   = 0,
    .jedec_id      = 0,
    .op_timeout_ms = 0,
};

// Macronix MX25S6433F (64Mb / 8MB secure SPI NOR)
constexpr FlashSpec Mx25S6433f = {
    .page_size     = 256,
    .sector_size   = 4096,
    .block_size    = 65536,
    .max_address   = 0x7FFFFF,
    .jedec_id      = 0xC22B27,
    .op_timeout_ms = 150000,  // chip-erase worst case
};

// Macronix MX66U1G45G (1Gb / 128MB octal SPI NOR)
constexpr FlashSpec Mx66U1G45G = {
    .page_size     = 256,
    .sector_size   = 4096,
    .block_size    = 65536,
    .max_address   = 0x07FFFFFF,
    .jedec_id      = 0xC2253B,
    .op_timeout_ms = 300000,  // chip-erase worst case
};

}  // namespace ExtFlashSpecs

}  // namespace nv::spi
