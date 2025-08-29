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
#include "nv/i3c/gpu.h"
#include "nv/gpio/driver.h"
#include "nv/logger/log.h"

using namespace nv::i3c;

Gpu::I2cAddrStatus
Gpu::get_i2c_addr(ipc::Gpios& module_id, ipc::Gpios& strap, Gpu::I2cAddr& addr)
{
    using namespace std::chrono_literals;
    const size_t                             MAX_SIZE = 8;
    const std::array<Gpu::I2cAddr, MAX_SIZE> address_table{
        Gpu::I2cAddr{0x69, 0x4F},
        Gpu::I2cAddr{0x6A, 0x4E},
        Gpu::I2cAddr{0x6B, 0x4D},
        Gpu::I2cAddr{0x6C, 0x4C},
        Gpu::I2cAddr{0x6D, 0x4B},
        Gpu::I2cAddr{0x66, 0x4A},
        Gpu::I2cAddr{0x6F, 0x49},
        Gpu::I2cAddr{0x70, 0x48}
    };
    uint8_t      strap_bit0 = 0;
    uint8_t      strap_bit1 = 0;
    gpio::Status status     = gpio::Driver::read(
        std::get<0>(module_id), std::get<1>(module_id), strap_bit0);
    if (status != gpio::Status::Ok) {
        addr = address_table.at(0);
        return I2cAddrStatus::ReadModulePinFail;
    }
    status = gpio::Driver::read(std::get<0>(strap), std::get<1>(strap), strap_bit1);
    if (status != gpio::Status::Ok) {
        addr = address_table.at(0);
        return I2cAddrStatus::ReadStrapPinFail;
    }
    const uint8_t strap_value = ((strap_bit1 & 0b1) << 1) | ((strap_bit0 & 0b1) << 0);
    if (strap_value > address_table.size()) {
        addr = address_table.at(0);
        return I2cAddrStatus::StrapUnknown;
    }
    addr = address_table.at(strap_value);
    return I2cAddrStatus::Ok;
}
