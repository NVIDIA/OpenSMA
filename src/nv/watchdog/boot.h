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
#include "sys/watchdog/boot.h"
namespace nv::watchdog {

class Boot : protected sys::watchdog::Boot
{
public:
    static void     init(uint32_t reset_ms);
    static void     disable();
    static void     enable();
    static void     feed();
    static void     write_sticky(uint32_t value);
    static uint32_t read_sticky();
    static bool     is_active();
    static uint32_t read_ticks();
    static void     start_watchdog(uint32_t reset_ms);
    static void     clear_boot_failed(nv::bootloader::Driver::ImageIndex index);
    static bool     check_boot_failed(nv::bootloader::Driver::ImageIndex index);
    static void     try_boot(nv::bootloader::Driver::ImageIndex index);
    static bool     check_boot_success(nv::bootloader::Driver::ImageIndex index);
    static void     mark_switch(nv::bootloader::Driver::ImageIndex index);
    static void     clear_try_times(nv::bootloader::Driver::ImageIndex index);
    static void     update_boot_status();
    static uint32_t get_previous_booted_slot();
    static void     update_booted(nv::bootloader::Driver::ImageIndex index);
    static void     update_wwdt_flag(nv::bootloader::Driver::ImageIndex index, bool is_reset);
    static bool     get_wwdt_flag(nv::bootloader::Driver::ImageIndex index);
    static bool     is_boot_failed_occurred();
    static uint8_t  get_runtime_flag(nv::bootloader::Driver::ImageIndex index);
    static nv::bootloader::Driver::ImageIndex get_target_boot_slot();
    static void set_target_boot_slot(nv::bootloader::Driver::ImageIndex index);

    static bool               able_to_switch();
    static void               increase_total_switch_time();
    static void               reset_total_switch_time();
    static constexpr uint32_t MaxSwitchTime = 6;
#if 0
    static void clear_update();
    static bool is_update(nv::bootloader::Driver::ImageIndex index);
    static void mark_updated(nv::bootloader::Driver::ImageIndex index);
#endif
};

}  // namespace nv::watchdog