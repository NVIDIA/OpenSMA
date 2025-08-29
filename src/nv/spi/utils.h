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
#include <span>
#include "nv/spi/common.h"
#include "nv/ctimer/ctimer.h"

namespace nv::spi {
inline uint16_t buf_to_u16(std::span<uint8_t> buf, uint8_t start_idx)
{
    if (start_idx + sizeof(uint16_t) > buf.size()) {
        return 0;
    }
    return (static_cast<uint16_t>(buf[start_idx] << ByteShift1))
         | (static_cast<uint16_t>(buf[start_idx + 1]));
}

inline uint32_t buf_to_u32(std::span<uint8_t> buf, uint8_t start_idx)
{
    if (start_idx + sizeof(uint32_t) > buf.size()) {
        return 0;
    }
    return (static_cast<uint32_t>(buf[start_idx] << ByteShift3))
         | (static_cast<uint32_t>(buf[start_idx + 1] << ByteShift2))
         | (static_cast<uint32_t>(buf[start_idx + 2] << ByteShift1))
         | (static_cast<uint32_t>(buf[start_idx + 3]));
}

void u16_to_buf(std::span<uint8_t> buf, uint16_t value, uint8_t start_idx);

void u32_to_buf(std::span<uint8_t> buf, uint32_t value, uint8_t start_idx);
}  // namespace nv::spi