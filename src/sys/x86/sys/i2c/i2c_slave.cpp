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

#include "sys/i2c/i2c_slave.h"

#include <cstring>

#include "nv/common/debug.h"
#include "nv/common/utils.h"
#include "nv/logger/log.h"
#include "nv/nv.h"

namespace sys::i2c {

I2CSlaveDriver::I2CSlaveDriver([[maybe_unused]] Config& driver_config)
{
    return;
}

I2CSlaveDriver::I2CSlaveDriver()
{
    return;
}

void I2CSlaveDriver::start()
{
    return;
}

void I2CSlaveDriver::callback([[maybe_unused]] void* base,
                              [[maybe_unused]] void* transfer,
                              [[maybe_unused]] void* user_data)
{
    return;
}

}  // namespace sys::i2c
