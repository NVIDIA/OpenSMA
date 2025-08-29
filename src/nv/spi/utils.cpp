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
#include "nv/spi/utils.h"

void nv::spi::u16_to_buf(std::span<uint8_t> buf, uint16_t value, uint8_t start_idx)
{
    if (start_idx + sizeof(uint16_t) > buf.size()) {
        return;
    }
    buf[start_idx]     = (uint8_t)((value >> ByteShift1) & UINT8_MAX);
    buf[start_idx + 1] = (uint8_t)((value)&UINT8_MAX);
}

void nv::spi::u32_to_buf(std::span<uint8_t> buf, uint32_t value, uint8_t start_idx)
{
    if (start_idx + sizeof(uint32_t) > buf.size()) {
        return;
    }
    buf[start_idx]     = (uint8_t)((value >> ByteShift3) & UINT8_MAX);
    buf[start_idx + 1] = (uint8_t)((value >> ByteShift2) & UINT8_MAX);
    buf[start_idx + 2] = (uint8_t)((value >> ByteShift1) & UINT8_MAX);
    buf[start_idx + 3] = (uint8_t)((value)&UINT8_MAX);
}