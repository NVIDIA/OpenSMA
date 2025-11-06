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

#include "recovery.h"
#include "sys/i2c/utils.h"

namespace sys::i2c {

nv::i2c::I2cStatus quick_recovery(nv::i2c::Port port, uint8_t address)
{
    const std::span<uint8_t> empty_buffer{};
    return i2c_write_read(port, address, empty_buffer, empty_buffer);
}

nv::i2c::I2cStatus bus_recovery(nv::i2c::Port port)
{
    // Get the LPI2C base address for this port
    LPI2C_Type* base = get_base(port);
    if (base == nullptr) {
        return nv::i2c::I2cStatus::Error;
    }

    // Call the SDK recovery function directly
    status_t result = LPI2C_MasterGenerate9thClock(base);
    return get_status(result);
}

}  // namespace sys::i2c
