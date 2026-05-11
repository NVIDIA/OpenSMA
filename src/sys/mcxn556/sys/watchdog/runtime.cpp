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
#include "nv/watchdog/runtime.h"
#include "nv/ipc/event.h"
#include "nv/watchdog/boot.h"
using namespace nv::watchdog;

void Runtime::init(uint32_t reset_ms, bool enable_reset)
{
    sys::watchdog::WwdtDriver::init(sys::watchdog::wwdt1, reset_ms, enable_reset);
}

void Runtime::update_timeout(uint32_t reset_ms)
{
    WWDT_Deinit(WWDT1);
    sys::watchdog::WwdtDriver::init(sys::watchdog::wwdt1, reset_ms, false);
}

void Runtime::feed()
{
    sys::watchdog::WwdtDriver::feed(sys::watchdog::wwdt1);
}

void Runtime::record_reset(nv::bootloader::Driver::ImageIndex index)
{
    Boot::update_wwdt_flag(index, true);
}

void Runtime::clear_reset(nv::bootloader::Driver::ImageIndex index)
{
    Boot::update_wwdt_flag(index, false);
}

bool Runtime::is_reset(nv::bootloader::Driver::ImageIndex index)
{
    return Boot::get_wwdt_flag(index);
}
