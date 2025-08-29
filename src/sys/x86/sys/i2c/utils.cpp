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
#include "nv/common/utils.h"
#include "nv/i2c/port.h"

namespace sys::i2c {
nv::i2c::I2cStatus i2c_write([[maybe_unused]] nv::i2c::Port      port,
                             [[maybe_unused]] uint8_t            address,
                             [[maybe_unused]] std::span<uint8_t> buffer)
{
    return nv::i2c::I2cStatus::Ok;
}

nv::i2c::I2cStatus i2c_read([[maybe_unused]] nv::i2c::Port      port,
                            [[maybe_unused]] uint8_t            address,
                            [[maybe_unused]] std::span<uint8_t> buffer)
{
    return nv::i2c::I2cStatus::Ok;
}

nv::i2c::I2cStatus i2c_write_read([[maybe_unused]] nv::i2c::Port      port,
                                  [[maybe_unused]] uint8_t            address,
                                  [[maybe_unused]] std::span<uint8_t> write_buffer,
                                  [[maybe_unused]] std::span<uint8_t> read_buffer)
{
    return nv::i2c::I2cStatus::Ok;
}

bool is_master_enabled([[maybe_unused]] nv::i2c::Port port)
{
    return true;
}

bool is_slave_enabled([[maybe_unused]] nv::i2c::Port port)
{
    return false;
}

}  // namespace sys::i2c
