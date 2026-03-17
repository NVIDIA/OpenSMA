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
#include <array>
#include <stdint.h>
#include <cstddef>

#include "nv/i2c/i2c_types.h"

#include NV_IPC_CONFIG_H

namespace nv::i2c {

constexpr static size_t I2cBufferSize = nv::lstp::EnableI2c ? 512 : 64;

using I2cBuffer = std::array<uint8_t, I2cBufferSize>;

static constexpr size_t I2cHidSmbBufferSize = 512;

using I2cHidBuffer = std::array<uint8_t, I2cHidSmbBufferSize>;

struct [[gnu::packed]] I2cRequest
{
    uint8_t   address;
    uint16_t  write_length;
    I2cBuffer write_buffer;
    uint16_t  read_length;
    uint8_t   src_id;
    I2cFlags  flags;
};

struct [[gnu::packed]] I2cResponse
{
    uint8_t   address;
    uint8_t   read_length;
    I2cBuffer read_buffer;
    I2cStatus status;
};

constexpr std::array<uint8_t, 2> TransparentSkippedEids   = {0x00, 0xFF};
constexpr std::array<uint8_t, 4> TransparentDsIndexCcodes = {0x1f, 0x2f, 0x3f, 0x4f};

}  // namespace nv::i2c
