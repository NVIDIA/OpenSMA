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
#include <cstdint>
#include <span>

#include "nv/common/literals.h"
#include "nv/ipc/event.h"
#include "nv/i2c/common.h"
#include "sys/i3c/driver.h"

namespace nv::i3c {

class Driver : protected sys::i3c::Driver
{
public:
    constexpr static size_t BufferSize = std::max<size_t>(71, sizeof(nv::i2c::I2cRequest));

    static constexpr size_t  IbiDataSize   = 8;
    static constexpr uint8_t TargetAddress = 0x31;
    using I3cBuffer                        = std::array<uint8_t, Driver::BufferSize>;
    using IbiBuffer                        = std::array<uint8_t, Driver::IbiDataSize>;

    enum class Port : uint8_t
    {
        Zero,
        One,
    };

    enum class Status
    {
        Success,
        Busy,
        Nack,
        Error,
        Timeout,
    };

    enum Event : nv::ipc::Event::Bits
    {
        Success = 0_bit,
        Nack    = 1_bit,
        Error   = 2_bit,
    };

    struct Freq
    {
        uint32_t i2c;
        uint32_t i2c_od;
        uint32_t i3c_pp;
    };

    Driver(Port port, Freq freq, bool is_gpu, void* task, nv::ipc::EventId event_id);
    constexpr Port port() const { return _port; }
    void           set_event(Event event);
    void           init();
    void           deinit();
    // CCC
    bool reset_daa(bool ping);
    bool get_status(uint8_t address, uint16_t& status);
    bool process_daa(std::span<uint8_t> address_list);
    bool enec();
    // Write/Read/Interrupt
    bool write(uint8_t address, std::span<uint8_t> data);
    bool read(uint8_t address, std::span<uint8_t> data, uint8_t& length);
    void on_ibi(void* args);
    // I2C APIs
    nv::i2c::I2cStatus
         i2c(uint8_t address, std::span<uint8_t> write_buffer, std::span<uint8_t> read_buffer);
    bool ocp_query_interface_mastering(uint8_t address, bool& enable);
    bool ocp_enable_interface_mastering(uint8_t address);
    bool gpu_query_i3c_mode(uint8_t address, bool& i3c);
    bool gpu_configure_cms1(uint8_t address);
    bool gpu_program_cms1(uint8_t address, std::span<uint8_t> buffer);
    bool gpu_read_cms1(uint8_t address, std::span<uint8_t> buffer);

    uint32_t                  _error_log_count{};
    uint32_t                  _i2c_error_log_count{};
    static constexpr uint32_t ErrorLogThreshold = 100;

protected:
    Port               _port;
    void*              _task = NULL;
    nv::ipc::Event&    _event;
    I3cBuffer          _w_buffer{};
    I3cBuffer          _r_buffer{};
    IbiBuffer          _ibi_data{};
    bool               _is_gpu  = false;
    bool               _is_init = false;
    Status             transfer(void* args, uint8_t& length);
    nv::i2c::I2cStatus i2c_read(uint8_t address, std::span<uint8_t> buffer);
    nv::i2c::I2cStatus i2c_write(uint8_t address, std::span<uint8_t> buffer);
    nv::i2c::I2cStatus i2c_write_read(uint8_t            address,
                                      std::span<uint8_t> write_buffer,
                                      std::span<uint8_t> read_buffer);
    // I2C status to I3C status mapping
    Status to_driver_status(nv::i2c::I2cStatus status);

    // Sdk status to I3C status mapping
    Status to_driver_status(uint32_t status);
};

}  // namespace nv::i3c
