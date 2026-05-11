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
#include <array>
#include <atomic>
#include <span>
#include <fsl_lpi2c.h>

#include "nv/i2c/port.h"
#include "nv/i2c/common.h"
#include "nv/i2c/slave_function.h"

namespace sys::i2c {

using nv::i2c::SlaveFunction;
using nv::i2c::SlaveFunctionEntry;

enum class I2cCustomizeCommand : uint8_t
{
    Begin   = 0x0,
    DumpLog = 0x1,
    End,
};
constexpr size_t BufferSize = 76;
using I2cBuffer             = std::array<uint8_t, BufferSize>;

class Driver
{
public:
    /// Target context used by slave callbacks
    struct TargetContex
    {
        I2cBuffer     buffer;
        void*         task;
        Driver*       owner;
        bool          transmit;
        SlaveFunction function;
    };

    void bind(nv::i2c::Port port, void* task);
    void init();
    void start(bool enable_target);
    bool write(std::span<uint8_t> data);

    void peripheral_recovery(bool enable_target);
    void check_target_timeout(bool enable_target);

    uint8_t address();
    /// Set slave match address on \p port. If \p sec_addr is non-zero, enable dual-address
    /// match (e.g. primary + FRU).
    static void        set_address(nv::i2c::Port port, uint8_t address, uint8_t sec_addr = 0);
    bool               get_status(uint8_t address);
    static IRQn_Type   get_irq(nv::i2c::Port port);
    nv::i2c::I2cStatus i2c_read(uint8_t            address,
                                std::span<uint8_t> buffer,
                                nv::i2c::I2cFlags  flags = nv::i2c::I2cFlags::NoFlag);
    nv::i2c::I2cStatus i2c_write(uint8_t            address,
                                 std::span<uint8_t> buffer,
                                 nv::i2c::I2cFlags  flags = nv::i2c::I2cFlags::NoFlag);

private:
    struct Lpi2cHandle
    {
        lpi2c_master_handle_t controller;
        lpi2c_slave_handle_t  target;
    };

    struct ControllerContex
    {
        I2cBuffer buffer;
        void*     task;
    };

    LPI2C_Type*      _base;
    nv::i2c::Port    _port;
    Lpi2cHandle      _lpi2c_handle;
    ControllerContex _controller_context;
    TargetContex     _target_context;
    uint32_t         _peripheral_clk_hz;

    static LPI2C_Type* get_base(nv::i2c::Port port);
    static void        irq_handler(uint32_t instance, void* handle);
    static void        controller_callback(LPI2C_Type*            base,
                                           lpi2c_master_handle_t* handle,
                                           status_t               completion_status,
                                           void*                  user_data);
    static void
    target_callback(LPI2C_Type* base, lpi2c_slave_transfer_t* transfer, void* user_data);
    static void
    target_callback_lookup(LPI2C_Type* base, lpi2c_slave_transfer_t* transfer, void* user_data);
    static void
    target_callback_eeprom(LPI2C_Type* base, lpi2c_slave_transfer_t* transfer, void* user_data);

    void update_target_state_time_stamp(lpi2c_slave_transfer_event_t event);

    std::atomic<uint32_t> _target_state_time_stamp{};
};

void download_log_in_isr(const uint8_t CurrentSession, I2cBuffer& log_buffer_data);

/// Lookup slave function based on port and address (defined in i2c.cpp with config access)
SlaveFunction get_slave_function(nv::i2c::Port port, uint8_t address);

}  // namespace sys::i2c
