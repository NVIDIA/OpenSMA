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
#include <stdint.h>

namespace nv::vruart {

enum class Status
{
    Ok,
    NotInit,
    TxFail,
};

enum class Instance : uint8_t
{
    Begin,
    _0 = Begin,
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8,
    _9,
    End,
};

enum class EdmaInst : uint8_t
{
    Begin,
    _0 = Begin,
    _1,
    End,
};

enum class EdmaChn : uint8_t
{
    Begin,
    _0 = Begin,
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8,
    _9,
    _10,
    _11,
    _12,
    _13,
    _14,
    _15,
    End,
};

using Baudrate = uint32_t;

typedef struct
{
    uint32_t port;
    uint32_t pin;
} Signal;

}  // namespace nv::vruart
