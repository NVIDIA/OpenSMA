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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "sys/i2c/i2c_slave.h"

namespace nv::emulation {

/** @brief Software I2C slave interface (byte-oriented read/write handlers). */
class EmulationDevice
{
public:
    EmulationDevice(const EmulationDevice&)            = delete;
    EmulationDevice& operator=(const EmulationDevice&) = delete;
    EmulationDevice(EmulationDevice&&)                 = delete;
    EmulationDevice& operator=(EmulationDevice&&)      = delete;

    virtual void i2c_write(const std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& i2c_data,
                           uint8_t data_length) = 0;

    virtual void i2c_write(std::span<const uint8_t> i2c_data)
    {
        sys::i2c::I2cSlaveBuffer buffer{};
        const auto               size = std::min(i2c_data.size(), buffer.size());
        std::copy_n(i2c_data.begin(), size, buffer.begin());
        i2c_write(buffer, static_cast<uint8_t>(size));
    }

    virtual void i2c_write_virtual(std::span<const uint8_t> i2c_data) { i2c_write(i2c_data); }

    virtual void i2c_read(uint8_t& out, bool new_transaction) = 0;

    virtual void i2c_read(std::span<uint8_t> i2c_data, size_t& i2c_size, bool new_transaction)
    {
        constexpr uint8_t kDummyReadByte = 0xFF;
        i2c_data[0]                      = kDummyReadByte;
        i2c_read(i2c_data[0], new_transaction);
        i2c_size = 1U;
    }

    virtual void
    i2c_read_virtual(std::span<uint8_t> i2c_data, size_t& i2c_size, bool new_transaction)
    {
        i2c_read(i2c_data, i2c_size, new_transaction);
    }

protected:
    EmulationDevice()  = default;
    ~EmulationDevice() = default;
};

}  // namespace nv::emulation
