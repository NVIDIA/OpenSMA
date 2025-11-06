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
#include "sys/mcxn236/sys/i2c/utils.h"

namespace sys::i2c {

nv::i2c::I2cStatus quick_recovery(nv::i2c::Port port, uint8_t address)
{
    (void)port;
    (void)address;
    return nv::i2c::I2cStatus::Ok;
}

nv::i2c::I2cStatus bus_recovery(nv::i2c::Port port)
{
    (void)port;
    return nv::i2c::I2cStatus::Ok;
}

}  // namespace sys::i2c
