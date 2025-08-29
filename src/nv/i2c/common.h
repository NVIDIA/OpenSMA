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
#include <array>
#include <stdint.h>
#include <cstddef>

namespace nv::i2c {

static constexpr size_t I2cBufferSize = 64;

using I2cBuffer = std::array<uint8_t, I2cBufferSize>;

static constexpr size_t I2cHidSmbBufferSize = 512;

using I2cHidBuffer = std::array<uint8_t, I2cHidSmbBufferSize>;

enum class I2cStatus : uint8_t
{
    Ok,
    Error,
    Busy,
    Nak,
    Timeout,
    ArbLost
};

enum class I2cQueueError : uint8_t
{
    GetReqFail,
    PutTxFail,
    PutRxFail,
    PutRoutingTableFail,
    GetRoutingTableFail,
    PutWdtNotifyFail
};

enum class I2cPktDrop : uint8_t
{
    Tx,
    Rx,
};

enum class I2cRecovery : uint8_t
{
    Success,
    Fail,
};

struct [[gnu::packed]] I2cRequest
{
    uint8_t   address;
    uint8_t   write_length;
    I2cBuffer write_buffer;
    uint16_t  read_length;
    uint8_t   src_id;
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
