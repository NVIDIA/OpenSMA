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
#include "nv/i2c/powersensor/sensor.h"
#include "sys/i2c/utils.h"
#include <climits>

using namespace nv::i2c;

I2cStatus PowerSensor::send_command(uint8_t offset)
{
    I2cBuffer buffer{offset};
    return sys::i2c::i2c_write(_port, _address, std::span<uint8_t>(buffer.data(), 1));
}

I2cStatus PowerSensor::write_reg(uint8_t offset, uint8_t value)
{
    I2cBuffer buffer{offset, value};

    return sys::i2c::i2c_write(_port, _address, std::span<uint8_t>(buffer.data(), 2));
}

// Write 16 bits register: MSB first
I2cStatus PowerSensor::write_reg_16bits(uint8_t offset, uint16_t value)
{
    I2cBuffer buffer{
        offset, static_cast<uint8_t>(value & 0xFF), static_cast<uint8_t>(value >> CHAR_BIT)};

    return sys::i2c::i2c_write(_port, _address, std::span<uint8_t>(buffer.data(), 3));
}

I2cStatus PowerSensor::read_reg(uint8_t offset, uint8_t& value)
{
    I2cBuffer write_buffer{offset};
    I2cBuffer read_buffer{};

    auto status = sys::i2c::i2c_write_read(_port,
                                           _address,
                                           std::span<uint8_t>(write_buffer.data(), 1),
                                           std::span<uint8_t>(read_buffer.data(), 1));

    if (status == I2cStatus::Ok) {
        value = read_buffer[0];
    }

    return status;
}

// Read 16 bits register: LSB first
I2cStatus PowerSensor::read_reg_16bits(uint8_t offset, uint16_t& value)
{
    I2cBuffer write_buffer{offset};
    I2cBuffer read_buffer{};

    auto status = sys::i2c::i2c_write_read(_port,
                                           _address,
                                           std::span<uint8_t>(write_buffer.data(), 1),
                                           std::span<uint8_t>(read_buffer.data(), 2));

    if (status == I2cStatus::Ok) {
        // coverity[cert_int31_c_violation]
        value = read_buffer[0] | (read_buffer[1] << CHAR_BIT);
    }

    return status;
}

// PMBus/SMBus Block Read
I2cStatus PowerSensor::read_block(uint8_t offset, std::span<uint8_t> value, uint8_t& length)
{
    I2cBuffer write_buffer{offset};
    I2cBuffer read_buffer{};

    // Read up to: 1 (byte_count) + length (max data bytes)
    // Limit to buffer size for safety
    size_t read_size = (length >= I2cBufferSize - 1) ? I2cBufferSize : (length + 1);

    auto status = sys::i2c::i2c_write_read(_port,
                                           _address,
                                           std::span<uint8_t>(write_buffer.data(), 1),
                                           std::span<uint8_t>(read_buffer.data(), read_size));

    if (status != I2cStatus::Ok) {
        return status;
    }

    // Parse block data
    // read_buffer[0] = byte_count (number of data bytes following)
    // read_buffer[1..N] = actual data
    uint8_t byte_count = read_buffer[0];

    // Sanity check: byte_count should not exceed buffer capacity
    if (byte_count > I2cBufferSize - 1) {
        return I2cStatus::Error;
    }

    // Sanity check: byte_count should not exceed caller's buffer
    if (byte_count > value.size() || byte_count > length) {
        return I2cStatus::Error;
    }

    // Copy data bytes (skip the byte_count at read_buffer[0])
    for (uint8_t i = 0; i < byte_count; i++) {
        value[i] = read_buffer[1 + i];
    }

    // Update length to actual number of bytes read
    length = byte_count;

    return I2cStatus::Ok;
}