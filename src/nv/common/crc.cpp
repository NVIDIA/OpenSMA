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
#include "nv/common/crc.h"

uint16_t nv::common::crc16(std::span<const uint8_t>         data,
                           const std::array<uint16_t, 256>& table)
{
    return crc16(0U, data, table);
}

uint16_t nv::common::crc16(uint16_t                         crc,
                           std::span<const uint8_t>         data,
                           const std::array<uint16_t, 256>& table)
{
    for (auto item : data) {
        const auto index = static_cast<uint8_t>((crc >> 8U) ^ item);
        crc              = static_cast<uint16_t>((crc << 8U) ^ table.at(index));
    }
    return crc;
}
