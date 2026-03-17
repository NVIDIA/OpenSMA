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

#include "sys/i2c/utils.h"
#include "nv/common/utils.h"
#include "nv/i2c/mutex.h"
#include "nv/i2c/port.h"
#include "nv/ipc/mutex.h"
#include "nv/ipc/supervisor.h"

namespace sys::i2c {

LPI2C_Type* get_base(nv::i2c::Port port)
{
    // NOLINTNEXTLINE: SDK definition
    LPI2C_Type*                  ptrs[] = LPI2C_BASE_PTRS;
    const std::span<LPI2C_Type*> bases  = ptrs;
    const size_t                 index  = nv::common::to_underlying(port);
    return index < bases.size() ? bases[index] : nullptr;
}

nv::i2c::I2cStatus get_status(status_t status)
{
    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Error;
    switch (status) {
        case kStatus_Success              : result = nv::i2c::I2cStatus::Ok; break;
        case kStatus_LPI2C_Busy           : result = nv::i2c::I2cStatus::Busy; break;
        case kStatus_LPI2C_Nak            : result = nv::i2c::I2cStatus::Nak; break;
        case kStatus_LPI2C_ArbitrationLost: result = nv::i2c::I2cStatus::ArbLost; break;
        case kStatus_LPI2C_Timeout        : result = nv::i2c::I2cStatus::Timeout; break;
        default                           : result = nv::i2c::I2cStatus::Error;
    }
    return result;
}

nv::i2c::I2cStatus i2c_write(nv::i2c::Port port, uint8_t address, std::span<uint8_t> buffer)
{
    LPI2C_Type* base = get_base(port);
    if (!base) {
        return nv::i2c::I2cStatus::Error;
    }

    lpi2c_master_transfer_t xfer{
        .flags          = kLPI2C_TransferDefaultFlag,
        .slaveAddress   = address,
        .direction      = kLPI2C_Write,
        .subaddress     = 0,
        .subaddressSize = 0,
        .data           = buffer.data(),
        .dataSize       = buffer.size(),
    };

    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(port));
    // Acquire mutex
    auto mutex_status = mutex.lock();
    if (mutex_status != nv::ipc::Mutex::Status::Ok) {
        return nv::i2c::I2cStatus::MutexError;
    }
    const status_t status = LPI2C_MasterTransferBlocking(base, &xfer);
    mutex.unlock();
    return get_status(status);
}

nv::i2c::I2cStatus i2c_read(nv::i2c::Port port, uint8_t address, std::span<uint8_t> buffer)
{
    LPI2C_Type* base = sys::i2c::get_base(port);
    if (!base) {
        return nv::i2c::I2cStatus::Error;
    }

    lpi2c_master_transfer_t xfer{
        .flags          = kLPI2C_TransferDefaultFlag,
        .slaveAddress   = address,
        .direction      = kLPI2C_Read,
        .subaddress     = 0,
        .subaddressSize = 0,
        .data           = buffer.data(),
        .dataSize       = buffer.size(),
    };

    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(port));
    // Acquire mutex
    auto mutex_status = mutex.lock();
    if (mutex_status != nv::ipc::Mutex::Status::Ok) {
        return nv::i2c::I2cStatus::MutexError;
    }
    const status_t status = LPI2C_MasterTransferBlocking(base, &xfer);
    mutex.unlock();
    return get_status(status);
}

nv::i2c::I2cStatus i2c_write_read(nv::i2c::Port      port,
                                  uint8_t            address,
                                  std::span<uint8_t> write_buffer,
                                  std::span<uint8_t> read_buffer)
{
    LPI2C_Type* base = sys::i2c::get_base(port);
    if (!base) {
        return nv::i2c::I2cStatus::Error;
    }

    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(port));
    // Acquire mutex
    auto mutex_status = mutex.lock();
    if (mutex_status != nv::ipc::Mutex::Status::Ok) {
        return nv::i2c::I2cStatus::MutexError;
    }

    // Write phase
    lpi2c_master_transfer_t xfer{
        .flags          = kLPI2C_TransferNoStopFlag,
        .slaveAddress   = address,
        .direction      = kLPI2C_Write,
        .subaddress     = 0,
        .subaddressSize = 0,
        .data           = write_buffer.data(),
        .dataSize       = write_buffer.size(),
    };

    status_t status = LPI2C_MasterTransferBlocking(base, &xfer);
    if (status != kStatus_Success) {
        mutex.unlock();
        return get_status(status);
    }

    // Read phase
    xfer.flags     = kLPI2C_TransferRepeatedStartFlag;
    xfer.direction = kLPI2C_Read;
    xfer.data      = read_buffer.data();
    xfer.dataSize  = read_buffer.size();

    status = LPI2C_MasterTransferBlocking(base, &xfer);
    mutex.unlock();
    return get_status(status);
}

bool is_master_enabled(nv::i2c::Port port)
{
    LPI2C_Type* base = get_base(port);
    if (!base) {
        return false;
    }
    return (base->MCR & LPI2C_MCR_MEN_MASK) != 0;
}

bool is_slave_enabled(nv::i2c::Port port)
{
    LPI2C_Type* base = get_base(port);
    if (!base) {
        return false;
    }
    return (base->SCR & LPI2C_SCR_SEN_MASK) != 0;
}

}  // namespace sys::i2c
