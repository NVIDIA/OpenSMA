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
#include "sys/i2c/i2c.h"

#include "nv/i2c/task.h"
#include "nv/nv.h"

using namespace sys::i2c;

void Driver::bind(nv::i2c::Port port, void* task)
{
    auto i2c_task = static_cast<nv::i2c::Task*>(task);
    nv::info("task %s binds to I2C port %d\n", i2c_task->name().data(), port);
}

void Driver::init()
{
    return;
}

void Driver::start()
{
    return;
}

bool Driver::write([[maybe_unused]] std::span<uint8_t> data)
{
    return true;
}

uint8_t Driver::address()
{
    constexpr uint8_t Kaddress = 0x52;
    return Kaddress;
}

bool Driver::get_status([[maybe_unused]] uint8_t address)
{
    return true;
}

bool Driver::send_i2c_recovery()
{
    return true;
}
