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

#include "sys/i2c/utils.h"

#include <fsl_clock.h>

#include "nv/common/utils.h"
#include "nv/i2c/port.h"

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
        .flags        = kLPI2C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kLPI2C_Write,
        .data         = buffer.data(),
        .dataSize     = buffer.size(),
    };

    const status_t status = LPI2C_MasterTransferBlocking(base, &xfer);
    return get_status(status);
}

nv::i2c::I2cStatus i2c_read(nv::i2c::Port port, uint8_t address, std::span<uint8_t> buffer)
{
    LPI2C_Type* base = sys::i2c::get_base(port);
    if (!base) {
        return nv::i2c::I2cStatus::Error;
    }

    lpi2c_master_transfer_t xfer{
        .flags        = kLPI2C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kLPI2C_Read,
        .data         = buffer.data(),
        .dataSize     = buffer.size(),
    };

    const status_t status = LPI2C_MasterTransferBlocking(base, &xfer);
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

    // Write phase
    lpi2c_master_transfer_t xfer{
        .flags        = kLPI2C_TransferNoStopFlag,
        .slaveAddress = address,
        .direction    = kLPI2C_Write,
        .data         = write_buffer.data(),
        .dataSize     = write_buffer.size(),
    };

    status_t status = LPI2C_MasterTransferBlocking(base, &xfer);
    if (status != kStatus_Success) {
        return get_status(status);
    }

    // Read phase
    xfer.flags     = kLPI2C_TransferRepeatedStartFlag;
    xfer.direction = kLPI2C_Read;
    xfer.data      = read_buffer.data();
    xfer.dataSize  = read_buffer.size();

    status = LPI2C_MasterTransferBlocking(base, &xfer);
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

nv::i2c::I2cStatus set_master_frequency(nv::i2c::Port port, uint32_t baud_hz)
{
    LPI2C_Type* base = get_base(port);
    if (!base || baud_hz == 0U) {
        return nv::i2c::I2cStatus::Error;
    }

    const uint32_t clock_hz = CLOCK_GetLPFlexCommClkFreq(LPI2C_GetInstance(base));
    LPI2C_MasterSetBaudRate(base, clock_hz, baud_hz);

    return nv::i2c::I2cStatus::Ok;
}

// Recover the live baud from the divider registers by reversing
// LPI2C_MasterSetBaudRate, so no cached state is kept (bug 6478775). The result
// is the actual programmed rate, which can differ slightly from the requested
// value because the divider is integer-only (e.g. a 400 kHz request on a 25 MHz
// clock reads back as ~403 kHz).
uint32_t get_master_frequency_hz(nv::i2c::Port port)
{
    LPI2C_Type* base = get_base(port);
    if (!base) {
        return 0U;
    }
    const uint32_t source_hz = CLOCK_GetLPFlexCommClkFreq(LPI2C_GetInstance(base));
    const uint32_t prescale  = (base->MCFGR1 & LPI2C_MCFGR1_PRESCALE_MASK)
                           >> LPI2C_MCFGR1_PRESCALE_SHIFT;
    const uint32_t divider  = 1U << prescale;
    const uint32_t filt_scl = (base->MCFGR2 & LPI2C_MCFGR2_FILTSCL_MASK)
                           >> LPI2C_MCFGR2_FILTSCL_SHIFT;
    const uint32_t clk_hi = (base->MCCR0 & LPI2C_MCCR0_CLKHI_MASK) >> LPI2C_MCCR0_CLKHI_SHIFT;
    const uint32_t clk_lo = (base->MCCR0 & LPI2C_MCCR0_CLKLO_MASK) >> LPI2C_MCCR0_CLKLO_SHIFT;

    // baud = (source / 2^prescale) / (CLKLO + CLKHI + 2 + (2 + FILTSCL) / 2^prescale)
    const uint32_t denom = clk_hi + clk_lo + 2U + (2U + filt_scl) / divider;
    if (denom == 0U) {
        return 0U;
    }
    return (source_hz / divider) / denom;
}
}  // namespace sys::i2c
