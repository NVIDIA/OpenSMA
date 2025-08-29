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
