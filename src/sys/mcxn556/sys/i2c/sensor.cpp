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
#include "nv/i2c/sensor.h"
#include "sys/i2c/utils.h"
#include "fsl_lpi2c.h"
#include <climits>

using namespace nv::i2c;

I2cStatus TempSensor::read_reg(uint8_t offset, uint8_t& value)
{
    LPI2C_Type*             base = sys::i2c::get_base(_port);
    lpi2c_master_transfer_t xfer{};
    I2cBuffer               buffer{};
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    status_t status = kStatus_Fail;
    // write phsae
    buffer[0]         = offset;
    xfer.flags        = kLPI2C_TransferNoStopFlag;
    xfer.slaveAddress = _address;
    xfer.direction    = kLPI2C_Write;
    xfer.data         = buffer.data();
    xfer.dataSize     = 1;
    status            = LPI2C_MasterTransferBlocking(base, &xfer);
    if (status != kStatus_Success) {
        return sys::i2c::get_status(status);
    }
    // read phsae
    xfer.flags        = kLPI2C_TransferRepeatedStartFlag;
    xfer.slaveAddress = _address;
    xfer.direction    = kLPI2C_Read;
    xfer.data         = buffer.data();
    xfer.dataSize     = 1;
    status            = LPI2C_MasterTransferBlocking(base, &xfer);
    if (status != kStatus_Success) {
        return sys::i2c::get_status(status);
    }
    value = buffer[0];
    return sys::i2c::get_status(status);
}

// Read 16 bits register: MSB first
I2cStatus TempSensor::read_reg_16bits(uint8_t offset, uint16_t& value)
{
    LPI2C_Type*             base = sys::i2c::get_base(_port);
    lpi2c_master_transfer_t xfer{};
    I2cBuffer               buffer{};
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    status_t status = kStatus_Fail;
    // write phsae
    buffer[0]         = offset;
    xfer.flags        = kLPI2C_TransferNoStopFlag;
    xfer.slaveAddress = _address;
    xfer.direction    = kLPI2C_Write;
    xfer.data         = buffer.data();
    xfer.dataSize     = 1;
    status            = LPI2C_MasterTransferBlocking(base, &xfer);
    if (status != kStatus_Success) {
        return sys::i2c::get_status(status);
    }
    // read phsae
    xfer.flags        = kLPI2C_TransferRepeatedStartFlag;
    xfer.slaveAddress = _address;
    xfer.direction    = kLPI2C_Read;
    xfer.data         = buffer.data();
    xfer.dataSize     = 2;
    status            = LPI2C_MasterTransferBlocking(base, &xfer);
    if (status != kStatus_Success) {
        return sys::i2c::get_status(status);
    }
    // coverity[cert_int31_c_violation]
    value = buffer[0] << CHAR_BIT | buffer[1];
    return sys::i2c::get_status(status);
}

I2cStatus TempSensor::write_reg(uint8_t offset, uint8_t value)
{
    LPI2C_Type*             base = sys::i2c::get_base(_port);
    lpi2c_master_transfer_t xfer{};
    I2cBuffer               buffer{};
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    status_t status = kStatus_Fail;
    // write phsae
    buffer[0]         = offset;
    buffer[1]         = value;
    xfer.flags        = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress = _address;
    xfer.direction    = kLPI2C_Write;
    xfer.data         = buffer.data();
    xfer.dataSize     = 2;
    status            = LPI2C_MasterTransferBlocking(base, &xfer);
    return sys::i2c::get_status(status);
}

// Write 16 bits register: MSB first
I2cStatus TempSensor::write_reg_16bits(uint8_t offset, uint16_t value)
{
    LPI2C_Type*             base = sys::i2c::get_base(_port);
    lpi2c_master_transfer_t xfer{};
    I2cBuffer               buffer{};
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    status_t status = kStatus_Fail;
    // write phsae
    buffer[0]         = offset;
    buffer[1]         = value >> CHAR_BIT;
    buffer[2]         = value & 0xFF;
    xfer.flags        = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress = _address;
    xfer.direction    = kLPI2C_Write;
    xfer.data         = buffer.data();
    xfer.dataSize     = 3;
    status            = LPI2C_MasterTransferBlocking(base, &xfer);
    return sys::i2c::get_status(status);
}

// PMBus/SMBus Block Read
I2cStatus TempSensor::read_block(uint8_t offset, std::span<uint8_t> value, uint8_t& length)
{
    LPI2C_Type*             base = sys::i2c::get_base(_port);
    lpi2c_master_transfer_t xfer{};
    I2cBuffer               buffer{};
    // coverity[UNUSED_VALUE] "tidy" complains no init value
    status_t status = kStatus_Fail;

    // Phase 1: Write command byte (with NO STOP)
    buffer[0]         = offset;
    xfer.flags        = kLPI2C_TransferNoStopFlag;
    xfer.slaveAddress = _address;
    xfer.direction    = kLPI2C_Write;
    xfer.data         = buffer.data();
    xfer.dataSize     = 1;
    status            = LPI2C_MasterTransferBlocking(base, &xfer);
    if (status != kStatus_Success) {
        return sys::i2c::get_status(status);
    }

    // Phase 2: Read block data (with REPEATED START)
    // PMBus Block Read format: [BYTE_COUNT] [DATA0] [DATA1] ... [DATAn-1]
    xfer.flags        = kLPI2C_TransferRepeatedStartFlag;
    xfer.slaveAddress = _address;
    xfer.direction    = kLPI2C_Read;
    xfer.data         = buffer.data();
    // Read up to: 1 (byte_count) + length (max data bytes)
    // Limit to buffer size for safety
    xfer.dataSize = (length >= I2cBufferSize - 1) ? I2cBufferSize : (length + 1);
    status        = LPI2C_MasterTransferBlocking(base, &xfer);
    if (status != kStatus_Success) {
        return sys::i2c::get_status(status);
    }

    // Parse block data
    // buffer[0] = byte_count (number of data bytes following)
    // buffer[1..N] = actual data
    uint8_t byte_count = buffer[0];

    // Sanity check: byte_count should not exceed buffer capacity
    if (byte_count > I2cBufferSize - 1) {
        return I2cStatus::Error;
    }

    // Sanity check: byte_count should not exceed caller's buffer
    if (byte_count > value.size() || byte_count > length) {
        return I2cStatus::Error;
    }

    // Copy data bytes (skip the byte_count at buffer[0])
    for (uint8_t i = 0; i < byte_count; i++) {
        value[i] = buffer[1 + i];
    }

    // Update length to actual number of bytes read
    length = byte_count;

    return sys::i2c::get_status(status);
}