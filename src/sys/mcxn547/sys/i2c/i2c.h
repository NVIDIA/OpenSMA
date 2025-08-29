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
#include <fsl_lpi2c.h>

#include "nv/i2c/port.h"

namespace sys::i2c {

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
    void    bind(nv::i2c::Port port, void* task);
    void    init();
    void    start();
    bool    write(std::span<uint8_t> data);
    uint8_t address();
    bool    get_status(uint8_t address);

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

    struct TargetContex
    {
        I2cBuffer buffer;
        void*     task;
        bool      transmit;
    };

    LPI2C_Type*      _base;
    Lpi2cHandle      _lpi2c_handle;
    ControllerContex _controller_context;
    TargetContex     _target_context;

    static LPI2C_Type* get_base(nv::i2c::Port port);
    static void        irq_handler(uint32_t instance, void* handle);
    static void        controller_callback(LPI2C_Type*            base,
                                           lpi2c_master_handle_t* handle,
                                           status_t               completion_status,
                                           void*                  user_data);
    static void
    target_callback(LPI2C_Type* base, lpi2c_slave_transfer_t* transfer, void* user_data);
};

void download_log_in_isr(const uint8_t CurrentSession, I2cBuffer& log_buffer_data);

}  // namespace sys::i2c
