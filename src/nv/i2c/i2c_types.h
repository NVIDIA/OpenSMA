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
#include <chrono>
#include <stdint.h>

#include "nv/common/literals.h"

namespace nv::i2c {

uint8_t constexpr SmbusBlockReadLength = 32;

constexpr auto RepeatedStartTimeoutMs = std::chrono::milliseconds(500);

enum class I2cStatus : uint8_t
{
    Ok,
    Error,
    Busy,
    Nak,
    Timeout,
    ArbLost,
    MutexError
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

enum I2cFlags : uint8_t
{
    NoFlag     = 0x00U,
    NoStop     = 0_bit,
    QuickWrite = 1_bit,
    QuickRead  = 2_bit,
    RecvLen    = 3_bit,
    AllFlags   = NoStop | QuickWrite | QuickRead | RecvLen,
};
inline constexpr I2cFlags operator|(I2cFlags a, I2cFlags b)
{
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<I2cFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline constexpr I2cFlags operator&(I2cFlags a, I2cFlags b)
{
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<I2cFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline constexpr I2cFlags operator~(I2cFlags a)
{
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<I2cFlags>(static_cast<uint8_t>(~static_cast<uint8_t>(a) & AllFlags));
}
inline constexpr I2cFlags& operator|=(I2cFlags& a, I2cFlags b)
{
    return a = a | b;
}
inline constexpr I2cFlags& operator&=(I2cFlags& a, I2cFlags b)
{
    return a = a & b;
}

}  // namespace nv::i2c
