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

#include "nv/emulation/pca9544a.h"
#include "nv/nv.h"

namespace nv::emulation {

Pca9544a::Pca9544a() : control_register(BootState)
{
    // done
    nv::info("Finished PCA9544A Initialization\n");
}

void Pca9544a::i2c_read(uint8_t& return_data)
{
    return_data = control_register;
}

void Pca9544a::i2c_write(std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& i2c_data,
                         uint8_t                                                    data_length)
{
    // only care about last byte for writing
    // cn only write to non interrupt bits
    // coverity[cert_int31_c_violation] dont care about lost bits
    control_register = static_cast<uint8_t>(i2c_data.at(data_length - 1U)
                                            & static_cast<uint8_t>(~InterruptMask))
                     | static_cast<uint8_t>(control_register & InterruptMask);
}

uint8_t Pca9544a::get_channel()
{
    return (control_register & ChannelMask);
}

bool Pca9544a::is_enabled()
{
    return ((control_register & EnableMask) != 0);
}

// stores as interrupt low
void Pca9544a::set_interrupt_pin(uint8_t channel, bool value)
{
    if (channel >= 4) {
        nv::error("Invalid channel %d for setting interrupt in PCA9544A\n", channel);
        return;
    }
    else {
        // coverity[cert_int31_c_violation] dont care about lost bits
        const uint8_t Mask = 0x01U << static_cast<uint8_t>(channel + InterruptShift);
        // coverity[cert_int31_c_violation] dont care about lost bits
        const uint8_t Target = static_cast<uint8_t>(!value)
                            << static_cast<uint8_t>(channel + InterruptShift);
        control_register = static_cast<uint8_t>(control_register & static_cast<uint8_t>(~Mask))
                         | static_cast<uint8_t>(Target & Mask);
    }
}

// weird implementation to store interrupt high but output interrupt low
bool Pca9544a::get_interrupt()
{
    return !((control_register & InterruptMask) != 0);
}

}  // namespace nv::emulation
