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
#include "nv/i3c/driver.h"

#include "nv/nv.h"

nv::i3c::Driver::Driver([[maybe_unused]] Port             port,
                        [[maybe_unused]] Freq             freq,
                        [[maybe_unused]] bool             is_gpu,
                        [[maybe_unused]] void*            task,
                        [[maybe_unused]] nv::ipc::EventId event_id,
                        [[maybe_unused]] uint32_t         clock)
: _port(port)
, _event(nv::ipc::Event::make(event_id))
{
    // TODO implement
}

void nv::i3c::Driver::init()
{
    // TODO implement
}

void nv::i3c::Driver::deinit()
{
    // TODO implement
}

bool nv::i3c::Driver::reset_daa([[maybe_unused]] bool ping)
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::process_daa([[maybe_unused]] std::span<uint8_t> address_list)
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::get_status([[maybe_unused]] uint8_t   address,
                                 [[maybe_unused]] uint16_t& status)
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::enec()
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::write([[maybe_unused]] uint8_t            address,
                            [[maybe_unused]] std::span<uint8_t> data)
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::read([[maybe_unused]] uint8_t            address,
                           [[maybe_unused]] std::span<uint8_t> data,
                           [[maybe_unused]] uint8_t&           length)
{
    // TODO implement
    return true;
}

void nv::i3c::Driver::on_ibi([[maybe_unused]] void* args)
{
    // TODO implement
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c([[maybe_unused]] uint8_t            address,
                                        [[maybe_unused]] std::span<uint8_t> write_buffer,
                                        [[maybe_unused]] std::span<uint8_t> read_buffer)
{
    // TODO implement
    return nv::i2c::I2cStatus::Ok;
}

bool nv::i3c::Driver::ocp_query_interface_mastering([[maybe_unused]] uint8_t address,
                                                    [[maybe_unused]] bool&   enable)
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::ocp_enable_interface_mastering([[maybe_unused]] uint8_t address)
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::gpu_query_i3c_mode([[maybe_unused]] uint8_t address,
                                         [[maybe_unused]] bool&   i3c)
{
    // TODO implement
    return true;
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c_write([[maybe_unused]] uint8_t            address,
                                              [[maybe_unused]] std::span<uint8_t> buffer)
{
    // TODO implement
    return nv::i2c::I2cStatus::Ok;
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c_read([[maybe_unused]] uint8_t            address,
                                             [[maybe_unused]] std::span<uint8_t> buffer)
{
    // TODO implement
    return nv::i2c::I2cStatus::Ok;
}

nv::i2c::I2cStatus
nv::i3c::Driver::i2c_write_read([[maybe_unused]] uint8_t            address,
                                [[maybe_unused]] std::span<uint8_t> write_buffer,
                                [[maybe_unused]] std::span<uint8_t> read_buffer)
{
    // TODO implement
    return nv::i2c::I2cStatus::Ok;
}

bool nv::i3c::Driver::gpu_configure_cms1([[maybe_unused]] uint8_t address)
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::gpu_program_cms1([[maybe_unused]] uint8_t            address,
                                       [[maybe_unused]] std::span<uint8_t> buffer)
{
    // TODO implement
    return true;
}

bool nv::i3c::Driver::gpu_read_cms1([[maybe_unused]] uint8_t            address,
                                    [[maybe_unused]] std::span<uint8_t> buffer)
{
    // TODO implement
    return true;
}