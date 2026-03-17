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
#include <span>
#include <stdint.h>

#include "nv/i2c/port.h"
#include "nv/i2c/common.h"

namespace sys::i2c {

class Driver
{
public:
    constexpr static size_t BufferSzie = 76;

    void               bind(nv::i2c::Port port, void* task);
    void               init();
    void               start(bool enable_target);
    bool               write(std::span<uint8_t> data);
    uint8_t            address();
    bool               get_status(uint8_t address);
    static void        set_address(nv::i2c::Port port, uint8_t address) {};
    nv::i2c::I2cStatus i2c_read(uint8_t            address,
                                std::span<uint8_t> buffer,
                                nv::i2c::I2cFlags  flags = nv::i2c::I2cFlags::NoFlag);
    nv::i2c::I2cStatus i2c_write(uint8_t            address,
                                 std::span<uint8_t> buffer,
                                 nv::i2c::I2cFlags  flags = nv::i2c::I2cFlags::NoFlag);
};

}  // namespace sys::i2c
