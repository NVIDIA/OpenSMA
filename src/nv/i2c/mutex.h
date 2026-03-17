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
#include "nv/i2c/port.h"
#include "nv/ipc/mutex.h"

namespace nv::i2c {

/// Helper to map I2C port to MutexId
constexpr nv::ipc::MutexId port_to_mutex_id(nv::i2c::Port port)
{
    switch (port) {
        case nv::i2c::Port::Zero : return nv::ipc::MutexId::I2cPort0;
        case nv::i2c::Port::One  : return nv::ipc::MutexId::I2cPort1;
        case nv::i2c::Port::Two  : return nv::ipc::MutexId::I2cPort2;
        case nv::i2c::Port::Three: return nv::ipc::MutexId::I2cPort3;
        case nv::i2c::Port::Four : return nv::ipc::MutexId::I2cPort4;
        case nv::i2c::Port::Five : return nv::ipc::MutexId::I2cPort5;
        case nv::i2c::Port::Six  : return nv::ipc::MutexId::I2cPort6;
        case nv::i2c::Port::Seven: return nv::ipc::MutexId::I2cPort7;
        case nv::i2c::Port::Eight: return nv::ipc::MutexId::I2cPort8;
        case nv::i2c::Port::Nine : return nv::ipc::MutexId::I2cPort9;
        default                  : return nv::ipc::MutexId::End;
    }
}

}  // namespace nv::i2c
