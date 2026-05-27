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

#include <cstring>

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
        case kStatus_Success                   : result = nv::i2c::I2cStatus::Ok; break;
        case kStatus_LPI2C_Idle                : result = nv::i2c::I2cStatus::Ok; break;
        case kStatus_LPI2C_Busy                : result = nv::i2c::I2cStatus::Busy; break;
        case kStatus_LPI2C_Nak                 : result = nv::i2c::I2cStatus::Nak; break;
        case kStatus_LPI2C_ArbitrationLost     : result = nv::i2c::I2cStatus::ArbLost; break;
        case kStatus_LPI2C_Timeout             : result = nv::i2c::I2cStatus::Timeout; break;
        case kStatus_LPI2C_FifoError           : result = nv::i2c::I2cStatus::FifoError; break;
        case kStatus_LPI2C_BitError            : result = nv::i2c::I2cStatus::BitError; break;
        case kStatus_LPI2C_PinLowTimeout       : result = nv::i2c::I2cStatus::PinLowTimeout; break;
        case kStatus_LPI2C_DmaRequestFail      : result = nv::i2c::I2cStatus::DmaError; break;
        case kStatus_LPI2C_NoTransferInProgress: result = nv::i2c::I2cStatus::NoTransfer; break;
        default                                : result = nv::i2c::I2cStatus::Error;
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

void slave_config_fill_from_registers(LPI2C_Type*           base,
                                      uint32_t              clk_hz,
                                      lpi2c_slave_config_t& cfg)
{
    std::memset(&cfg, 0, sizeof(cfg));

    const uint32_t samr = base->SAMR;
    /* SAMR stores 7-bit addr shifted: ADDR0 = addr<<1 (see LPI2C_SAMR_ADDR0), ADDR1 = addr<<17.
     */
    cfg.address0 = static_cast<uint8_t>(
        ((samr & LPI2C_SAMR_ADDR0_MASK) >> LPI2C_SAMR_ADDR0_SHIFT) & 0x7FU);
    cfg.address1 = static_cast<uint8_t>(
        ((samr & LPI2C_SAMR_ADDR1_MASK) >> LPI2C_SAMR_ADDR1_SHIFT) & 0x7FU);

    const uint32_t scfgr1 = base->SCFGR1;
    const uint32_t acfg   = (scfgr1 & LPI2C_SCFGR1_ADDRCFG_MASK) >> LPI2C_SCFGR1_ADDRCFG_SHIFT;
    switch (acfg) {
        case 2U: cfg.addressMatchMode = kLPI2C_MatchAddress0OrAddress1; break;
        case 6U: cfg.addressMatchMode = kLPI2C_MatchAddress0ThroughAddress1; break;
        case 0U:
        default: cfg.addressMatchMode = kLPI2C_MatchAddress0; break;
    }
    cfg.ignoreAck                 = (scfgr1 & LPI2C_SCFGR1_IGNACK_MASK) != 0U;
    cfg.enableReceivedAddressRead = (scfgr1 & LPI2C_SCFGR1_RXCFG_MASK) != 0U;
    cfg.enableGeneralCall         = (scfgr1 & LPI2C_SCFGR1_GCEN_MASK) != 0U;
    cfg.sclStall.enableAck        = (scfgr1 & LPI2C_SCFGR1_ACKSTALL_MASK) != 0U;
    cfg.sclStall.enableTx         = (scfgr1 & LPI2C_SCFGR1_TXDSTALL_MASK) != 0U;
    cfg.sclStall.enableRx         = (scfgr1 & LPI2C_SCFGR1_RXSTALL_MASK) != 0U;
    cfg.sclStall.enableAddress    = (scfgr1 & LPI2C_SCFGR1_ADRSTALL_MASK) != 0U;

    const uint32_t scfgr2 = base->SCFGR2;
    if (clk_hz != 0U) {
        const uint64_t clk64     = static_cast<uint64_t>(clk_hz);
        const uint64_t period_ns = (1000000000ULL + clk64 / 2ULL) / clk64;

        const uint32_t filt_sda = (scfgr2 & LPI2C_SCFGR2_FILTSDA_MASK)
                               >> LPI2C_SCFGR2_FILTSDA_SHIFT;
        const uint32_t filt_scl = (scfgr2 & LPI2C_SCFGR2_FILTSCL_MASK)
                               >> LPI2C_SCFGR2_FILTSCL_SHIFT;
        const uint32_t datavd = (scfgr2 & LPI2C_SCFGR2_DATAVD_MASK)
                             >> LPI2C_SCFGR2_DATAVD_SHIFT;
        const uint32_t clkhold = (scfgr2 & LPI2C_SCFGR2_CLKHOLD_MASK)
                              >> LPI2C_SCFGR2_CLKHOLD_SHIFT;

        cfg.sdaGlitchFilterWidth_ns = (filt_sda == 0U)
                                        ? 0U
                                        : static_cast<uint32_t>(
                                              (static_cast<uint64_t>(filt_sda) + 3ULL)
                                              * period_ns);
        cfg.sclGlitchFilterWidth_ns = (filt_scl == 0U)
                                        ? 0U
                                        : static_cast<uint32_t>(
                                              (static_cast<uint64_t>(filt_scl) + 3ULL)
                                              * period_ns);
        cfg.dataValidDelay_ns       = static_cast<uint32_t>(static_cast<uint64_t>(datavd)
                                                      * period_ns);
        cfg.clockHoldTime_ns = static_cast<uint32_t>((static_cast<uint64_t>(clkhold) + 3ULL)
                                                     * period_ns);
    }

    const uint32_t scr    = base->SCR;
    cfg.filterEnable      = (scr & LPI2C_SCR_FILTEN_MASK) != 0U;
    const uint32_t filtdz = (scr & LPI2C_SCR_FILTDZ_MASK) >> LPI2C_SCR_FILTDZ_SHIFT;
    /* SCR FILTDZ written as !filterDozeEnable in LPI2C_SlaveInit. */
    cfg.filterDozeEnable = (filtdz == 0U);
    cfg.enableSlave      = (scr & LPI2C_SCR_SEN_MASK) != 0U;
}

}  // namespace sys::i2c
