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
#include <stdint.h>
#include <span>
#include <fsl_lpi2c.h>
#include "nv/i2c/port.h"
#include "nv/i2c/common.h"
#include "sys/i2c/i2c.h"

namespace sys::i2c {

LPI2C_Type* get_base(nv::i2c::Port port);

nv::i2c::I2cStatus get_status(status_t status);
// I2C operations
nv::i2c::I2cStatus i2c_write(nv::i2c::Port port, uint8_t address, std::span<uint8_t> buffer);
nv::i2c::I2cStatus i2c_read(nv::i2c::Port port, uint8_t address, std::span<uint8_t> buffer);
nv::i2c::I2cStatus i2c_write_read(nv::i2c::Port      port,
                                  uint8_t            address,
                                  std::span<uint8_t> write_buffer,
                                  std::span<uint8_t> read_buffer);
bool               is_master_enabled(nv::i2c::Port port);
bool               is_slave_enabled(nv::i2c::Port port);

}  // namespace sys::i2c
