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
#include "nv/bootloader.h"

#include "nv/nv.h"

// NOLINTBEGIN

using namespace nv::bootloader;

void Driver::run_on_index(ImageIndex image_index) {}

Driver::ImageIndex Driver::current_boot_index()
{
    return Driver::ImageIndex::Image0;
}

void Driver::boot_init()
{
    return;
}

bool Driver::set_image_bootable(ImageIndex index, bool bootable)
{
    return true;
}

bool Driver::set_inactive_bootable(bool bootable)
{
    return true;
}

void Driver::set_task_booted(nv::ipc::BootedEventBits boot_bit) {}

void Driver::on_timer([[maybe_unused]] ipc::Timer& id) {}

uint8_t Driver::get_boot_src()
{
    return 0;
}

uint32_t Driver::get_auth_result()
{
    return 0;
}

void Driver::set_stack_cookie()
{
    return;
}

// NOLINTEND
